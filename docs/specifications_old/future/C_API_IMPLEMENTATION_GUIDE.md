# C API Implementation Guide

---

## IMPLEMENTATION STATUS: NOT IMPLEMENTED - FUTURE/DESIGN ONLY

**IMPORTANT:** This is a guide for implementing a C API that **DOES NOT CURRENTLY EXIST**. This document is aspirational/planning only.

**Current Status:**
- No C API implementation exists
- This guide is for future development work
- See C_API_SPECIFICATION.md in this directory for the design

---

## Implementation Strategy

### Phase 1: Core Infrastructure (Alpha 1.02)

```c
// Minimal viable API for Alpha
// File: src/api/core.c

#include "scratchbird_internal.h"

// Global environment
static struct {
    atomic_bool     initialized;
    sb_config_t     config;
    MemoryPool*     global_pool;
    ThreadPool*     thread_pool;
    ErrorManager*   error_manager;
    LogManager*     log_manager;
} g_environment = {0};

// Initialize environment
sb_status_t sb_init(sb_environment_t* env, const sb_config_t* config) {
    // Ensure single initialization
    bool expected = false;
    if (!atomic_compare_exchange_strong(&g_environment.initialized, 
                                       &expected, true)) {
        return SB_ALREADY_EXISTS;
    }
    
    // Apply configuration
    if (config) {
        g_environment.config = *config;
    } else {
        sb_apply_default_config(&g_environment.config);
    }
    
    // Initialize subsystems
    g_environment.global_pool = memory_pool_create(
        g_environment.config.page_cache_size
    );
    
    g_environment.thread_pool = thread_pool_create(
        g_environment.config.max_threads
    );
    
    g_environment.error_manager = error_manager_create();
    g_environment.log_manager = log_manager_create(
        g_environment.config.log_level
    );
    
    // Create environment handle
    *env = allocate_handle(HANDLE_ENVIRONMENT);
    (*env)->data = &g_environment;
    
    return SB_SUCCESS;
}
```

### Phase 2: Database Operations (Alpha 1.03)

```c
// Database handle implementation
// File: src/api/database.c

typedef struct sb_database_impl {
    char            path[PATH_MAX];
    Database*       engine_db;      // Internal engine database
    PageCache*      page_cache;
    sb_page_size_t  page_size;
    RWLock          lock;
    atomic_int      ref_count;
    bool            exclusive;
    
    // Statistics
    struct {
        atomic_uint64_t reads;
        atomic_uint64_t writes;
        atomic_uint64_t transactions;
    } stats;
} sb_database_impl_t;

sb_status_t sb_create_database(sb_environment_t env,
                              const char* path,
                              const sb_create_options_t* options,
                              sb_database_t* database) {
    // Validate parameters
    if (!env || !path || !database) {
        return SB_INVALID_PARAMETER;
    }
    
    // Check if exists
    if (!options->overwrite && file_exists(path)) {
        return SB_ALREADY_EXISTS;
    }
    
    // Create database file
    DatabaseFile* db_file = create_database_file(
        path,
        options->page_size,
        options->initial_pages
    );
    
    if (!db_file) {
        return SB_ERROR;
    }
    
    // Initialize database structures
    initialize_system_tables(db_file);
    initialize_tip_pages(db_file);
    initialize_pip_pages(db_file);
    
    // Create handle
    sb_database_impl_t* impl = calloc(1, sizeof(sb_database_impl_t));
    strncpy(impl->path, path, PATH_MAX - 1);
    impl->page_size = options->page_size;
    impl->engine_db = engine_open_database(db_file);
    impl->page_cache = create_page_cache(options->page_size);
    
    *database = allocate_handle(HANDLE_DATABASE);
    (*database)->data = impl;
    
    return SB_SUCCESS;
}
```

### Phase 3: Connection Management (Alpha 1.04)

