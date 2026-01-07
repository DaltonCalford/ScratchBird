# ScratchBird Client Library API Specification

## 1. Overview

The `libscratchbird_client` library provides a C API for connecting to ScratchBird databases, executing queries, and managing transactions. It supports both embedded (in-process) and network (client-server) modes.

**Library Name:** `libscratchbird_client.so` / `libscratchbird_client.a`
**Header:** `scratchbird_client.h`
**Version:** 1.0.0

---

## 2. Design Principles

1. **C API for Maximum Compatibility**: Pure C interface for easy FFI bindings
2. **Thread Safety**: All functions are thread-safe unless noted otherwise
3. **Error Handling**: Consistent error codes with detailed error messages
4. **Resource Management**: Clear ownership semantics with explicit cleanup
5. **Zero-Copy Where Possible**: Minimize data copying for performance

---

## 3. Core Types

### 3.1 Opaque Handle Types

```c
/* Connection handle */
typedef struct SBConnection SBConnection;

/* Statement handle (prepared statement) */
typedef struct SBStatement SBStatement;

/* Result set handle */
typedef struct SBResult SBResult;

/* Transaction handle */
typedef struct SBTransaction SBTransaction;

/* Batch handle (for bulk operations) */
typedef struct SBBatch SBBatch;

/* Subscription handle (for notifications) */
typedef struct SBSubscription SBSubscription;
```

### 3.2 Error Codes

```c
typedef enum SBError {
    SB_OK                       = 0,     /* Success */

    /* Connection errors (1-99) */
    SB_ERR_CONNECTION_FAILED    = 1,     /* Cannot connect to server */
    SB_ERR_AUTH_FAILED          = 2,     /* Authentication failed */
    SB_ERR_SSL_FAILED           = 3,     /* TLS/SSL error */
    SB_ERR_TIMEOUT              = 4,     /* Connection/query timeout */
    SB_ERR_DISCONNECTED         = 5,     /* Connection lost */
    SB_ERR_PROTOCOL             = 6,     /* Protocol error */
    SB_ERR_VERSION_MISMATCH     = 7,     /* Incompatible server version */

    /* Query errors (100-199) */
    SB_ERR_SYNTAX               = 100,   /* SQL syntax error */
    SB_ERR_SEMANTIC             = 101,   /* SQL semantic error */
    SB_ERR_TABLE_NOT_FOUND      = 102,   /* Table does not exist */
    SB_ERR_COLUMN_NOT_FOUND     = 103,   /* Column does not exist */
    SB_ERR_TYPE_MISMATCH        = 104,   /* Type conversion error */
    SB_ERR_CONSTRAINT           = 105,   /* Constraint violation */
    SB_ERR_PERMISSION           = 106,   /* Permission denied */

    /* Transaction errors (200-299) */
    SB_ERR_TXN_CONFLICT         = 200,   /* Transaction conflict (MGA) */
    SB_ERR_DEADLOCK             = 201,   /* Deadlock detected */
    SB_ERR_SERIALIZATION        = 202,   /* Serialization failure */
    SB_ERR_TXN_ABORTED          = 203,   /* Transaction aborted */
    SB_ERR_NO_ACTIVE_TXN        = 204,   /* No active transaction */

    /* Resource errors (300-399) */
    SB_ERR_OUT_OF_MEMORY        = 300,   /* Memory allocation failed */
    SB_ERR_DISK_FULL            = 301,   /* Disk space exhausted */
    SB_ERR_TOO_MANY_CONNECTIONS = 302,   /* Connection limit reached */
    SB_ERR_RESOURCE_BUSY        = 303,   /* Resource is busy */

    /* Parameter errors (400-499) */
    SB_ERR_INVALID_HANDLE       = 400,   /* Invalid handle passed */
    SB_ERR_INVALID_PARAM        = 401,   /* Invalid parameter value */
    SB_ERR_PARAM_COUNT          = 402,   /* Wrong number of parameters */
    SB_ERR_NULL_POINTER         = 403,   /* NULL pointer passed */

    /* State errors (500-599) */
    SB_ERR_INVALID_STATE        = 500,   /* Operation not valid in current state */
    SB_ERR_ALREADY_CONNECTED    = 501,   /* Already connected */
    SB_ERR_NOT_CONNECTED        = 502,   /* Not connected */
    SB_ERR_RESULT_EXHAUSTED     = 503,   /* No more rows */
    SB_ERR_STATEMENT_CLOSED     = 504,   /* Statement already closed */

    /* Internal errors (900-999) */
    SB_ERR_INTERNAL             = 900,   /* Internal error */
    SB_ERR_NOT_IMPLEMENTED      = 901,   /* Feature not implemented */
    SB_ERR_UNKNOWN              = 999,   /* Unknown error */
} SBError;
```

