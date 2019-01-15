#include <string.h>

#include <curl/curl.h>

#define OBXC_USE_OBX_ALIASES
#include "error_manager.h"
#include "http_utils.h"

//----------------------------------------------
// Utilities
//----------------------------------------------

// create a new curl handle and allocate memory for the result
obx_err init_curl(CURL** handle, Memory** mem) {
    *handle = curl_easy_init();
    if (*handle == NULL) {
        return obx_set_last_error_code(OBX_ERROR_CURL_INIT_FAILED);
    }

    curl_easy_setopt(*handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    // curl_easy_setopt(*handle, CURLOPT_VERBOSE, 1L);

    *mem = (Memory*) malloc(sizeof(Memory));
    if (*mem == NULL) return obx_set_last_error_code(OBX_ERROR_ALLOCATION);
    (*mem)->size = 0;
    (*mem)->buf = (char*) malloc(1);
    if ((*mem)->buf == NULL) return obx_set_last_error_code(OBX_ERROR_ALLOCATION);
    curl_easy_setopt(*handle, CURLOPT_WRITEFUNCTION, memory_grow);
    curl_easy_setopt(*handle, CURLOPT_WRITEDATA, *mem);

    // for completeness:
    // curl_easy_setopt(*handle, CURLOPT_ENCODING, "gzip, deflate");
    curl_easy_setopt(*handle, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(*handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(*handle, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(*handle, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(*handle, CURLOPT_EXPECT_100_TIMEOUT_MS, 0L);

    return obx_set_last_error_code(OBX_SUCCESS);
}

// result needs to be freed with curl_free after use
char* url_encode(HttpApi* api, const char* data, size_t len) { return curl_easy_escape(api->url_encoder, data, len); }

//----------------------------------------------
// Memory
//----------------------------------------------

size_t memory_grow(void* contents, size_t sz, size_t nmemb, void* ctx) {
    size_t realsize = sz * nmemb;
    Memory* mem = (Memory*) ctx;
    char* ptr = (char*) realloc(mem->buf, mem->size + realsize);
    if (!ptr) {
        obx_set_last_error_code(OBX_ERROR_ALLOCATION);
        return 0;
    }
    mem->buf = ptr;
    memcpy(&(mem->buf[mem->size]), contents, realsize);
    mem->size += realsize;
    obx_set_last_error_code(OBX_SUCCESS);
    return realsize;
}

void memory_free(Memory* mem) {
    if (mem) {
        if (mem->buf) free(mem->buf);
        free(mem);
    }
}

void memory_move(Memory* src, void** dest, size_t* destsize) {
    if (dest) *dest = src->buf;
    if (destsize) *destsize = src->size;
    src->buf = NULL;
    src->size = 0;
}

//----------------------------------------------
// HttpRequest
//----------------------------------------------

void request_close(HttpRequest* request) {
    if (request != NULL) {
        if (request->result != NULL) memory_free(request->result);
        if (request->curl != NULL) curl_easy_cleanup(request->curl);
        free(request);
    }
}

int request_cookies(HttpRequest* request, const char* data) {
    curl_easy_setopt(request->curl, CURLOPT_COOKIE, data);
    return 0;
}

HttpRequest* request_create(HttpApi* info, const char* method, const char* path) {
    HttpRequest* request = (HttpRequest*) malloc(sizeof(HttpRequest));
    if (request == NULL) {
        obx_set_last_error_code(OBX_ERROR_ALLOCATION);
        return NULL;
    }

    request->curl = NULL;
    request->result = NULL;

    if (init_curl(&request->curl, &request->result) != OBX_SUCCESS) {
        request_close(request);
        return NULL;
    }

    curl_easy_setopt(request->curl, CURLOPT_CUSTOMREQUEST, method);

    {
        // set URL: concatenate url + path
        size_t urlLen = strlen(info->url) + strlen(path);
        char* total_url = (char*) malloc(urlLen + 1);
        if (total_url == NULL) {
            obx_set_last_error_code(OBX_ERROR_ALLOCATION);
            request_close(request);
            return NULL;
        }

        char* pUrl = total_url;
        strcpy(pUrl, info->url);
        pUrl += strlen(info->url);
        strcpy(pUrl, path);
        pUrl += strlen(path);
        *pUrl = '\0';

        curl_easy_setopt(request->curl, CURLOPT_URL, total_url);
        free(total_url);
    }

    request_cookies(request, info->cookies);

    obx_set_last_error_code(OBX_SUCCESS);
    return request;
}

int request_payload(HttpRequest* request, const void* data, size_t dataSize) {
    curl_easy_setopt(request->curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(request->curl, CURLOPT_POSTFIELDSIZE, dataSize);
    return 0;
}

long request_execute(HttpRequest* request) {
    // perform the request, res will get the return code 
    CURLcode res = curl_easy_perform(request->curl);

    // check for errors
    long rc = 0;
    if (res != CURLE_OK) {
        obx_set_last_error_code(OBX_ERROR_REQUEST_FAILED);
        OBX_LAST_ERROR_MESSAGE = (char*) curl_easy_strerror(res);
    } else {
        curl_easy_getinfo(request->curl, CURLINFO_RESPONSE_CODE, &rc);
    }

    return rc;
}

//----------------------------------------------
// RestCall
//----------------------------------------------

RestCall* rest_call_create(HttpApi* api, const char* method, const char* path) {
    RestCall* ret = (RestCall*) malloc(sizeof(RestCall));
    if (ret == NULL) {
        obx_set_last_error_code(OBX_ERROR_ALLOCATION);
        return NULL;
    }
    ret->code = 0;
    ret->request = request_create(api, method, path);
    if (ret->request == NULL) return NULL;
    return ret;
}

long rest_call_execute(RestCall* rest_call) {
    rest_call->code = request_execute(rest_call->request);
    if (rest_call->request->result == NULL) return -1;
    return rest_call->code;
}

Memory* rest_call_response(const RestCall* rest_call) {
    if (rest_call->request->result == NULL || rest_call->request->result->buf == NULL) {
        return NULL;
    }
    return rest_call->request->result;
}

void rest_call_close(RestCall* rest_call) {
    if (rest_call != NULL) {
        request_close(rest_call->request);
        free(rest_call);
    }
}

//----------------------------------------------
// HttpApi
//----------------------------------------------

HttpApi* rest_create(const char* url) {
    HttpApi* api = (HttpApi*) malloc(sizeof(HttpApi));
    if (api == NULL) {
        obx_set_last_error_code(OBX_ERROR_ALLOCATION);
        return NULL;
    }

    api->url = (char*) malloc(strlen(url) + 1);
    if (api->url == NULL) {
        obx_set_last_error_code(OBX_ERROR_ALLOCATION);
        free(api);
        return NULL;
    }
    api->cookies = NULL;
    api->url_encoder = curl_easy_init();
    if (api->url_encoder == NULL) {
        obx_set_last_error_code(OBX_ERROR_CURL_INIT_FAILED);
        free(api->url);
        free(api);
        return NULL;
    }

    memcpy(api->url, url, strlen(url) + 1);
    obx_set_last_error_code(OBX_SUCCESS);
    return api;
}

// TODO: merging with existing cookies
obx_err rest_cookie(HttpApi* api, const char* name, const char* value, size_t value_len) {
    size_t necessarySize = strlen(name) + value_len + 2;  // name=value\0
    char* ptr = NULL;
    if (api->cookies == NULL) {
        api->cookies = (char*) malloc(sizeof(char) * necessarySize);
        if (!api->cookies) {
            return obx_set_last_error_code(OBX_ERROR_ALLOCATION);
        }
        ptr = api->cookies;
    } else {
        necessarySize += 2;  // "; " // semicolon is not enough, space is necessary as well
        size_t originalSize = strlen(api->cookies);
        ptr = (char*) realloc(api->cookies, sizeof(char) * (strlen(api->cookies) + necessarySize));
        if (!ptr) {
            return obx_set_last_error_code(OBX_ERROR_ALLOCATION);
        }
        api->cookies = ptr;
        ptr += originalSize;
        memset(ptr++, ';', 1);
        memset(ptr++, ' ', 1);
    }

    memcpy(ptr, name, strlen(name));
    ptr += strlen(name);
    memset(ptr++, '=', 1);
    memcpy(ptr, value, value_len);
    ptr += value_len;
    memset(ptr++, '\0', 1);
    return obx_set_last_error_code(OBX_SUCCESS);
}

RestCall* rest_get(HttpApi* api, const char* path) {
    RestCall* call = rest_call_create(api, "GET", path);
    if (call == NULL) return NULL;
    rest_call_execute(call);
    return call;
}

RestCall* rest_del(HttpApi* api, const char* path) {
    RestCall* call = rest_call_create(api, "DELETE", path);
    if (call == NULL) return NULL;
    rest_call_execute(call);
    return call;
}

RestCall* rest_post(HttpApi* api, const char* path, const void* data, size_t size) {
    RestCall* call = rest_call_create(api, "POST", path);
    if (call == NULL) return NULL;
    request_payload(call->request, data, size);
    rest_call_execute(call);
    return call;
}

RestCall* rest_put(HttpApi* api, const char* path, const void* data, size_t size) {
    RestCall* call = rest_call_create(api, "PUT", path);
    if (call == NULL) return NULL;
    request_payload(call->request, data, size);
    rest_call_execute(call);
    return call;
}

void rest_close(HttpApi* api) {
    if (api == NULL) return;
    if (api->cookies != NULL) free(api->cookies);
    if (api->url != NULL) free(api->url);
    if (api->url_encoder != NULL) curl_easy_cleanup(api->url_encoder);
    free(api);
}