```c
// Connection implementation
// File: src/api/connection.c

typedef struct sb_connection_impl {
    sb_database_impl_t* database;
    Session*            engine_session;
    Transaction*        current_txn;
    sb_protocol_t       protocol;
    
    // Connection state
    sb_connection_state_t state;
    char                username[128];
    char                current_schema[128];
    
    // Prepared statements
    HashTable*          prepared_stmts;
    
    // Error handling
    sb_error_info_t     last_error;
    sb_error_callback_t error_callback;
    void*              error_user_data;
    
    // Statistics
    ConnectionStats     stats;
} sb_connection_impl_t;

sb_status_t sb_connect(sb_database_t database,
                      const sb_connect_options_t* options,
                      sb_connection_t* connection) {
    sb_database_impl_t* db = (sb_database_impl_t*)database->data;
    
    // Create session in engine
    Session* session = engine_create_session(
        db->engine_db,
        options->username,
        options->password
    );
    
    if (!session) {
        return SB_CONNECTION_FAILED;
    }
    
    // Create connection handle
    sb_connection_impl_t* impl = calloc(1, sizeof(sb_connection_impl_t));
    impl->database = db;
    impl->engine_session = session;
    impl->state = SB_CONN_IDLE;
    impl->protocol = options->protocol;
    strncpy(impl->username, options->username, 127);
    
    // Set session parameters
    if (options->search_path) {
        engine_set_search_path(session, options->search_path);
    }
    
    *connection = allocate_handle(HANDLE_CONNECTION);
    (*connection)->data = impl;
    
    atomic_fetch_add(&db->ref_count, 1);
    
    return SB_SUCCESS;
}
```

### Phase 4: Statement Execution (Alpha 1.05)

```c
// Statement execution implementation
// File: src/api/statement.c

typedef struct sb_statement_impl {
    sb_connection_impl_t* connection;
    char*                sql;
    BLRData*             blr;          // Compiled BLR
    QueryPlan*           plan;         // Execution plan
    
    // Parameters
    uint32_t             param_count;
    ParamDescriptor*     param_descs;
    ParamValue*          param_values;
    
    // Results
    uint32_t             column_count;
    ColumnDescriptor*    column_descs;
    
    // State
    bool                 prepared;
    bool                 executed;
    
} sb_statement_impl_t;

sb_status_t sb_prepare(sb_connection_t connection,
                       const char* sql,
                       sb_statement_t* statement) {
    sb_connection_impl_t* conn = (sb_connection_impl_t*)connection->data;
    
    // Parse SQL to BLR
    BLRData* blr = parse_sql_to_blr(sql, conn->protocol);
    if (!blr) {
        set_error(conn, "SQL parse error");
        return SB_ERROR;
    }
    
    // Generate execution plan
    QueryPlan* plan = generate_plan(blr, conn->engine_session);
    if (!plan) {
        set_error(conn, "Plan generation failed");
        return SB_ERROR;
    }
    
    // Create statement handle
    sb_statement_impl_t* impl = calloc(1, sizeof(sb_statement_impl_t));
    impl->connection = conn;
    impl->sql = strdup(sql);
    impl->blr = blr;
    impl->plan = plan;
    impl->prepared = true;
    
    // Extract parameter info
    extract_param_info(blr, &impl->param_count, &impl->param_descs);
    impl->param_values = calloc(impl->param_count, sizeof(ParamValue));
    
    // Extract result info
    extract_result_info(plan, &impl->column_count, &impl->column_descs);
    
    *statement = allocate_handle(HANDLE_STATEMENT);
    (*statement)->data = impl;
    
    return SB_SUCCESS;
}

sb_status_t sb_execute(sb_statement_t statement, sb_result_t* result) {
    sb_statement_impl_t* stmt = (sb_statement_impl_t*)statement->data;
    sb_connection_impl_t* conn = stmt->connection;
    
    // Validate state
    if (!stmt->prepared) {
        return SB_INVALID_PARAMETER;
    }
    
    // Execute plan
    ResultSet* rs = engine_execute_plan(
        conn->engine_session,
        stmt->plan,
        stmt->param_values,
        stmt->param_count
    );
    
    if (!rs) {
        set_error(conn, "Execution failed");
        return SB_ERROR;
    }
    
    // Create result handle if needed
    if (result && stmt->column_count > 0) {
        sb_result_impl_t* impl = calloc(1, sizeof(sb_result_impl_t));
        impl->statement = stmt;
        impl->result_set = rs;
        impl->current_row = -1;
        
        *result = allocate_handle(HANDLE_RESULT);
        (*result)->data = impl;
    } else {
        // No result set (INSERT/UPDATE/DELETE)
        engine_free_result_set(rs);
    }
    
    stmt->executed = true;
    conn->state = SB_CONN_IDLE;
    
    return SB_SUCCESS;
}
```

## Memory Management