### 3.3 Data Types

```c
typedef enum SBType {
    SB_TYPE_NULL        = 0,

    /* Boolean */
    SB_TYPE_BOOLEAN     = 1,

    /* Integers */
    SB_TYPE_SMALLINT    = 2,
    SB_TYPE_INTEGER     = 3,
    SB_TYPE_BIGINT      = 4,

    /* Floating point */
    SB_TYPE_REAL        = 5,
    SB_TYPE_DOUBLE      = 6,
    SB_TYPE_DECIMAL     = 7,

    /* Character types */
    SB_TYPE_CHAR        = 10,
    SB_TYPE_VARCHAR     = 11,
    SB_TYPE_TEXT        = 12,

    /* Binary */
    SB_TYPE_BLOB        = 20,

    /* Date/Time */
    SB_TYPE_DATE        = 30,
    SB_TYPE_TIME        = 31,
    SB_TYPE_TIMESTAMP   = 32,
    SB_TYPE_TIMESTAMP_TZ = 33,
    SB_TYPE_INTERVAL    = 34,

    /* Other */
    SB_TYPE_UUID        = 40,
    SB_TYPE_JSON        = 41,
    SB_TYPE_ARRAY       = 50,

    /* Network */
    SB_TYPE_INET        = 60,
    SB_TYPE_CIDR        = 61,
    SB_TYPE_MACADDR     = 62,

    /* Geometric */
    SB_TYPE_POINT       = 70,
    SB_TYPE_LINE        = 71,
    SB_TYPE_POLYGON     = 72,
    SB_TYPE_BOX         = 73,
    SB_TYPE_CIRCLE      = 74,

    /* Range types */
    SB_TYPE_INT4RANGE   = 80,
    SB_TYPE_INT8RANGE   = 81,
    SB_TYPE_NUMRANGE    = 82,
    SB_TYPE_TSRANGE     = 83,
    SB_TYPE_DATERANGE   = 84,
} SBType;
```

### 3.4 Value Structure

```c
/* Represents a typed value */
typedef struct SBValue {
    SBType type;
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
} SBValue;
```

### 3.5 Column Description

```c
typedef struct SBColumnDesc {
    const char* name;           /* Column name */
    const char* table_name;     /* Source table (may be NULL) */
    const char* schema_name;    /* Source schema (may be NULL) */
    SBType type;                /* Data type */
    int32_t type_modifier;      /* Type-specific modifier */
    int nullable;               /* 1 if nullable, 0 otherwise */
    int32_t precision;          /* For numeric types */
    int32_t scale;              /* For numeric types */
} SBColumnDesc;
```

---

## 4. Connection Management

### 4.1 Connection Options

```c
typedef struct SBConnectOptions {
    /* Required */
    const char* host;           /* Hostname or IP (NULL for embedded) */
    uint16_t port;              /* Port (0 for default 3092) */
    const char* database;       /* Database path or name */
    const char* user;           /* Username */
    const char* password;       /* Password */

    /* Optional - Timeouts (milliseconds, 0 = default) */
    uint32_t connect_timeout;   /* Connection timeout (default: 10000) */
    uint32_t query_timeout;     /* Query timeout (default: 0 = no limit) */
    uint32_t idle_timeout;      /* Idle connection timeout (default: 0) */

    /* Optional - SSL/TLS */
    int use_ssl;                /* 1 to enable SSL */
    const char* ssl_mode;       /* disable/allow/prefer/require/verify-ca/verify-full */
    const char* ssl_ca_cert;    /* CA certificate path */
    const char* ssl_client_cert;/* Client certificate path */
    const char* ssl_client_key; /* Client private key path */

    /* Optional - Connection behavior */
    int auto_reconnect;         /* 1 to auto-reconnect on disconnect */
    int read_only;              /* 1 for read-only connection */
    const char* application_name; /* Application name for logging */

    /* Optional - Embedded mode */
    int embedded;               /* 1 for embedded (in-process) mode */
    int create_if_missing;      /* 1 to create database if not exists */

    /* Reserved for future use */
    void* reserved[8];
} SBConnectOptions;

/* Initialize options with defaults */
void sb_connect_options_init(SBConnectOptions* options);
```

