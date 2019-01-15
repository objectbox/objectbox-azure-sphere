#include <stdio.h>
#include <string.h>

#include <curl/curl.h>

#define OBXC_USE_OBX_ALIASES
#include "error_manager.h"
#include "http_utils.h"
#include "objectbox.h"
#include "obtypes.h"
#include "utilities.h"

obx_err obx_store_authenticate(OBX_store* store, const char* db, const char* user, const char* pass,
                               const OBX_bytes* model) {
    if (store == NULL || store->http_api == NULL) {
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_ARGUMENT);
    }

    // if user and password are not given, do not authenticate (which is not necessarily an issue)
    if (user == NULL && pass == NULL) {
        // still providing a database name is invalid in this case
        if (db != NULL) {
            return obx_set_last_error_code(OBX_ERROR_ILLEGAL_ARGUMENT);
        }
        return obx_set_last_error_code(OBX_SUCCESS);
    }

    // URL encode all given strings for safe transmission via HTTP POST
    char *db_enc = db == NULL ? NULL : url_encode(store->http_api, db, 0),
         *user_enc = url_encode(store->http_api, user, 0), *pass_enc = url_encode(store->http_api, pass, 0),
         *model_enc = model == NULL ? NULL : url_encode(store->http_api, model->data, model->size);
    if ((db != NULL && db_enc == NULL) || user_enc == NULL || pass_enc == NULL ||
        (model != NULL && model_enc == NULL)) {
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_STATE);
    }

    // build post data from encoded strings (db only needs to be included if given), finally free all cURL stuff
    size_t post_data_len = (strlen(user_enc) + strlen(pass_enc)) * 3 + 12;
    if (db != NULL) post_data_len += strlen(db_enc) * 3 + 4;
    if (model != NULL) post_data_len += model->size * 3 + 7;
    char post_data[post_data_len];
    int post_data_actual_len = snprintf(post_data, post_data_len, "user=%s&pass=%s", user_enc, pass_enc);
    if (db != NULL) {
        post_data_actual_len +=
            snprintf(post_data + post_data_actual_len, post_data_len - post_data_actual_len, "&db=%s", db_enc);
        curl_free(db_enc);
    }
    if (model != NULL) {
        snprintf(post_data + post_data_actual_len, post_data_len - post_data_actual_len, "&model=%s", model_enc);
        curl_free(model_enc);
    }
    curl_free(user_enc);
    curl_free(pass_enc);

    // actually do the POST request and check if response has expected format (would be regex /^"[0-9A-Za-z]{10}"$/)
    RestCall* call = rest_post(store->http_api, "/sessions", post_data, strlen(post_data));
    if (call == NULL) return OBX_LAST_ERROR_CODE;

    Memory* session_resp = rest_call_response(call);
    if (session_resp == NULL || session_resp->size <= 2 || session_resp->buf[0] != '"' ||
        session_resp->buf[session_resp->size - 1] != '"') {
        if (session_resp != NULL) parse_error_response(session_resp);
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_RESPONSE);
    }

    // finally store the cookie and free all temporary data
    if (rest_cookie(store->http_api, "s", session_resp->buf, session_resp->size) != OBX_SUCCESS) {
        return OBX_LAST_ERROR_CODE;
    }
    rest_call_close(call);

    return obx_set_last_error_code(OBX_SUCCESS);
}

OBX_store* obx_store_open(const OBX_store_options* options) {
    if (options == NULL || options->base_url == NULL) {
        obx_set_last_error_code(OBX_ERROR_ILLEGAL_ARGUMENT);
        return NULL;
    }

    OBX_store* ret = (OBX_store*) malloc(sizeof(OBX_store));
    if (ret == NULL) {
        obx_set_last_error_code(OBX_ERROR_ALLOCATION);
        return NULL;
    }

    ret->http_api = rest_create(options->base_url);
    if (obx_store_authenticate(ret, options->db, options->user, options->pass, options->model.data == NULL ? NULL : &options->model)) {
        return NULL;
    }

    obx_set_last_error_code(OBX_SUCCESS);
    return ret;
}

obx_err obx_store_close(OBX_store* store) {
    if (store != NULL) {
        if (store->http_api != NULL) rest_close(store->http_api);
        free(store);
    }
    return obx_set_last_error_code(OBX_SUCCESS);
}
