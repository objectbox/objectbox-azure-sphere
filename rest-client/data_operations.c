#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OBXC_USE_OBX_ALIASES
#include "error_manager.h"
#include "objectbox.h"
#include "obtypes.h"
#include "utilities.h"

#define OBX_CONSTRUCT_REST_PATH(FMTSTRING, ...) \
    char path[128];                             \
    snprintf(path, 128, FMTSTRING, __VA_ARGS__);

#define OBX_CHECK_REST_CALL         \
    if (call == NULL) {             \
        rest_call_close(call);      \
        return OBX_LAST_ERROR_CODE; \
    }

#define OBX_REST_CALL(RESTFUNC, FMTSTRING, ...)       \
    OBX_CONSTRUCT_REST_PATH(FMTSTRING, __VA_ARGS__);  \
    RestCall* call = RESTFUNC(store->http_api, path); \
    OBX_CHECK_REST_CALL

#define OBX_REST_CALL_DATA(RESTFUNC, DATA, SIZE, FMTSTRING, ...)  \
    OBX_CONSTRUCT_REST_PATH(FMTSTRING, __VA_ARGS__);              \
    RestCall* call = RESTFUNC(store->http_api, path, DATA, SIZE); \
    OBX_CHECK_REST_CALL

obx_err obx_data_count(OBX_store* store, int entityId, uint64_t* count) {
    // check if parameters are valid
    if (store == NULL || store->http_api == NULL || entityId < 0 || count == NULL) {
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_ARGUMENT);
    }

    // do rest call and parse response as unsigned long long
    OBX_REST_CALL(rest_get, "/data/%d/count", entityId);
    Memory* resp_mem = rest_call_response(call);
    if (resp_mem == NULL || !safe_uint64_parse(resp_mem->buf, resp_mem->size, count)) {
        if (resp_mem != NULL) parse_error_response(resp_mem);
        rest_call_close(call);
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_RESPONSE);
    }

    rest_call_close(call);
    return obx_set_last_error_code(OBX_SUCCESS);
}

obx_err obx_data_get(OBX_store* store, int entityId, int id, OBX_bytes* dest) {
    // check if parameters are valid
    if (store == NULL || store->http_api == NULL || entityId < 0 || id < 0 || dest == NULL) {
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_ARGUMENT);
    }

    // do rest call and move data to given buffer
    OBX_REST_CALL(rest_get, "/data/%d/%d?fb", entityId, id);
    Memory* resp_mem = rest_call_response(call);
    if (parse_error_response(resp_mem)) {
        rest_call_close(call);
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_RESPONSE);
    }
    memory_move(resp_mem, &dest->data, &dest->size);

    rest_call_close(call);
    return obx_set_last_error_code(OBX_SUCCESS);
}

obx_err obx_data_get_all(OBX_store* store, int entityId, OBX_bytes_array* dest) {
    // check if parameters are valid
    if (store == NULL || store->http_api == NULL || entityId < 0 || dest == NULL) {
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_ARGUMENT);
    }

    // do rest call
    OBX_REST_CALL(rest_get, "/data/%d/?fb", entityId);
    Memory* resp_mem = rest_call_response(call);
    if (parse_error_response(resp_mem)) {
        rest_call_close(call);
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_RESPONSE);
    }

    // find number of entries first: entry consists of 32 bit size and this much data, last entry is indicated by size=0
    char* curr_ptr = resp_mem->buf;
    int chunk_idx = 0, i = 0, curr_size;
    while (i < resp_mem->size && (curr_size = *((uint32_t*) curr_ptr)) != 0) {
        ++chunk_idx;
        curr_ptr += curr_size + 4;  // +4 to skip the 32 bit size as well
    }
    if (curr_size != 0) {
        rest_call_close(call);
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_RESPONSE);
    }
    dest->count = chunk_idx;
    dest->bytes = (OBX_bytes*) malloc(chunk_idx * sizeof(OBX_bytes));

    // re-iterate through the data to populate the entries with pointers to data
    chunk_idx = 0;
    curr_ptr = resp_mem->buf;
    while ((curr_size = *((uint32_t*) curr_ptr)) != 0) {
        dest->bytes[chunk_idx].size = curr_size;
        dest->bytes[chunk_idx].data = curr_ptr + 4;
        ++chunk_idx;
        curr_ptr += curr_size + 4;
    }

    // move address of response to dest's base pointer to indicate that its memory is continuous
    memory_move(resp_mem, &dest->baseptr, NULL);
    rest_call_close(call);
    return obx_set_last_error_code(OBX_SUCCESS);
}

obx_err obx_data_insert(OBX_store* store, int entityId, const OBX_bytes* src, int* id) {
    // check if parameters are valid
    if (store == NULL || store->http_api == NULL || entityId < 0 || src == NULL || src->data == NULL ||
        src->size == 0 || id == NULL) {
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_ARGUMENT);
    }

    // do rest call, new id is returned as response
    OBX_REST_CALL_DATA(rest_post, src->data, src->size, "/data/%d?fb", entityId);
    Memory* resp_mem = rest_call_response(call);
    if (parse_error_response(resp_mem)) {
        rest_call_close(call);
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_RESPONSE);
    }
    *id = atoi_n((char*) resp_mem->buf, resp_mem->size);

    rest_call_close(call);
    return obx_set_last_error_code(OBX_SUCCESS);
}

obx_err obx_data_update(OBX_store* store, int entityId, int id, const OBX_bytes* src) {
    // check if parameters are valid
    if (store == NULL || store->http_api == NULL || entityId < 0 || id < 0 || src == NULL || src->data == NULL ||
        src->size == 0) {
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_ARGUMENT);
    }

    // do rest call which responds with "204 No Content"
    OBX_REST_CALL_DATA(rest_put, src->data, src->size, "/data/%d/%d?fb", entityId, id);
    Memory* resp_mem = rest_call_response(call);
    if (parse_error_response(resp_mem) || call->code != 204) {
        rest_call_close(call);
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_RESPONSE);
    }

    rest_call_close(call);
    return obx_set_last_error_code(OBX_SUCCESS);
}

obx_err obx_data_delete(OBX_store* store, int entityId, int id) {
    // check if parameters are valid
    if (store == NULL || store->http_api == NULL || entityId < 0 || id < 0) {
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_ARGUMENT);
    }

    // do rest call which responds with "204 No Content"
    OBX_REST_CALL(rest_del, "/data/%d/%d", entityId, id);
    Memory* resp_mem = rest_call_response(call);
    if (parse_error_response(resp_mem) || call->code != 204) {
        rest_call_close(call);
        return obx_set_last_error_code(OBX_ERROR_ILLEGAL_RESPONSE);
    }

    rest_call_close(call);
    return obx_set_last_error_code(OBX_SUCCESS);
}

void obx_bytes_free(OBX_bytes* bytes) {
    if (bytes) {
        if (bytes->data) {
            free(bytes->data);
            bytes->data = NULL;
        }
        bytes->size = 0;
    }
}

void obx_bytes_array_free(OBX_bytes_array* bytes_array) {
    if (bytes_array) {
        if (bytes_array->bytes) {
            if (bytes_array->baseptr) {
                free(bytes_array->baseptr);
            } else {
                for (int i = 0; i < bytes_array->count; ++i) {
                    obx_bytes_free(bytes_array->bytes + i);
                }
            }
            free(bytes_array->bytes);
            bytes_array->bytes = NULL;
            bytes_array->baseptr = NULL;
        }

        bytes_array->count = 0;
    }
}