### 4.2 Connection Functions

```c
/**
 * Create a new connection to ScratchBird database.
 *
 * @param options Connection options (must not be NULL)
 * @param conn_out Pointer to receive connection handle
 * @return SB_OK on success, error code on failure
 *
 * Thread Safety: Safe to call from multiple threads
 *
 * Example:
 *   SBConnectOptions opts;
 *   sb_connect_options_init(&opts);
 *   opts.host = "localhost";
 *   opts.port = 3092;
 *   opts.database = "mydb";
 *   opts.user = "admin";
 *   opts.password = "secret";
 *
 *   SBConnection* conn;
 *   SBError err = sb_connect(&opts, &conn);
 */
SBError sb_connect(const SBConnectOptions* options, SBConnection** conn_out);

/**
 * Create embedded (in-process) connection.
 *
 * @param database_path Path to database file
 * @param create Create if not exists
 * @param conn_out Pointer to receive connection handle
 * @return SB_OK on success, error code on failure
 */
SBError sb_connect_embedded(const char* database_path, int create,
                            SBConnection** conn_out);

/**
 * Close connection and release resources.
 *
 * @param conn Connection handle (may be NULL)
 *
 * After this call, conn is invalid and must not be used.
 */
void sb_disconnect(SBConnection* conn);

/**
 * Check if connection is still alive.
 *
 * @param conn Connection handle
 * @return 1 if connected, 0 if disconnected
 */
int sb_is_connected(SBConnection* conn);

/**
 * Send a ping to verify connection.
 *
 * @param conn Connection handle
 * @return SB_OK if server responds, error code otherwise
 */
SBError sb_ping(SBConnection* conn);

/**
 * Reset connection state (cancel pending queries, rollback transaction).
 *
 * @param conn Connection handle
 * @return SB_OK on success, error code on failure
 */
SBError sb_reset(SBConnection* conn);

/**
 * Get server version string.
 *
 * @param conn Connection handle
 * @return Server version string (do not free)
 */
const char* sb_server_version(SBConnection* conn);

/**
 * Get last error message for connection.
 *
 * @param conn Connection handle
 * @return Error message string (do not free)
 */
const char* sb_error_message(SBConnection* conn);

/**
 * Get SQLSTATE code for last error.
 *
 * @param conn Connection handle
 * @return 5-character SQLSTATE code (do not free)
 */
const char* sb_error_sqlstate(SBConnection* conn);
```

---

## 5. Query Execution

### 5.1 Simple Query Execution

```c
/**
 * Execute a SQL query and return results.
 *
 * @param conn Connection handle
 * @param sql SQL query string
 * @param result_out Pointer to receive result handle
 * @return SB_OK on success, error code on failure
 *
 * The result must be freed with sb_result_free().
 *
 * Example:
 *   SBResult* result;
 *   SBError err = sb_execute(conn, "SELECT * FROM users", &result);
 *   if (err == SB_OK) {
 *       while (sb_result_next(result) == SB_OK) {
 *           // Process row
 *       }
 *       sb_result_free(result);
 *   }
 */
SBError sb_execute(SBConnection* conn, const char* sql, SBResult** result_out);

/**
 * Execute a SQL statement that returns no result set.
 *
 * @param conn Connection handle
 * @param sql SQL statement string
 * @param rows_affected_out Pointer to receive affected row count (may be NULL)
 * @return SB_OK on success, error code on failure
 *
 * Use for INSERT, UPDATE, DELETE, DDL statements.
 */
SBError sb_execute_update(SBConnection* conn, const char* sql,
                          int64_t* rows_affected_out);

/**
 * Execute multiple SQL statements separated by semicolons.
 *
 * @param conn Connection handle
 * @param sql Multiple SQL statements
 * @return SB_OK on success, error code on failure (on first error)
 */
SBError sb_execute_batch_sql(SBConnection* conn, const char* sql);
```

### 5.2 Prepared Statements

