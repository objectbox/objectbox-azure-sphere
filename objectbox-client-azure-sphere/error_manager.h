#ifndef OBJECTBOX_ERROR_MANAGER_H
#define OBJECTBOX_ERROR_MANAGER_H

#include "objectbox.h"

extern obx_err OBX_LAST_ERROR_CODE;
extern char* OBX_LAST_ERROR_MESSAGE;
extern obx_err OBX_LAST_ERROR_SECONDARY;
extern char OBX_LAST_RESPONSE_ERROR_MESSAGE[256];

obx_err obx_last_error_code();
obx_err obx_last_error_secondary();
const char* obx_last_error_message();
void obx_last_error_clear();

obx_err obx_set_last_error_code(obx_err e);

#endif  // OBJECTBOX_ERROR_MANAGER_H
