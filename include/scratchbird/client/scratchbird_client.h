#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef struct sb_connection sb_connection;
typedef struct sb_prepared sb_prepared;
typedef struct sb_result sb_result;
typedef struct sb_row {
    struct sb_result* result;
    size_t row_index;
} sb_row;

typedef enum sb_error_code {
    SB_OK = 0,
    SB_ERR_CONNECTION_FAILED = 1,
    SB_ERR_AUTH_FAILED = 2,
    SB_ERR_SSL_FAILED = 3,
    SB_ERR_TIMEOUT = 4,
    SB_ERR_DISCONNECTED = 5,
    SB_ERR_PROTOCOL = 6,
    SB_ERR_SYNTAX = 100,
    SB_ERR_CONSTRAINT = 105,
    SB_ERR_TYPE_MISMATCH = 104,
    SB_ERR_TXN_CONFLICT = 200,
    SB_ERR_DEADLOCK = 201,
    SB_ERR_SERIALIZATION = 202,
    SB_ERR_TXN_ABORTED = 203,
    SB_ERR_NO_ACTIVE_TXN = 204,
    SB_ERR_OUT_OF_MEMORY = 300,
    SB_ERR_DISK_FULL = 301,
    SB_ERR_TOO_MANY_CONNECTIONS = 302,
    SB_ERR_RESOURCE_BUSY = 303,
    SB_ERR_INVALID_HANDLE = 400,
    SB_ERR_INVALID_PARAM = 401,
    SB_ERR_PARAM_COUNT = 402,
    SB_ERR_NULL_POINTER = 403,
    SB_ERR_INVALID_STATE = 500,
    SB_ERR_ALREADY_CONNECTED = 501,
    SB_ERR_NOT_CONNECTED = 502,
    SB_ERR_RESULT_EXHAUSTED = 503,
    SB_ERR_STATEMENT_CLOSED = 504,
    SB_ERR_NOT_IMPLEMENTED = 901,
    SB_ERR_UNKNOWN = 999
} sb_error_code;

typedef struct sb_error {
    sb_error_code code;
    char message[256];
} sb_error;

typedef enum sb_type {
    SB_TYPE_NULL = 0,
    SB_TYPE_BOOLEAN = 1,
    SB_TYPE_SMALLINT = 2,
    SB_TYPE_INTEGER = 3,
    SB_TYPE_BIGINT = 4,
    SB_TYPE_REAL = 5,
    SB_TYPE_DOUBLE = 6,
    SB_TYPE_DECIMAL = 7,
    SB_TYPE_CHAR = 10,
    SB_TYPE_VARCHAR = 11,
    SB_TYPE_TEXT = 12,
    SB_TYPE_BLOB = 20,
    SB_TYPE_DATE = 30,
    SB_TYPE_TIME = 31,
    SB_TYPE_TIMESTAMP = 32,
    SB_TYPE_TIMESTAMP_TZ = 33,
    SB_TYPE_INTERVAL = 34,
    SB_TYPE_UUID = 40,
    SB_TYPE_JSON = 41,
    SB_TYPE_ARRAY = 50,
    SB_TYPE_INET = 60,
    SB_TYPE_CIDR = 61,
    SB_TYPE_MACADDR = 62
} sb_type;

typedef struct sb_value {
    sb_type type;
    int is_null;
    union {
        int8_t boolean_val;
        int16_t smallint_val;
        int32_t integer_val;
        int64_t bigint_val;
        float real_val;
        double double_val;
        struct {
            const char* data;
            size_t length;
        } string_val;
        struct {
            const uint8_t* data;
            size_t length;
        } binary_val;
        struct {
            int32_t year;
            int32_t month;
            int32_t day;
        } date_val;
        struct {
            int32_t hour;
            int32_t minute;
            int32_t second;
            int32_t microsecond;
        } time_val;
        struct {
            int64_t epoch_microseconds;
            int32_t tz_offset_seconds;
        } timestamp_val;
        struct {
            uint8_t bytes[16];
        } uuid_val;
    } data;
} sb_value;

typedef struct sb_column_meta {
    const char* name;
    sb_type type;
    int32_t type_modifier;
    int nullable;
} sb_column_meta;

sb_connection* sb_connect(const char* conn_str, sb_error* err);
void sb_disconnect(sb_connection* conn);

sb_result* sb_execute(sb_connection* conn, const char* sql, sb_error* err);
sb_result* sb_query(sb_connection* conn, const char* sql, sb_error* err);

int sb_fetch(sb_result* result, sb_row* row, sb_error* err);
void sb_result_free(sb_result* result);

int sb_column_count(sb_result* result);
int sb_get_column_meta(sb_result* result, int index, sb_column_meta* out);

int sb_value_get(sb_row* row, int column, sb_value* out);
int sb_get_int64(sb_row* row, int column, int64_t* out);
const char* sb_get_string(sb_row* row, int column, size_t* length);

sb_prepared* sb_prepare(sb_connection* conn, const char* sql, sb_error* err);
int sb_bind_index(sb_prepared* stmt, size_t index, const sb_value* value, sb_error* err);
int sb_bind_name(sb_prepared* stmt, const char* name, const sb_value* value, sb_error* err);
sb_result* sb_execute_prepared(sb_prepared* stmt, sb_error* err);
void sb_prepared_free(sb_prepared* stmt);

int sb_tx_begin(sb_connection* conn, sb_error* err);
int sb_tx_commit(sb_connection* conn, sb_error* err);
int sb_tx_rollback(sb_connection* conn, sb_error* err);

#ifdef __cplusplus
}
#endif
