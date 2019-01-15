#define OBXC_USE_OBX_ALIASES
#include "error_manager.h"

obx_err OBX_LAST_ERROR_CODE = 0;
char* OBX_LAST_ERROR_MESSAGE;
obx_err OBX_LAST_ERROR_SECONDARY = 0;
char OBX_LAST_RESPONSE_ERROR_MESSAGE[256];

obx_err obx_last_error_code() { return OBX_LAST_ERROR_CODE; }

obx_err obx_last_error_secondary() { return OBX_LAST_ERROR_SECONDARY; }

const char* obx_last_error_message() {
    if (OBX_LAST_ERROR_CODE == OBX_ERROR_ILLEGAL_RESPONSE) {
        return OBX_LAST_RESPONSE_ERROR_MESSAGE;
    }
    return OBX_LAST_ERROR_MESSAGE;
}

void obx_last_error_clear() {
    OBX_LAST_ERROR_CODE = 0;
    OBX_LAST_ERROR_SECONDARY = 0;
    OBX_LAST_ERROR_MESSAGE = "";
}

obx_err obx_set_last_error_code(obx_err e) {
    OBX_LAST_ERROR_CODE = e;
    return e;
}