```c
/**
 * Prepare a SQL statement for execution.
 *
 * @param conn Connection handle
 * @param sql SQL with parameter placeholders ($1, $2, ... or ?)
 * @param stmt_out Pointer to receive statement handle
 * @return SB_OK on success, error code on failure
 *
 * The statement must be freed with sb_statement_free().
 *
 * Example:
 *   SBStatement* stmt;
 *   sb_prepare(conn, "SELECT * FROM users WHERE id = $1", &stmt);
 *   sb_bind_int(stmt, 1, 42);
 *   SBResult* result;
 *   sb_statement_execute(stmt, &result);
 */
SBError sb_prepare(SBConnection* conn, const char* sql, SBStatement** stmt_out);

/**
 * Get number of parameters in prepared statement.
 *
 * @param stmt Statement handle
 * @return Number of parameters
 */
int sb_statement_param_count(SBStatement* stmt);

/**
 * Bind NULL to parameter.
 *
 * @param stmt Statement handle
 * @param index Parameter index (1-based)
 * @return SB_OK on success, error code on failure
 */
SBError sb_bind_null(SBStatement* stmt, int index);

/**
 * Bind integer value to parameter.
 */
SBError sb_bind_int(SBStatement* stmt, int index, int64_t value);

/**
 * Bind double value to parameter.
 */
SBError sb_bind_double(SBStatement* stmt, int index, double value);

/**
 * Bind string value to parameter.
 *
 * @param stmt Statement handle
 * @param index Parameter index (1-based)
 * @param value String value (will be copied)
 * @param length String length (-1 for null-terminated)
 */
SBError sb_bind_string(SBStatement* stmt, int index,
                       const char* value, int length);

/**
 * Bind binary data to parameter.
 */
SBError sb_bind_blob(SBStatement* stmt, int index,
                     const void* data, size_t length);

/**
 * Bind date value to parameter.
 */
SBError sb_bind_date(SBStatement* stmt, int index,
                     int year, int month, int day);

/**
 * Bind timestamp value to parameter.
 */
SBError sb_bind_timestamp(SBStatement* stmt, int index,
                          int64_t epoch_microseconds);

/**
 * Bind value from SBValue structure.
 */
SBError sb_bind_value(SBStatement* stmt, int index, const SBValue* value);

/**
 * Clear all parameter bindings.
 */
SBError sb_statement_clear_bindings(SBStatement* stmt);

/**
 * Execute prepared statement and return results.
 *
 * @param stmt Statement handle
 * @param result_out Pointer to receive result handle (may be NULL for non-SELECT)
 * @return SB_OK on success, error code on failure
 */
SBError sb_statement_execute(SBStatement* stmt, SBResult** result_out);

/**
 * Execute prepared statement for UPDATE/INSERT/DELETE.
 *
 * @param stmt Statement handle
 * @param rows_affected_out Pointer to receive affected row count
 * @return SB_OK on success, error code on failure
 */
SBError sb_statement_execute_update(SBStatement* stmt, int64_t* rows_affected_out);

/**
 * Free prepared statement resources.
 *
 * @param stmt Statement handle (may be NULL)
 */
void sb_statement_free(SBStatement* stmt);
```

---

## 6. Result Set Handling

### 6.1 Result Navigation

```c
/**
 * Get number of columns in result set.
 *
 * @param result Result handle
 * @return Number of columns
 */
int sb_result_column_count(SBResult* result);

/**
 * Get column description.
 *
 * @param result Result handle
 * @param index Column index (0-based)
 * @return Column description (do not free, valid until result is freed)
 */
const SBColumnDesc* sb_result_column_desc(SBResult* result, int index);

/**
 * Get column index by name.
 *
 * @param result Result handle
 * @param name Column name
 * @return Column index (0-based), or -1 if not found
 */
int sb_result_column_index(SBResult* result, const char* name);

/**
 * Move to next row.
 *
 * @param result Result handle
 * @return SB_OK if row available, SB_ERR_RESULT_EXHAUSTED if no more rows
 */
SBError sb_result_next(SBResult* result);

/**
 * Get number of rows affected (for UPDATE/INSERT/DELETE).
 *
 * @param result Result handle
 * @return Number of affected rows, or -1 if not applicable
 */
int64_t sb_result_rows_affected(SBResult* result);

/**
 * Free result set resources.
 *
 * @param result Result handle (may be NULL)
 */
void sb_result_free(SBResult* result);
```

### 6.2 Value Retrieval