```c
// Handle management system
// File: src/api/handles.c

typedef enum {
    HANDLE_ENVIRONMENT,
    HANDLE_DATABASE,
    HANDLE_CONNECTION,
    HANDLE_TRANSACTION,
    HANDLE_STATEMENT,
    HANDLE_RESULT,
    HANDLE_CURSOR,
    HANDLE_BLOB,
    HANDLE_EVENT,
    HANDLE_BACKUP
} handle_type_t;

typedef struct handle {
    uint32_t        magic;          // Validation magic
    handle_type_t   type;           // Handle type
    atomic_int      ref_count;      // Reference count
    void*          data;            // Implementation data
    RWLock         lock;            // Thread safety
} handle_t;

#define HANDLE_MAGIC 0x5342484E  // "SBHN"

static handle_t* allocate_handle(handle_type_t type) {
    handle_t* h = calloc(1, sizeof(handle_t));
    h->magic = HANDLE_MAGIC;
    h->type = type;
    h->ref_count = 1;
    rwlock_init(&h->lock);
    return h;
}

static sb_status_t validate_handle(void* handle, handle_type_t expected_type) {
    if (!handle) return SB_INVALID_PARAMETER;
    
    handle_t* h = (handle_t*)handle;
    if (h->magic != HANDLE_MAGIC) return SB_INVALID_PARAMETER;
    if (h->type != expected_type) return SB_TYPE_MISMATCH;
    
    return SB_SUCCESS;
}

static void release_handle(handle_t* handle) {
    if (atomic_fetch_sub(&handle->ref_count, 1) == 1) {
        // Last reference - clean up
        switch (handle->type) {
            case HANDLE_CONNECTION:
                cleanup_connection((sb_connection_impl_t*)handle->data);
                break;
            case HANDLE_STATEMENT:
                cleanup_statement((sb_statement_impl_t*)handle->data);
                break;
            // ... other types
        }
        
        rwlock_destroy(&handle->lock);
        free(handle->data);
        handle->magic = 0;  // Invalidate
        free(handle);
    }
}
```

## Error Handling

```c
// Error management
// File: src/api/errors.c

typedef struct sb_error_impl {
    sb_error_info_t info;
    char            message_buffer[1024];
    char            detail_buffer[1024];
    char            hint_buffer[256];
    sb_error_impl_t* cause;
} sb_error_impl_t;

static thread_local sb_error_impl_t* tls_last_error = NULL;

static void set_error(sb_connection_impl_t* conn, const char* message) {
    sb_error_impl_t* error = calloc(1, sizeof(sb_error_impl_t));
    
    error->info.code = SB_ERROR;
    error->info.sqlstate = "58000";  // System error
    error->info.message = error->message_buffer;
    strncpy(error->message_buffer, message, 1023);
    
    // Get location info
    error->info.file = __FILE__;
    error->info.line = __LINE__;
    
    // Chain to previous error if exists
    if (tls_last_error) {
        error->cause = tls_last_error;
    }
    
    tls_last_error = error;
    conn->last_error = error->info;
    
    // Call error callback if set
    if (conn->error_callback) {
        conn->error_callback((sb_connection_t)conn, 
                            (sb_error_t)error,
                            conn->error_user_data);
    }
}

sb_error_t sb_last_error(sb_connection_t connection) {
    sb_connection_impl_t* conn = (sb_connection_impl_t*)connection->data;
    return (sb_error_t)&conn->last_error;
}
```

## Thread Safety

```c
// Thread-safe operations
// File: src/api/thread_safety.c

// Connection pool implementation
typedef struct sb_pool_impl {
    sb_database_impl_t* database;
    Queue<sb_connection_t> available;
    Set<sb_connection_t> in_use;
    
    uint32_t min_connections;
    uint32_t max_connections;
    uint32_t current_connections;
    
    Mutex mutex;
    CondVar not_empty;
    
    bool shutdown;
    Thread maintenance_thread;
} sb_pool_impl_t;

sb_status_t sb_pool_acquire(sb_pool_t pool, sb_connection_t* connection) {
    sb_pool_impl_t* p = (sb_pool_impl_t*)pool->data;
    
    unique_lock lock(p->mutex);
    
    // Wait for available connection
    while (p->available.empty() && !p->shutdown) {
        if (p->current_connections < p->max_connections) {
            // Create new connection
            sb_connection_t conn;
            sb_status_t status = create_pooled_connection(p, &conn);
            if (status == SB_SUCCESS) {
                p->in_use.insert(conn);
                *connection = conn;
                return SB_SUCCESS;
            }
        }
        
        // Wait for connection to be returned
        p->not_empty.wait(lock);
    }
    
    if (p->shutdown) {
        return SB_CANCELLED;
    }
    
    // Get connection from pool
    *connection = p->available.pop();
    p->in_use.insert(*connection);
    
    // Test connection if configured
    if (p->test_on_borrow) {
        if (!test_connection(*connection)) {
            // Connection dead, create new one
            close_connection(*connection);
            return create_pooled_connection(p, connection);
        }
    }
    
    return SB_SUCCESS;
}
```

