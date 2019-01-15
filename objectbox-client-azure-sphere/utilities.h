#ifndef OBJECTBOX_UTILITIES_H
#define OBJECTBOX_UTILITIES_H

#include <inttypes.h>
#include <stddef.h>

#include "http_utils.h"

int safe_uint64_parse(const char* str, size_t len, uint64_t* dest);
int parse_error_response(Memory* mem);
int atoi_n(char* str, size_t len);

#endif  // OBJECTBOX_UTILITIES_H