```c
/**
 * Check if column value is NULL.
 *
 * @param result Result handle
 * @param index Column index (0-based)
 * @return 1 if NULL, 0 otherwise
 */
int sb_is_null(SBResult* result, int index);

/**
 * Get column value as boolean.
 *
 * @param result Result handle
 * @param index Column index (0-based)
 * @param value_out Pointer to receive value
 * @return SB_OK on success, error code on failure
 */
SBError sb_get_bool(SBResult* result, int index, int* value_out);

/**
 * Get column value as 32-bit integer.
 */
SBError sb_get_int(SBResult* result, int index, int32_t* value_out);

/**
 * Get column value as 64-bit integer.
 */
SBError sb_get_int64(SBResult* result, int index, int64_t* value_out);

/**
 * Get column value as double.
 */
SBError sb_get_double(SBResult* result, int index, double* value_out);

/**
 * Get column value as string.
 *
 * @param result Result handle
 * @param index Column index (0-based)
 * @param value_out Pointer to receive string pointer
 * @param length_out Pointer to receive string length (may be NULL)
 * @return SB_OK on success, error code on failure
 *
 * The returned string is valid until next sb_result_next() or sb_result_free().
 * String is NOT null-terminated; use length_out for string length.
 */
SBError sb_get_string(SBResult* result, int index,
                      const char** value_out, size_t* length_out);

/**
 * Get column value as null-terminated string.
 *
 * @param result Result handle
 * @param index Column index (0-based)
 * @return Null-terminated string (valid until next row or free)
 */
const char* sb_get_string_z(SBResult* result, int index);

/**
 * Get column value as binary data.
 *
 * @param result Result handle
 * @param index Column index (0-based)
 * @param data_out Pointer to receive data pointer
 * @param length_out Pointer to receive data length
 * @return SB_OK on success, error code on failure
 */
SBError sb_get_blob(SBResult* result, int index,
                    const void** data_out, size_t* length_out);

/**
 * Get column value as date components.
 *
 * @param result Result handle
 * @param index Column index (0-based)
 * @param year_out Pointer to receive year
 * @param month_out Pointer to receive month (1-12)
 * @param day_out Pointer to receive day (1-31)
 * @return SB_OK on success, error code on failure
 */
SBError sb_get_date(SBResult* result, int index,
                    int* year_out, int* month_out, int* day_out);

/**
 * Get column value as time components.
 */
SBError sb_get_time(SBResult* result, int index,
                    int* hour_out, int* minute_out, int* second_out,
                    int* microsecond_out);

/**
 * Get column value as timestamp (microseconds since epoch).
 */
SBError sb_get_timestamp(SBResult* result, int index, int64_t* value_out);

/**
 * Get column value as SBValue structure.
 *
 * @param result Result handle
 * @param index Column index (0-based)
 * @param value_out Pointer to receive value (caller allocates)
 * @return SB_OK on success, error code on failure
 */
SBError sb_get_value(SBResult* result, int index, SBValue* value_out);

/**
 * Get column type.
 *
 * @param result Result handle
 * @param index Column index (0-based)
 * @return Column type
 */
SBType sb_get_type(SBResult* result, int index);
```

---

## 7. Transaction Management

### 7.1 Transaction Functions

```c
/**
 * Begin a new transaction.
 *
 * @param conn Connection handle
 * @return SB_OK on success, error code on failure
 *
 * Uses default isolation level (SNAPSHOT/Repeatable Read).
 */
SBError sb_begin(SBConnection* conn);

/**
 * Begin a transaction with specific isolation level.
 *
 * @param conn Connection handle
 * @param isolation Isolation level string:
 *   - "READ COMMITTED"
 *   - "REPEATABLE READ" or "SNAPSHOT"
 *   - "SERIALIZABLE"
 * @return SB_OK on success, error code on failure
 */
SBError sb_begin_isolation(SBConnection* conn, const char* isolation);

/**
 * Begin a read-only transaction.
 *
 * @param conn Connection handle
 * @return SB_OK on success, error code on failure
 */
SBError sb_begin_read_only(SBConnection* conn);

/**
 * Commit current transaction.
 *
 * @param conn Connection handle
 * @return SB_OK on success, error code on failure
 */
SBError sb_commit(SBConnection* conn);

/**
 * Rollback current transaction.
 *
 * @param conn Connection handle
 * @return SB_OK on success, error code on failure
 */
SBError sb_rollback(SBConnection* conn);

/**
 * Create a savepoint.
 *
 * @param conn Connection handle
 * @param name Savepoint name
 * @return SB_OK on success, error code on failure
 */
SBError sb_savepoint(SBConnection* conn, const char* name);

/**
 * Rollback to a savepoint.
 *
 * @param conn Connection handle
 * @param name Savepoint name
 * @return SB_OK on success, error code on failure
 */
SBError sb_rollback_to(SBConnection* conn, const char* name);

/**
 * Release a savepoint.
 *
 * @param conn Connection handle
 * @param name Savepoint name
 * @return SB_OK on success, error code on failure
 */
SBError sb_release_savepoint(SBConnection* conn, const char* name);

/**
 * Check if a transaction is active.
 *
 * @param conn Connection handle
 * @return 1 if in transaction, 0 otherwise
 */
int sb_in_transaction(SBConnection* conn);

/**
 * Set auto-commit mode.
 *
 * @param conn Connection handle
 * @param auto_commit 1 to enable, 0 to disable
 * @return SB_OK on success, error code on failure
 */
SBError sb_set_autocommit(SBConnection* conn, int auto_commit);
```

