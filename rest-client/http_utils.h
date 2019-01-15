#ifndef OBJECTBOX_HTTP_UTILS_H
#define OBJECTBOX_HTTP_UTILS_H

#include <stdlib.h>

#include <curl/curl.h>

#include "error_manager.h"
#include "objectbox.h"

// resizable buffer from https://curl.haxx.se/libcurl/c/crawler.html
typedef struct Memory {
    char* buf;
    size_t size;
} Memory;

typedef struct HttpRequest {
    CURL* curl;
    Memory* result;
} HttpRequest;

typedef struct RestCall {
    long code;
    HttpRequest* request;
} RestCall;

typedef struct HttpApi {
    char* url;
    char* cookies;
    CURL* url_encoder;
} HttpApi;

// Utilities
obx_err init_curl(CURL** handle, Memory** mem);
char* url_encode(HttpApi* api, const char* data, size_t len);

// Memory
size_t memory_grow(void* contents, size_t sz, size_t nmemb, void* ctx);
void memory_free(Memory* mem);
void memory_move(Memory* src, void** dest, size_t* destsize);

// HttpRequest
HttpRequest* request_create(HttpApi* info, const char* method, const char* path);
int request_cookies(HttpRequest* request, const char* data);
int request_payload(HttpRequest* request, const void* data, size_t dataSize);
long request_execute(HttpRequest* request);
void request_close(HttpRequest* request);

// RestCall
RestCall* rest_call_create(HttpApi* api, const char* method, const char* path);
long rest_call_execute(RestCall* rest_call);
Memory* rest_call_response(const RestCall* rest_call);
void rest_call_close(RestCall* rest_call);

// HttpApi
HttpApi* rest_create(const char* url);
obx_err rest_cookie(HttpApi* api, const char* name, const char* value, size_t value_len);
RestCall* rest_get(HttpApi* api, const char* path);
RestCall* rest_del(HttpApi* api, const char* path);
RestCall* rest_post(HttpApi* api, const char* path, const void* data, size_t size);
RestCall* rest_put(HttpApi* api, const char* path, const void* data, size_t size);
void rest_close(HttpApi* api);

#endif  // OBJECTBOX_HTTP_UTILS_H