## Testing Framework

```c
// Unit tests for C API
// File: tests/api/test_core.c

#include <scratchbird.h>
#include <assert.h>

void test_environment_init() {
    sb_environment_t env;
    sb_status_t status;
    
    // Test initialization
    status = sb_init(&env, NULL);
    assert(status == SB_SUCCESS);
    
    // Test double initialization
    sb_environment_t env2;
    status = sb_init(&env2, NULL);
    assert(status == SB_ALREADY_EXISTS);
    
    // Test shutdown
    status = sb_shutdown(env);
    assert(status == SB_SUCCESS);
}

void test_database_operations() {
    sb_environment_t env;
    sb_database_t db;
    
    sb_init(&env, NULL);
    
    // Test database creation
    sb_create_options_t opts = {
        .page_size = SB_PAGE_16K,
        .overwrite = true
    };
    
    sb_status_t status = sb_create_database(env, "test.db", &opts, &db);
    assert(status == SB_SUCCESS);
    
    // Test database info
    sb_database_info_t info;
    status = sb_database_info(db, SB_INFO_GENERAL, &info, sizeof(info));
    assert(status == SB_SUCCESS);
    assert(info.page_size == SB_PAGE_16K);
    
    sb_close_database(db);
    sb_shutdown(env);
}

// Run all tests
int main() {
    test_environment_init();
    test_database_operations();
    // ... more tests
    
    printf("All tests passed!\n");
    return 0;
}
```

## Performance Optimizations

```c
// Zero-copy result handling
// File: src/api/results_optimized.c

typedef struct sb_result_impl {
    sb_statement_impl_t* statement;
    
    // Current page of results
    struct {
        uint8_t*    data;           // Raw page data
        size_t      size;           // Page size
        uint32_t    row_count;      // Rows in page
        uint32_t    current_row;    // Current row index
        RowOffset*  row_offsets;    // Offsets to each row
    } page;
    
    // Prefetch buffer
    struct {
        uint8_t*    data;
        size_t      size;
        bool        ready;
    } prefetch;
    
    // Column accessors (zero-copy)
    ColumnAccessor* accessors;
    
} sb_result_impl_t;

// Zero-copy string access
sb_status_t sb_get_string(sb_result_t result,
                         uint32_t column_index,
                         const char** value,
                         size_t* length) {
    sb_result_impl_t* res = (sb_result_impl_t*)result->data;
    
    // Get column accessor
    ColumnAccessor* acc = &res->accessors[column_index];
    
    // Point directly to data in page
    *value = (const char*)(res->page.data + acc->offset);
    *length = acc->length;
    
    // No copying!
    return SB_SUCCESS;
}
```

## Integration with Engine

```c
// Bridge to internal engine
// File: src/api/engine_bridge.c

// Convert public API types to engine types
static EngineType api_type_to_engine(sb_type_t api_type) {
    switch (api_type) {
        case SB_TYPE_INT32:     return ENGINE_INT32;
        case SB_TYPE_INT64:     return ENGINE_INT64;
        case SB_TYPE_VARCHAR:   return ENGINE_VARCHAR;
        // ... more mappings
    }
}

// Execute through engine
static ResultSet* engine_execute_plan(Session* session,
                                     QueryPlan* plan,
                                     ParamValue* params,
                                     uint32_t param_count) {
    // Set up execution context
    ExecutionContext ctx;
    ctx.session = session;
    ctx.transaction = session->current_transaction;
    ctx.params = params;
    ctx.param_count = param_count;
    
    // Execute plan
    return execute_query_plan(&ctx, plan);
}
```

This implementation provides a solid foundation for the C API that can be built incrementally through the Alpha phases.