---

## 8. Batch Operations

### 8.1 Batch Insert

```c
/**
 * Create a batch insert operation.
 *
 * @param conn Connection handle
 * @param table_name Target table name
 * @param columns Array of column names
 * @param column_count Number of columns
 * @param batch_out Pointer to receive batch handle
 * @return SB_OK on success, error code on failure
 *
 * Example:
 *   const char* cols[] = {"id", "name", "email"};
 *   SBBatch* batch;
 *   sb_batch_create(conn, "users", cols, 3, &batch);
 *
 *   for (int i = 0; i < 1000; i++) {
 *       sb_batch_add_int(batch, i);
 *       sb_batch_add_string(batch, names[i], -1);
 *       sb_batch_add_string(batch, emails[i], -1);
 *       sb_batch_end_row(batch);
 *   }
 *
 *   int64_t inserted;
 *   sb_batch_execute(batch, &inserted);
 *   sb_batch_free(batch);
 */
SBError sb_batch_create(SBConnection* conn, const char* table_name,
                        const char** columns, int column_count,
                        SBBatch** batch_out);

/**
 * Add NULL value to current row.
 */
SBError sb_batch_add_null(SBBatch* batch);

/**
 * Add integer value to current row.
 */
SBError sb_batch_add_int(SBBatch* batch, int64_t value);

/**
 * Add double value to current row.
 */
SBError sb_batch_add_double(SBBatch* batch, double value);

/**
 * Add string value to current row.
 */
SBError sb_batch_add_string(SBBatch* batch, const char* value, int length);

/**
 * Add binary value to current row.
 */
SBError sb_batch_add_blob(SBBatch* batch, const void* data, size_t length);

/**
 * End current row and start new row.
 */
SBError sb_batch_end_row(SBBatch* batch);

/**
 * Execute batch insert.
 *
 * @param batch Batch handle
 * @param rows_inserted_out Pointer to receive inserted row count
 * @return SB_OK on success, error code on failure
 */
SBError sb_batch_execute(SBBatch* batch, int64_t* rows_inserted_out);

/**
 * Free batch resources.
 */
void sb_batch_free(SBBatch* batch);
```

---

## 9. Async Operations

### 9.1 Async Query Execution

```c
/**
 * Start async query execution.
 *
 * @param conn Connection handle
 * @param sql SQL query string
 * @return SB_OK if query started, error code on failure
 */
SBError sb_execute_async(SBConnection* conn, const char* sql);

/**
 * Check if async query is complete.
 *
 * @param conn Connection handle
 * @return 1 if complete, 0 if still running
 */
int sb_is_query_complete(SBConnection* conn);

/**
 * Wait for async query to complete.
 *
 * @param conn Connection handle
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return SB_OK if complete, SB_ERR_TIMEOUT if timed out
 */
SBError sb_wait_query(SBConnection* conn, uint32_t timeout_ms);

/**
 * Get result from completed async query.
 *
 * @param conn Connection handle
 * @param result_out Pointer to receive result handle
 * @return SB_OK on success, error code on failure
 */
SBError sb_get_async_result(SBConnection* conn, SBResult** result_out);

/**
 * Cancel running async query.
 *
 * @param conn Connection handle
 * @return SB_OK on success, error code on failure
 */
SBError sb_cancel_query(SBConnection* conn);
```

### 9.2 Notifications/Subscriptions

