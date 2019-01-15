/*
 * Copyright 2018 ObjectBox Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Single header file for the ObjectBox C API
//
// Naming conventions
// ------------------
// * methods: obx_thing_action()
// * structs: OBX_thing {}
// * error codes: OBX_ERROR_REASON
// * enums: TODO
//

#ifndef OBJECTBOX_H
#define OBJECTBOX_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------
// Return codes
//----------------------------------------------

/// Value returned when no error occurred (0)
#define OBX_SUCCESS 0

/// Returned by e.g. get operations if nothing was found for a specific ID.
/// This is NOT an error condition, and thus no last error info is set.
#define OBX_NOT_FOUND 404

// General errors
#define OBX_ERROR_ILLEGAL_STATE 10001
#define OBX_ERROR_ILLEGAL_ARGUMENT 10002
#define OBX_ERROR_ALLOCATION 10003
#define OBX_ERROR_NO_ERROR_INFO 10097
#define OBX_ERROR_GENERAL 10098
#define OBX_ERROR_UNKNOWN 10099

// Storage errors (often have a secondary error code)
#define OBX_ERROR_DB_FULL 10101
#define OBX_ERROR_MAX_READERS_EXCEEDED 10102
#define OBX_ERROR_STORE_MUST_SHUTDOWN 10103
#define OBX_ERROR_STORAGE_GENERAL 10199

// Data errors
#define OBX_ERROR_UNIQUE_VIOLATED 10201
#define OBX_ERROR_NON_UNIQUE_RESULT 10202
#define OBX_ERROR_PROPERTY_TYPE_MISMATCH 10203
#define OBX_ERROR_CONSTRAINT_VIOLATED 10299

// STD errors
#define OBX_ERROR_STD_ILLEGAL_ARGUMENT 10301
#define OBX_ERROR_STD_OUT_OF_RANGE 10302
#define OBX_ERROR_STD_LENGTH 10303
#define OBX_ERROR_STD_BAD_ALLOC 10304
#define OBX_ERROR_STD_RANGE 10305
#define OBX_ERROR_STD_OVERFLOW 10306
#define OBX_ERROR_STD_OTHER 10399

// Inconsistencies detected
#define OBX_ERROR_SCHEMA 10501
#define OBX_ERROR_FILE_CORRUPT 10502

// Networking errors
#define OBX_ERROR_REQUEST_FAILED 10601
#define OBX_ERROR_ILLEGAL_RESPONSE 10602
#define OBX_ERROR_CURL_INIT_FAILED 10603

//----------------------------------------------
// Common types
//----------------------------------------------
/// Schema entity & property identifiers
typedef uint32_t obx_schema_id;

/// Universal identifier used in schema for entities & properties
typedef uint64_t obx_uid;

/// ID of a single Object stored in the database
typedef uint64_t obx_id;

/// Error code returned by an obx_* function
typedef int obx_err;

typedef struct OBXC_bytes {
    void* data;
    size_t size;
} OBXC_bytes;

typedef struct OBXC_bytes_array {
    OBXC_bytes* bytes;
    size_t count;

    // if not NULL, this variable indicates that the entire array's memory is continuous and to free it,
    // only one call to free() is needed
    void* baseptr;
} OBXC_bytes_array;

//----------------------------------------------
// Error info
//----------------------------------------------

obx_err obxc_last_error_code();
const char* obxc_last_error_message();
obx_err obxc_last_error_secondary();
void obxc_last_error_clear();

//----------------------------------------------
// Store
//----------------------------------------------

struct OBXC_store;
typedef struct OBXC_store OBXC_store;

typedef struct OBXC_store_options {
    const char* base_url;
    const char* db;
    const char* user;
    const char* pass;
    OBXC_bytes model;
} OBXC_store_options;

OBXC_store* obxc_store_open(const OBXC_store_options* options);
obx_err obxc_store_close(OBXC_store* store);

//----------------------------------------------
// Data insertion and retrieval
//----------------------------------------------

obx_err obxc_data_count(OBXC_store* store, int entityId, uint64_t* count);
obx_err obxc_data_get(OBXC_store* store, int entityId, int id, OBXC_bytes* dest);
obx_err obxc_data_get_all(OBXC_store* store, int entityId, OBXC_bytes_array* dest);
obx_err obxc_data_insert(OBXC_store* store, int entityId, const OBXC_bytes* src, int* id);
obx_err obxc_data_update(OBXC_store* store, int entityId, int id, const OBXC_bytes* src);
obx_err obxc_data_delete(OBXC_store* store, int entityId, int id);

void obxc_bytes_free(OBXC_bytes* bytes);
void obxc_bytes_array_free(OBXC_bytes_array* bytes_array);

#ifdef OBXC_USE_OBX_ALIASES
#define OBX_bytes OBXC_bytes
#define OBX_bytes_array OBXC_bytes_array

#define obx_last_error_code obxc_last_error_code
#define obx_last_error_message obxc_last_error_message
#define obx_last_error_secondary obxc_last_error_secondary
#define obx_last_error_clear obxc_last_error_clear

#define OBX_store OBXC_store
#define OBX_store_options OBXC_store_options
#define obx_store_open obxc_store_open
#define obx_store_close obxc_store_close

#define obx_data_count obxc_data_count
#define obx_data_get obxc_data_get
#define obx_data_get_all obxc_data_get_all
#define obx_data_insert obxc_data_insert
#define obx_data_update obxc_data_update
#define obx_data_delete obxc_data_delete

#define obx_bytes_free obxc_bytes_free
#define obx_bytes_array_free obxc_bytes_array_free
#endif

#ifdef __cplusplus
}
#endif

#endif  // OBJECTBOX_H
