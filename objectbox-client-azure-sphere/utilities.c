#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OBXC_USE_OBX_ALIASES
#include "error_manager.h"
#include "utilities.h"

#define MAX_NUM_STRLEN 20

int safe_uint64_parse(const char* str, size_t len, uint64_t* dest) {
    if (str == NULL || dest == NULL) return 0;
    *dest = strtoull(str, NULL, 10);

    char num_str[MAX_NUM_STRLEN + 1];
    snprintf(num_str, MAX_NUM_STRLEN + 1, "%" PRIu64, *dest);
    if (len != strlen(num_str) || strncmp(str, num_str, len) != 0) {
        return 0;
    }

    return 1;
}

int is_whitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
int is_number(char c) { return c >= '0' && c <= '9'; }
void consume_whitespaces(const char* str, size_t len, size_t* i) {
    while (*i < len && is_whitespace(str[*i]))
        ++(*i);
}

// implements regex like /^\{ "error": \{ "code": [0-9]+, "message": ".*?" } }$/ with arbitrary whitespaces
// returns 1 if parsing was successful (in which case OBX's global error variables are set); 0 otherwise
int parse_error_response(Memory* mem) {
    if (mem == NULL || mem->buf == NULL || mem->size == 0) return 0;
    const char* str = mem->buf;
    size_t len = mem->size;

    char code_buf[11];
    char msg_buf[256];
    size_t i = 0, code_buf_i = 0, msg_buf_i = 0;

    if (len < ++i || str[i - 1] != '{') return 0;
    consume_whitespaces(str, len, &i);
    if (len < i + 8 || strncmp(&str[i], "\"error\":", 8)) return 0;
    i += 8;
    consume_whitespaces(str, len, &i);
    if (len < ++i || str[i - 1] != '{') return 0;
    consume_whitespaces(str, len, &i);
    if (len < i + 7 || strncmp(&str[i], "\"code\":", 7)) return 0;
    i += 7;
    consume_whitespaces(str, len, &i);
    while (i < len && is_number(str[i])) {
        if (code_buf_i < 10) code_buf[code_buf_i++] = str[i];
        ++i;
    }
    code_buf[code_buf_i] = '\0';
    if (len < ++i || str[i - 1] != ',') return 0;
    consume_whitespaces(str, len, &i);
    if (len < i + 10 || strncmp(&str[i], "\"message\":", 10)) return 0;
    i += 10;
    consume_whitespaces(str, len, &i);
    if (len < ++i || str[i - 1] != '"') return 0;
    while (i < len) {
        if (str[i] == '"') {
            ++i;
            break;
        }
        if (msg_buf_i < 255) msg_buf[msg_buf_i++] = str[i];
        ++i;
    }
    if (i == len) return 0;
    msg_buf[msg_buf_i] = '\0';
    consume_whitespaces(str, len, &i);
    if (len < ++i || str[i - 1] != '}') return 0;
    consume_whitespaces(str, len, &i);
    if (len < ++i || str[i - 1] != '}') return 0;

    OBX_LAST_ERROR_CODE = OBX_ERROR_ILLEGAL_RESPONSE;
    strncpy(OBX_LAST_RESPONSE_ERROR_MESSAGE, msg_buf, 256);
    OBX_LAST_ERROR_SECONDARY = atoi(code_buf);
    return 1;
}

int atoi_n(char* str, size_t len) {
    int res = 0;
    int sign = 1;

    for (int i = 0; i < len; ++i, ++str) {
        if (*str < '0' || *str > '9')
            return 0;
        else if (*str == '-')
            sign = -1;
        else
            res = ((res * 10) + (*str - '0'));
    }

    return sign * res;
}