```c
/**
 * Subscribe to a notification channel.
 *
 * @param conn Connection handle
 * @param channel Channel name
 * @param callback Callback function for notifications
 * @param user_data User data passed to callback
 * @param sub_out Pointer to receive subscription handle
 * @return SB_OK on success, error code on failure
 */
typedef void (*SBNotifyCallback)(const char* channel, const char* payload,
                                  void* user_data);

SBError sb_subscribe(SBConnection* conn, const char* channel,
                     SBNotifyCallback callback, void* user_data,
                     SBSubscription** sub_out);

/**
 * Unsubscribe from notification channel.
 */
SBError sb_unsubscribe(SBSubscription* sub);

/**
 * Send notification on channel.
 *
 * @param conn Connection handle
 * @param channel Channel name
 * @param payload Notification payload
 * @return SB_OK on success, error code on failure
 */
SBError sb_notify(SBConnection* conn, const char* channel, const char* payload);
```

---

## 10. Metadata and Information

### 10.1 Database Information

```c
/**
 * Get list of databases.
 *
 * @param conn Connection handle
 * @param result_out Pointer to receive result with database names
 * @return SB_OK on success, error code on failure
 */
SBError sb_list_databases(SBConnection* conn, SBResult** result_out);

/**
 * Get list of schemas.
 *
 * @param conn Connection handle
 * @param result_out Pointer to receive result with schema names
 * @return SB_OK on success, error code on failure
 */
SBError sb_list_schemas(SBConnection* conn, SBResult** result_out);

/**
 * Get list of tables in schema.
 *
 * @param conn Connection handle
 * @param schema Schema name (NULL for current)
 * @param result_out Pointer to receive result
 * @return SB_OK on success, error code on failure
 */
SBError sb_list_tables(SBConnection* conn, const char* schema,
                       SBResult** result_out);

/**
 * Get column information for table.
 *
 * @param conn Connection handle
 * @param schema Schema name
 * @param table Table name
 * @param result_out Pointer to receive result
 * @return SB_OK on success, error code on failure
 */
SBError sb_describe_table(SBConnection* conn, const char* schema,
                          const char* table, SBResult** result_out);

/**
 * Get index information for table.
 */
SBError sb_describe_indexes(SBConnection* conn, const char* schema,
                            const char* table, SBResult** result_out);
```

---

## 11. Utility Functions

### 11.1 String Escaping

```c
/**
 * Escape string for use in SQL.
 *
 * @param conn Connection handle (for encoding context)
 * @param input Input string
 * @param input_length Input length (-1 for null-terminated)
 * @param output Buffer for output
 * @param output_size Size of output buffer
 * @return Length of escaped string, or required size if buffer too small
 */
size_t sb_escape_string(SBConnection* conn, const char* input,
                        int input_length, char* output, size_t output_size);

/**
 * Escape identifier (table/column name) for use in SQL.
 */
size_t sb_escape_identifier(SBConnection* conn, const char* input,
                            char* output, size_t output_size);
```

### 11.2 Type Conversion

```c
/**
 * Get type name from type code.
 *
 * @param type Type code
 * @return Type name string
 */
const char* sb_type_name(SBType type);

/**
 * Parse type name to type code.
 *
 * @param name Type name string
 * @return Type code, or SB_TYPE_NULL if unknown
 */
SBType sb_type_from_name(const char* name);

/**
 * Convert value to string representation.
 *
 * @param value Value to convert
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Length of string, or required size if buffer too small
 */
size_t sb_value_to_string(const SBValue* value, char* buffer, size_t buffer_size);
```

### 11.3 Error Handling

```c
/**
 * Get error message for error code.
 *
 * @param error Error code
 * @return Error message string
 */
const char* sb_error_string(SBError error);

/**
 * Check if error is retryable.
 *
 * @param error Error code
 * @return 1 if retryable, 0 otherwise
 */
int sb_error_is_retryable(SBError error);
```

---

## 12. Thread Safety

### 12.1 Thread Safety Guarantees

| Operation | Thread Safety |
|-----------|---------------|
| `sb_connect` | Safe (creates new connection) |
| Operations on same connection | NOT safe (serialize access) |
| Operations on different connections | Safe |
| `sb_result_*` on same result | NOT safe |
| Global functions (`sb_type_name`, etc.) | Safe |

### 12.2 Connection Per Thread Pattern

```c
/* Recommended pattern: one connection per thread */
__thread SBConnection* thread_conn = NULL;

SBConnection* get_thread_connection(void) {
    if (thread_conn == NULL) {
        SBConnectOptions opts;
        sb_connect_options_init(&opts);
        /* Configure opts... */
        sb_connect(&opts, &thread_conn);
    }
    return thread_conn;
}
```

---

## 13. Memory Management

### 13.1 Ownership Rules

| Function | Ownership |
|----------|-----------|
| `sb_connect` | Caller owns returned connection |
| `sb_prepare` | Caller owns returned statement |
| `sb_execute` | Caller owns returned result |
| `sb_get_string` | Borrowed (valid until next row or free) |
| `sb_get_blob` | Borrowed (valid until next row or free) |
| `sb_result_column_desc` | Borrowed (valid until result freed) |
| `sb_error_message` | Borrowed (valid until next error) |

### 13.2 Cleanup Functions

```c
/* Always pair with allocation */
sb_connect(...)      → sb_disconnect(conn)
sb_prepare(...)      → sb_statement_free(stmt)
sb_execute(...)      → sb_result_free(result)
sb_batch_create(...) → sb_batch_free(batch)
sb_subscribe(...)    → sb_unsubscribe(sub)
```

---

## 14. Example Usage

### 14.1 Complete Example

```c
#include <stdio.h>
#include <scratchbird_client.h>

int main() {
    SBConnection* conn = NULL;
    SBResult* result = NULL;
    SBError err;

    /* Connect */
    SBConnectOptions opts;
    sb_connect_options_init(&opts);
    opts.host = "localhost";
    opts.port = 3092;
    opts.database = "mydb";
    opts.user = "admin";
    opts.password = "secret";

    err = sb_connect(&opts, &conn);
    if (err != SB_OK) {
        fprintf(stderr, "Connect failed: %s\n", sb_error_string(err));
        return 1;
    }

    /* Create table */
    err = sb_execute_update(conn,
        "CREATE TABLE IF NOT EXISTS users ("
        "  id INTEGER PRIMARY KEY,"
        "  name VARCHAR(100),"
        "  email VARCHAR(100)"
        ")", NULL);

    if (err != SB_OK) {
        fprintf(stderr, "Create table failed: %s\n", sb_error_message(conn));
        goto cleanup;
    }

    /* Insert with prepared statement */
    SBStatement* stmt;
    err = sb_prepare(conn, "INSERT INTO users (id, name, email) VALUES ($1, $2, $3)", &stmt);
    if (err != SB_OK) goto cleanup;

    sb_begin(conn);
    for (int i = 1; i <= 10; i++) {
        sb_bind_int(stmt, 1, i);
        sb_bind_string(stmt, 2, "User", -1);
        sb_bind_string(stmt, 3, "user@example.com", -1);

        int64_t affected;
        err = sb_statement_execute_update(stmt, &affected);
        if (err != SB_OK) {
            sb_rollback(conn);
            goto cleanup;
        }
    }
    sb_commit(conn);
    sb_statement_free(stmt);

    /* Query */
    err = sb_execute(conn, "SELECT id, name, email FROM users", &result);
    if (err != SB_OK) goto cleanup;

    printf("Users:\n");
    while (sb_result_next(result) == SB_OK) {
        int32_t id;
        sb_get_int(result, 0, &id);
        const char* name = sb_get_string_z(result, 1);
        const char* email = sb_get_string_z(result, 2);

        printf("  %d: %s <%s>\n", id, name, email);
    }
    sb_result_free(result);
    result = NULL;

cleanup:
    if (result) sb_result_free(result);
    if (conn) sb_disconnect(conn);
    return (err == SB_OK) ? 0 : 1;
}
```

---

## 15. Build and Link

### 15.1 Compilation

```bash
# Compile with client library
gcc -o myapp myapp.c -lscratchbird_client

# With pkg-config
gcc -o myapp myapp.c $(pkg-config --cflags --libs scratchbird_client)
```

### 15.2 pkg-config File

```
# /usr/lib/pkgconfig/scratchbird_client.pc
prefix=/usr
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: ScratchBird Client
Description: ScratchBird database client library
Version: 1.0.0
Libs: -L${libdir} -lscratchbird_client
Cflags: -I${includedir}
```

---

## 16. Version Information

```c
/**
 * Get library version.
 *
 * @return Version string (e.g., "1.0.0")
 */
const char* sb_library_version(void);

/**
 * Get library version as integer.
 *
 * @return Version as (major * 10000 + minor * 100 + patch)
 */
int sb_library_version_number(void);

/* Version macros */
#define SB_VERSION_MAJOR 1
#define SB_VERSION_MINOR 0
#define SB_VERSION_PATCH 0
#define SB_VERSION_STRING "1.0.0"
#define SB_VERSION_NUMBER 10000
```
