# ScratchBird Network Layer Specification
## Comprehensive Technical Specification for Network and Connection Management

## Overview

ScratchBird's network layer combines Firebird's efficient connection pooling,
PostgreSQL's robust protocol handling, MySQL's result caching, and SQL Server's
connection context preservation. Multi-protocol support is implemented by the
listener + parser pool control plane; historical "Y-Valve" references in this
document map to that listener/pool layer.

**Scope Note:** TDS/MSSQL protocol support is post-gold; any TDS references are forward-looking.

## 1. Enhanced Connection Pooling

### 1.1 Connection Pool Architecture

```c
// Master connection pool structure
typedef struct sb_connection_pool {
    // Pool identification
    UUID            pool_uuid;          // Pool UUID v7
    char            pool_name[64];      // Pool name
    PoolType        pool_type;          // Pool classification
    
    // Pool sizing
    uint32_t        pool_size_min;      // Minimum connections
    uint32_t        pool_size_max;      // Maximum connections
    uint32_t        pool_size_current;  // Current size
    uint32_t        pool_size_target;   // Target size (adaptive)
    
    // Connection arrays
    PooledConnection** pool_connections; // Array of connections
    uint32_t        pool_active_count;  // Active connections
    uint32_t        pool_idle_count;    // Idle connections
    
    // Timing configuration
    uint32_t        pool_lifetime_ms;   // Connection lifetime
    uint32_t        pool_idle_timeout_ms; // Idle timeout
    uint32_t        pool_validation_interval_ms; // Validation interval
    uint32_t        pool_acquire_timeout_ms; // Acquire timeout
    
    // Statement caching (PostgreSQL-style)
    bool            pool_statement_cache_enabled;
    uint32_t        pool_statement_cache_size;
    StatementCache* pool_statement_cache;
    
    // Result caching (MySQL-style)
    bool            pool_result_cache_enabled;
    uint64_t        pool_result_cache_size_bytes;
    ResultCache*    pool_result_cache;
    
    // Statistics
    PoolStatistics  pool_stats;
    
    // Synchronization
    pthread_mutex_t pool_mutex;
    pthread_cond_t  pool_not_empty;
    pthread_cond_t  pool_not_full;
    
    // Background maintenance
    pthread_t       pool_maintenance_thread;
    bool            pool_shutdown;
} SBConnectionPool;

// Pool types for different workloads
typedef enum pool_type {
    POOL_TRANSACTIONAL,     // OLTP workloads
    POOL_ANALYTICAL,        // OLAP workloads
    POOL_BULK,             // Bulk operations
    POOL_READONLY,         // Read-only replicas
    POOL_FEDERATED,        // Federated connections
    POOL_MAINTENANCE       // Admin operations
} PoolType;

// Individual pooled connection
typedef struct pooled_connection {
    // Connection identity
    uint64_t        conn_id;            // Connection ID
    UUID            conn_uuid;          // Connection UUID v7
    
    // Network details
    int             conn_socket_fd;     // Socket descriptor
    SSL*            conn_ssl;           // SSL context if encrypted
    
    // Protocol information
    ProtocolType    conn_protocol;      // Protocol type
    void*           conn_protocol_state; // Protocol-specific state
    
    // Database connection
    DatabaseHandle* conn_database;      // Database handle
    SessionHandle*  conn_session;       // Session handle
    TransactionId   conn_transaction;   // Current transaction
    
    // Connection state
    ConnectionState conn_state;         // Current state
    time_t          conn_created;       // Creation time
    time_t          conn_last_used;     // Last use time
    uint64_t        conn_use_count;     // Total uses
    
    // Connection properties
    char            conn_database_name[128];
    char            conn_username[128];
    char            conn_client_encoding[32];
    uint32_t        conn_client_version;
    
    // Prepared statements cache
    PreparedStmtCache* conn_stmt_cache;
    
    // Connection context (SQL Server-style)
    ConnectionContext* conn_context;
    
    // Health and validation
    time_t          conn_last_validated;
    uint32_t        conn_validation_failures;
    bool            conn_marked_bad;
    
    // Metrics
    uint64_t        conn_bytes_sent;
    uint64_t        conn_bytes_received;
    uint64_t        conn_queries_executed;
    double          conn_total_exec_time;
} PooledConnection;
```

### 1.2 Connection Acquisition and Return

```c
// Get connection from pool with advanced features
PooledConnection* pool_acquire_connection(
    SBConnectionPool* pool,
    const ConnectionRequest* request,
    uint32_t timeout_ms)
{
    pthread_mutex_lock(&pool->pool_mutex);
    
    struct timespec wait_until;
    clock_gettime(CLOCK_REALTIME, &wait_until);
    add_milliseconds(&wait_until, timeout_ms);
    
    while (true) {
        // Step 1: Try to find suitable existing connection
        PooledConnection* conn = find_suitable_connection(pool, request);
        
        if (conn != NULL) {
            // Found suitable connection
            if (validate_connection(conn)) {
                // Connection is good
                conn->conn_state = CONN_STATE_ACTIVE;
                conn->conn_last_used = time(NULL);
                conn->conn_use_count++;
                pool->pool_active_count++;
                pool->pool_idle_count--;
                
                // Update statistics
                pool->pool_stats.acquisitions++;
                pool->pool_stats.acquisition_time_total += 
                    time_since(start_time);
                
                pthread_mutex_unlock(&pool->pool_mutex);
                return conn;
            } else {
                // Connection is bad - remove it
                remove_connection(pool, conn);
                pool->pool_stats.bad_connections++;
            }
        }
        
        // Step 2: Try to create new connection if pool not full
        if (pool->pool_size_current < pool->pool_size_max) {
            pthread_mutex_unlock(&pool->pool_mutex);
            
            conn = create_new_connection(request);
            
            pthread_mutex_lock(&pool->pool_mutex);
            
            if (conn != NULL) {
                add_to_pool(pool, conn);
                conn->conn_state = CONN_STATE_ACTIVE;
                pool->pool_active_count++;
                pool->pool_size_current++;
                
                pool->pool_stats.connections_created++;
                
                pthread_mutex_unlock(&pool->pool_mutex);
                return conn;
            }
        }
        
        // Step 3: Wait for available connection
        int wait_result = pthread_cond_timedwait(
            &pool->pool_not_empty,
            &pool->pool_mutex,
            &wait_until);
        
        if (wait_result == ETIMEDOUT) {
            pool->pool_stats.acquisition_timeouts++;
            pthread_mutex_unlock(&pool->pool_mutex);
            return NULL;  // Timeout
        }
    }
}

// Return connection to pool
void pool_return_connection(
    SBConnectionPool* pool,
    PooledConnection* conn)
{
    pthread_mutex_lock(&pool->pool_mutex);
    
    // Reset connection state
    if (conn->conn_transaction != INVALID_TRANSACTION_ID) {
        // Rollback any active transaction
        rollback_transaction(conn->conn_transaction);
        conn->conn_transaction = INVALID_TRANSACTION_ID;
    }
    
    // Clear temporary tables
    clear_temp_tables(conn->conn_session);
    
    // Reset session variables to defaults
    reset_session_variables(conn->conn_session);
    
    // Clear prepared statement cache if needed
    if (pool->pool_statement_cache_enabled) {
        // Keep frequently used statements
        prune_statement_cache(conn->conn_stmt_cache);
    }
    
    // Update connection state
    conn->conn_state = CONN_STATE_IDLE;
    conn->conn_last_used = time(NULL);
    pool->pool_active_count--;
    pool->pool_idle_count++;
    
    // Signal waiting threads
    pthread_cond_signal(&pool->pool_not_empty);
    
    // Update statistics
    pool->pool_stats.returns++;
    
    pthread_mutex_unlock(&pool->pool_mutex);
}
```

### 1.3 Connection Validation and Health Checking

```c
// Validate connection health
bool validate_connection(PooledConnection* conn) {
    // Quick validation - check socket
    if (!socket_is_alive(conn->conn_socket_fd)) {
        return false;
    }
    
    // Check last validation time
    time_t now = time(NULL);
    if (now - conn->conn_last_validated < VALIDATION_INTERVAL) {
        return true;  // Recently validated
    }
    
    // Perform protocol-specific validation
    bool valid = false;
    
    switch (conn->conn_protocol) {
        case PROTOCOL_POSTGRESQL:
            valid = validate_postgresql_connection(conn);
            break;
        case PROTOCOL_MYSQL:
            valid = validate_mysql_connection(conn);
            break;
        case PROTOCOL_FIREBIRD:
            valid = validate_firebird_connection(conn);
            break;
        case PROTOCOL_TDS:  // post-gold
            valid = validate_tds_connection(conn);
            break;
        case PROTOCOL_NATIVE:
            valid = validate_native_connection(conn);
            break;
    }
    
    if (valid) {
        conn->conn_last_validated = now;
        conn->conn_validation_failures = 0;
    } else {
        conn->conn_validation_failures++;
        if (conn->conn_validation_failures >= MAX_VALIDATION_FAILURES) {
            conn->conn_marked_bad = true;
        }
    }
    
    return valid;
}

// PostgreSQL-style validation
bool validate_postgresql_connection(PooledConnection* conn) {
    // Send simple query
    PGMessage msg;
    msg.type = 'Q';  // Query
    msg.query = "SELECT 1";
    
    if (!send_pg_message(conn->conn_socket_fd, &msg)) {
        return false;
    }
    
    // Wait for response
    PGMessage response;
    if (!receive_pg_message(conn->conn_socket_fd, &response, 
                          VALIDATION_TIMEOUT_MS)) {
        return false;
    }
    
    return response.type == 'T';  // RowDescription expected
}
```

## 2. Listener/Pool Router Enhancements

### 2.1 Smart Routing and Load Balancing

```c
// Enhanced listener/pool router with smart routing
typedef struct listener_router {
    // Router identity
    UUID            router_uuid;        // Router UUID
    char            router_name[64];    // Router name
    
    // Connection pools per type
    SBConnectionPool* pools[POOL_TYPE_COUNT];
    
    // Routing rules
    RoutingRule*    routing_rules;      // Array of rules
    uint32_t        rule_count;         // Number of rules
    
    // Load balancing
    LoadBalancer*   load_balancer;      // Load balancer
    
    // Protocol handlers
    ProtocolHandler* handlers[PROTOCOL_COUNT];
    
    // Translation cache
    TranslationCache* translation_cache;
    
    // Statistics
    RouterStatistics stats;
} ListenerRouter;

// Routing rule for smart query routing
typedef struct routing_rule {
    RuleType        rule_type;          // Type of rule
    
    // Conditions
    QueryType       query_type;         // SELECT, INSERT, etc.
    char*           table_pattern;      // Table name pattern
    char*           schema_pattern;     // Schema pattern
    uint32_t        estimated_cost;     // Cost threshold
    
    // Actions
    PoolType        target_pool;        // Target pool type
    ServerNode*     target_server;      // Specific server
    uint32_t        priority;           // Rule priority
    
    // Statistics
    uint64_t        matches;            // Times matched
    uint64_t        redirects;          // Times redirected
} RoutingRule;

// Route query to appropriate backend
PooledConnection* listener_route_query(
    ListenerRouter* router,
    const ParsedQuery* query,
    const SessionContext* session)
{
    // Step 1: Check routing rules
    RoutingRule* rule = find_matching_rule(router, query);
    
    if (rule != NULL) {
        // Apply routing rule
        PoolType pool_type = rule->target_pool;
        
        // Update statistics
        rule->matches++;
        
        // Get connection from specified pool
        return pool_acquire_connection(
            router->pools[pool_type],
            create_connection_request(session),
            DEFAULT_TIMEOUT);
    }
    
    // Step 2: Default routing based on query type
    PoolType pool_type = POOL_TRANSACTIONAL;  // Default
    
    if (query->type == QUERY_SELECT && !query->for_update) {
        // Read query - can go to replica
        if (session->in_transaction) {
            // Must use same connection
            pool_type = POOL_TRANSACTIONAL;
        } else {
            // Can use read replica
            pool_type = POOL_READONLY;
        }
    } else if (query->type == QUERY_COPY || 
               query->type == QUERY_BULK_INSERT) {
        // Bulk operation
        pool_type = POOL_BULK;
    } else if (is_analytical_query(query)) {
        // Complex analytical query
        pool_type = POOL_ANALYTICAL;
    }
    
    // Step 3: Load balancing within pool
    return load_balance_connection(
        router->pools[pool_type],
        router->load_balancer,
        session);
}
```

### 2.2 Protocol Translation Cache

```c
// Cache for translated queries between protocols
typedef struct translation_cache {
    // Cache configuration
    uint64_t        cache_size_bytes;   // Maximum cache size
    uint32_t        cache_entries_max;  // Maximum entries
    uint32_t        cache_ttl_seconds;  // Entry TTL
    
    // Cache storage
    HashTable*      cache_table;        // Hash table
    LRUList*        cache_lru;          // LRU list
    
    // Statistics
    uint64_t        cache_hits;         // Cache hits
    uint64_t        cache_misses;       // Cache misses
    uint64_t        cache_evictions;    // Evictions
    
    // Synchronization
    pthread_rwlock_t cache_lock;        // Read-write lock
} TranslationCache;

// Cache entry for translated query
typedef struct translation_entry {
    // Key
    uint64_t        entry_hash;         // Query hash
    ProtocolType    source_protocol;    // Source protocol
    ProtocolType    target_protocol;    // Target protocol
    char*           source_query;       // Original query
    
    // Value
    char*           translated_query;   // Translated query
    BLR*            compiled_blr;        // Compiled BLR
    QueryPlan*      cached_plan;        // Cached plan
    
    // Metadata
    time_t          created_time;       // Creation time
    time_t          last_accessed;      // Last access
    uint64_t        access_count;       // Access count
    uint32_t        translation_cost;   // Translation cost
} TranslationEntry;

// Look up or translate query
const char* translate_query_cached(
    TranslationCache* cache,
    const char* query,
    ProtocolType source,
    ProtocolType target)
{
    // Calculate hash
    uint64_t hash = hash_query(query, source, target);
    
    // Try read lock first
    pthread_rwlock_rdlock(&cache->cache_lock);
    
    TranslationEntry* entry = hash_table_get(cache->cache_table, hash);
    
    if (entry != NULL) {
        // Cache hit
        entry->last_accessed = time(NULL);
        entry->access_count++;
        cache->cache_hits++;
        
        // Move to front of LRU
        lru_touch(cache->cache_lru, entry);
        
        const char* result = entry->translated_query;
        pthread_rwlock_unlock(&cache->cache_lock);
        return result;
    }
    
    pthread_rwlock_unlock(&cache->cache_lock);
    
    // Cache miss - need to translate
    cache->cache_misses++;
    
    // Perform translation
    char* translated = translate_query(query, source, target);
    
    // Add to cache with write lock
    pthread_rwlock_wrlock(&cache->cache_lock);
    
    // Check size limit
    if (cache->cache_entries_max > 0 && 
        hash_table_size(cache->cache_table) >= cache->cache_entries_max) {
        // Evict LRU entry
        TranslationEntry* lru = lru_remove_tail(cache->cache_lru);
        hash_table_remove(cache->cache_table, lru->entry_hash);
        free_translation_entry(lru);
        cache->cache_evictions++;
    }
    
    // Create new entry
    entry = create_translation_entry(query, translated, source, target);
    hash_table_put(cache->cache_table, hash, entry);
    lru_add_head(cache->cache_lru, entry);
    
    pthread_rwlock_unlock(&cache->cache_lock);
    
    return translated;
}
```

## 3. Protocol Implementation

### 3.1 Multi-Protocol Support

```c
// Protocol handler interface
typedef struct protocol_handler {
    ProtocolType    protocol_type;      // Protocol identifier
    
    // Connection lifecycle
    Status (*handle_startup)(PooledConnection* conn, 
                           const StartupMessage* msg);
    Status (*handle_authentication)(PooledConnection* conn);
    Status (*handle_ssl_negotiation)(PooledConnection* conn);
    
    // Message handling
    Status (*receive_message)(PooledConnection* conn, 
                             ProtocolMessage* msg);
    Status (*send_message)(PooledConnection* conn, 
                          const ProtocolMessage* msg);
    
    // Query execution
    Status (*handle_query)(PooledConnection* conn, 
                          const char* query);
    Status (*handle_parse)(PooledConnection* conn,
                          const char* stmt_name,
                          const char* query);
    Status (*handle_bind)(PooledConnection* conn,
                         const char* portal_name,
                         const char* stmt_name,
                         const ParamValue* params);
    Status (*handle_execute)(PooledConnection* conn,
                            const char* portal_name,
                            uint32_t max_rows);
    
    // Result handling
    Status (*send_row_description)(PooledConnection* conn,
                                  const RowDescription* desc);
    Status (*send_data_row)(PooledConnection* conn,
                           const DataRow* row);
    Status (*send_command_complete)(PooledConnection* conn,
                                   const char* tag);
    
    // Error handling
    Status (*send_error)(PooledConnection* conn,
                        const ErrorInfo* error);
    
    // Protocol-specific features
    void* (*get_protocol_state)(PooledConnection* conn);
    Status (*handle_special_command)(PooledConnection* conn,
                                   const SpecialCommand* cmd);
} ProtocolHandler;
```

### 3.2 PostgreSQL Protocol Implementation

```c
// PostgreSQL protocol state
typedef struct pg_protocol_state {
    // Protocol version
    uint32_t        protocol_version;   // 3.0 for modern PostgreSQL
    
    // Connection parameters
    HashMap*        parameters;         // Server parameters
    
    // Transaction state
    char            transaction_state;  // I=Idle, T=Transaction, E=Error
    
    // Prepared statements
    HashMap*        prepared_statements;
    HashMap*        portals;
    
    // Extended query state
    bool            in_extended_query;
    char*           current_portal;
    
    // Copy state
    bool            in_copy;
    CopyDirection   copy_direction;
    CopyFormat      copy_format;
} PGProtocolState;

// Handle PostgreSQL query
Status handle_postgresql_query(
    PooledConnection* conn,
    const char* query)
{
    PGProtocolState* state = conn->conn_protocol_state;
    
    // Parse query
    ParsedQuery* parsed = parse_sql(query, SQL_DIALECT_POSTGRESQL);
    
    // Check for PostgreSQL-specific syntax
    if (has_returning_clause(parsed)) {
        handle_returning_clause(conn, parsed);
    }
    
    // Convert to BLR
    BLR* blr = convert_to_blr(parsed);
    
    // Execute via engine
    ExecutionResult* result = execute_blr(blr, conn->conn_session);
    
    // Send results in PostgreSQL format
    send_pg_row_description(conn, result->row_desc);
    
    for (DataRow* row : result->rows) {
        send_pg_data_row(conn, row);
    }
    
    send_pg_command_complete(conn, result->command_tag);
    
    return STATUS_OK;
}
```

### 3.3 Connection Multiplexing

```c
// Multiplexed connection for multiple logical connections
typedef struct multiplexed_connection {
    // Physical connection
    int             socket_fd;          // Single TCP connection
    SSL*            ssl_context;        // SSL if encrypted
    
    // Logical connections
    LogicalConn**   logical_conns;      // Array of logical
    uint32_t        logical_count;      // Number of logical
    uint32_t        logical_max;        // Maximum logical
    
    // Stream management
    uint32_t        next_stream_id;     // Next stream ID
    StreamTable*    active_streams;     // Active streams
    
    // Flow control
    uint32_t        window_size;        // Flow control window
    uint32_t        bytes_in_flight;    // Unacked bytes
    
    // Compression
    bool            compression_enabled;
    CompressionCtx* compression_ctx;
} MultiplexedConnection;

// Logical connection over multiplexed physical
typedef struct logical_connection {
    uint32_t        stream_id;          // Stream identifier
    MultiplexedConnection* physical;    // Physical connection
    
    // Logical state
    SessionHandle*  session;            // Session
    TransactionId   transaction;        // Transaction
    
    // Buffer for this stream
    CircularBuffer* send_buffer;        // Send buffer
    CircularBuffer* recv_buffer;        // Receive buffer
    
    // Flow control
    uint32_t        stream_window;      // Stream window
    bool            blocked;            // Flow blocked
} LogicalConnection;

// Send message over multiplexed connection
Status multiplex_send_message(
    LogicalConnection* logical,
    const Message* msg)
{
    // Create frame header
    FrameHeader header;
    header.stream_id = logical->stream_id;
    header.frame_type = FRAME_DATA;
    header.flags = 0;
    header.length = msg->length;
    
    // Check flow control
    if (logical->physical->bytes_in_flight + msg->length > 
        logical->physical->window_size) {
        // Would exceed window - buffer
        buffer_message(logical->send_buffer, msg);
        logical->blocked = true;
        return STATUS_BLOCKED;
    }
    
    // Compress if enabled
    if (logical->physical->compression_enabled) {
        compress_message(logical->physical->compression_ctx, msg);
        header.flags |= FLAG_COMPRESSED;
    }
    
    // Send frame
    send_frame(logical->physical->socket_fd, &header, msg->data);
    
    // Update flow control
    logical->physical->bytes_in_flight += msg->length;
    
    return STATUS_OK;
}
```

## 4. Network Optimizations

### 4.1 Zero-Copy Send/Receive

```c
// Zero-copy network operations
typedef struct zero_copy_buffer {
    // Memory mapping
    void*           mapped_memory;      // Mapped memory region
    size_t          mapped_size;        // Size of mapping
    
    // Ring buffer for zero-copy
    uint32_t        ring_size;          // Ring buffer size
    uint32_t        read_pos;           // Read position
    uint32_t        write_pos;          // Write position
    
    // Kernel bypass (if available)
    bool            kernel_bypass;      // Using kernel bypass
    void*           bypass_context;     // DPDK/XDP context
} ZeroCopyBuffer;

// Send data with zero-copy
ssize_t zero_copy_send(
    int socket_fd,
    ZeroCopyBuffer* buffer,
    size_t length)
{
    if (buffer->kernel_bypass) {
        // Use kernel bypass (DPDK/XDP)
        return bypass_send(buffer->bypass_context, 
                          buffer->mapped_memory + buffer->read_pos,
                          length);
    } else {
        // Use sendfile for zero-copy
        off_t offset = buffer->read_pos;
        ssize_t sent = sendfile(socket_fd, 
                               buffer->mapped_memory,
                               &offset,
                               length);
        
        if (sent > 0) {
            buffer->read_pos = (buffer->read_pos + sent) % 
                              buffer->ring_size;
        }
        
        return sent;
    }
}
```

### 4.2 Compression

```c
// Compression context for network traffic
typedef struct compression_context {
    CompressionType type;               // Compression algorithm
    
    // Zstd compression (preferred)
    ZSTD_CCtx*      zstd_compress;      // Compression context
    ZSTD_DCtx*      zstd_decompress;    // Decompression context
    ZSTD_CDict*     zstd_dict;          // Shared dictionary
    
    // Statistics
    uint64_t        bytes_uncompressed;
    uint64_t        bytes_compressed;
    double          compression_ratio;
} CompressionContext;

// Compress message
size_t compress_message(
    CompressionContext* ctx,
    const uint8_t* input,
    size_t input_size,
    uint8_t* output,
    size_t output_capacity)
{
    size_t compressed_size;
    
    switch (ctx->type) {
        case COMPRESS_ZSTD:
            if (ctx->zstd_dict != NULL) {
                // Use dictionary compression
                compressed_size = ZSTD_compress_usingCDict(
                    ctx->zstd_compress,
                    output, output_capacity,
                    input, input_size,
                    ctx->zstd_dict);
            } else {
                // Standard compression
                compressed_size = ZSTD_compressCCtx(
                    ctx->zstd_compress,
                    output, output_capacity,
                    input, input_size,
                    3);  // Compression level
            }
            break;
            
        case COMPRESS_LZ4:
            compressed_size = LZ4_compress_default(
                input, output,
                input_size, output_capacity);
            break;
            
        case COMPRESS_SNAPPY:
            snappy_compress(input, input_size,
                          output, &compressed_size);
            break;
    }
    
    // Update statistics
    ctx->bytes_uncompressed += input_size;
    ctx->bytes_compressed += compressed_size;
    ctx->compression_ratio = (double)ctx->bytes_compressed / 
                           ctx->bytes_uncompressed;
    
    return compressed_size;
}
```

## 5. Result Caching

### 5.1 Query Result Cache

```c
// Result cache for frequently accessed data
typedef struct result_cache {
    // Cache configuration
    uint64_t        cache_size_bytes;   // Maximum size
    uint32_t        cache_ttl_ms;       // TTL in milliseconds
    
    // Cache storage
    HashTable*      cache_entries;      // Hash table
    SkipList*       expiry_list;        // Sorted by expiry
    
    // Cache invalidation
    InvalidationList* invalidation_list; // Tables to watch
    uint64_t        invalidation_counter; // Invalidation version
    
    // Statistics
    CacheStatistics stats;
    
    // Synchronization
    pthread_rwlock_t cache_lock;
} ResultCache;

// Cache entry
typedef struct cache_entry {
    // Key
    uint64_t        query_hash;         // Query hash
    char*           query_text;         // Query text
    ParamValue*     parameters;         // Parameters if any
    
    // Result
    ResultSet*      result_set;         // Cached result
    uint32_t        result_size;        // Size in bytes
    
    // Metadata
    time_t          created_time;       // Creation time
    time_t          expiry_time;        // Expiry time
    uint64_t        access_count;       // Access count
    uint64_t        invalidation_version; // Version for invalidation
    
    // Cost tracking
    double          computation_cost;   // Cost to compute
    double          savings_total;      // Total cost saved
} CacheEntry;

// Check cache for query result
ResultSet* check_result_cache(
    ResultCache* cache,
    const char* query,
    const ParamValue* params)
{
    uint64_t hash = hash_query_with_params(query, params);
    
    pthread_rwlock_rdlock(&cache->cache_lock);
    
    CacheEntry* entry = hash_table_get(cache->cache_entries, hash);
    
    if (entry != NULL) {
        // Check if still valid
        time_t now = time(NULL);
        
        if (now < entry->expiry_time &&
            entry->invalidation_version == cache->invalidation_counter) {
            // Cache hit
            entry->access_count++;
            cache->stats.hits++;
            cache->stats.bytes_served += entry->result_size;
            
            ResultSet* result = copy_result_set(entry->result_set);
            pthread_rwlock_unlock(&cache->cache_lock);
            return result;
        } else {
            // Expired or invalidated
            hash_table_remove(cache->cache_entries, hash);
            skip_list_remove(cache->expiry_list, entry);
            free_cache_entry(entry);
            cache->stats.evictions++;
        }
    }
    
    cache->stats.misses++;
    pthread_rwlock_unlock(&cache->cache_lock);
    return NULL;
}
```

## 6. Connection Migration

### 6.1 Transparent Connection Migration

```c
// Connection migration for failover
typedef struct connection_migration {
    // Migration state
    MigrationState  state;              // Current state
    
    // Source and target
    PooledConnection* source_conn;      // Source connection
    PooledConnection* target_conn;      // Target connection
    
    // Session state to migrate
    SessionSnapshot* session_snapshot;  // Session state
    TransactionSnapshot* txn_snapshot;  // Transaction state
    
    // Prepared statements
    PreparedStmt**  prepared_stmts;     // Statements to migrate
    uint32_t        stmt_count;         // Number of statements
    
    // Temporary objects
    TempObject**    temp_objects;       // Temp tables, etc.
    uint32_t        temp_count;         // Number of temp objects
} ConnectionMigration;

// Migrate connection transparently
Status migrate_connection(
    PooledConnection* source,
    PooledConnection* target)
{
    ConnectionMigration migration;
    migration.source_conn = source;
    migration.target_conn = target;
    migration.state = MIGRATION_STARTING;
    
    // Step 1: Capture session state
    migration.session_snapshot = capture_session_state(source->conn_session);
    
    // Step 2: Capture transaction state if active
    if (source->conn_transaction != INVALID_TRANSACTION_ID) {
        migration.txn_snapshot = capture_transaction_state(
            source->conn_transaction);
    }
    
    // Step 3: Capture prepared statements
    migration.prepared_stmts = get_all_prepared_statements(
        source->conn_stmt_cache);
    
    // Step 4: Apply state to target
    restore_session_state(target->conn_session, 
                         migration.session_snapshot);
    
    if (migration.txn_snapshot != NULL) {
        restore_transaction_state(target, migration.txn_snapshot);
    }
    
    // Step 5: Re-prepare statements
    for (uint32_t i = 0; i < migration.stmt_count; i++) {
        prepare_statement(target, migration.prepared_stmts[i]);
    }
    
    // Step 6: Recreate temporary objects
    for (uint32_t i = 0; i < migration.temp_count; i++) {
        recreate_temp_object(target, migration.temp_objects[i]);
    }
    
    migration.state = MIGRATION_COMPLETE;
    
    return STATUS_OK;
}
```

## 7. Security and Encryption

### 7.1 TLS/SSL Support

```c
// TLS configuration
typedef struct tls_config {
    // Certificates
    char*           cert_file;          // Server certificate
    char*           key_file;           // Private key
    char*           ca_file;            // CA certificate
    
    // Protocol settings
    uint32_t        min_protocol_version; // Minimum TLS version
    uint32_t        max_protocol_version; // Maximum TLS version
    char*           cipher_list;        // Allowed ciphers
    
    // Client certificates
    bool            require_client_cert; // Require client cert
    bool            verify_client_cert;  // Verify client cert
    
    // Session resumption
    bool            session_resumption;  // Enable resumption
    uint32_t        session_cache_size;  // Session cache size
    uint32_t        session_timeout;     // Session timeout
} TLSConfig;

// Negotiate TLS
Status negotiate_tls(
    PooledConnection* conn,
    TLSConfig* config)
{
    // Create SSL context
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    
    // Configure protocol versions
    SSL_CTX_set_min_proto_version(ctx, config->min_protocol_version);
    SSL_CTX_set_max_proto_version(ctx, config->max_protocol_version);
    
    // Load certificates
    SSL_CTX_use_certificate_file(ctx, config->cert_file, SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, config->key_file, SSL_FILETYPE_PEM);
    
    // Set cipher list
    SSL_CTX_set_cipher_list(ctx, config->cipher_list);
    
    // Configure client certificates if required
    if (config->require_client_cert) {
        SSL_CTX_set_verify(ctx, 
                          SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                          verify_callback);
        SSL_CTX_load_verify_locations(ctx, config->ca_file, NULL);
    }
    
    // Create SSL connection
    conn->conn_ssl = SSL_new(ctx);
    SSL_set_fd(conn->conn_ssl, conn->conn_socket_fd);
    
    // Perform handshake
    int result = SSL_accept(conn->conn_ssl);
    
    if (result <= 0) {
        int ssl_error = SSL_get_error(conn->conn_ssl, result);
        return handle_ssl_error(ssl_error);
    }
    
    return STATUS_OK;
}
```

## 8. Monitoring and Diagnostics

### 8.1 Connection Pool Monitoring

```c
// Pool statistics
typedef struct pool_statistics {
    // Connection metrics
    uint64_t        total_connections;  // Total created
    uint64_t        active_connections; // Currently active
    uint64_t        idle_connections;   // Currently idle
    uint64_t        failed_connections; // Failed to create
    
    // Usage metrics
    uint64_t        acquisitions;       // Total acquisitions
    uint64_t        returns;           // Total returns
    uint64_t        acquisition_timeouts; // Timeouts
    uint64_t        validation_failures; // Validation failures
    
    // Performance metrics
    double          avg_acquisition_time; // Average acquire time
    double          avg_connection_lifetime; // Average lifetime
    double          avg_queries_per_connection; // Queries per conn
    
    // Cache metrics
    uint64_t        stmt_cache_hits;    // Statement cache hits
    uint64_t        stmt_cache_misses;  // Statement cache misses
    uint64_t        result_cache_hits;  // Result cache hits
    uint64_t        result_cache_misses; // Result cache misses
} PoolStatistics;

// Get pool statistics
PoolStatistics* get_pool_statistics(SBConnectionPool* pool) {
    PoolStatistics* stats = allocate(sizeof(PoolStatistics));
    
    pthread_mutex_lock(&pool->pool_mutex);
    
    // Copy current statistics
    memcpy(stats, &pool->pool_stats, sizeof(PoolStatistics));
    
    // Calculate derived metrics
    stats->active_connections = pool->pool_active_count;
    stats->idle_connections = pool->pool_idle_count;
    
    if (stats->acquisitions > 0) {
        stats->avg_acquisition_time = 
            pool->pool_stats.acquisition_time_total / 
            stats->acquisitions;
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
    
    return stats;
}
```

## 9. Integration with Listener/Pool Control Plane

### 9.1 Listener/Pool Connection Management

```c
// Listener/pool integration point
typedef struct listener_connection_manager {
    // Connection pools
    SBConnectionPool** pools;           // Array of pools
    uint32_t        pool_count;         // Number of pools
    
    // Protocol handlers
    ProtocolHandler** handlers;         // Protocol handlers
    
    // Routing engine
    ListenerRouter* router;             // Router
    
    // Global statistics
    GlobalNetworkStats* global_stats;
} ListenerConnectionManager;

// Handle incoming connection through listener/pool
Status listener_handle_connection(
    ListenerConnectionManager* manager,
    int client_fd,
    struct sockaddr* client_addr)
{
    // Detect protocol
    ProtocolType protocol = detect_protocol(client_fd);
    
    // Get protocol handler
    ProtocolHandler* handler = manager->handlers[protocol];
    
    // Handle startup
    StartupMessage startup;
    handler->handle_startup(NULL, &startup);
    
    // Route to appropriate pool
    ConnectionRequest request;
    request.database = startup.database;
    request.username = startup.username;
    request.protocol = protocol;
    
    PooledConnection* conn = listener_route_query(
        manager->router,
        NULL,  // No query yet
        &request);
    
    if (conn == NULL) {
        send_error(client_fd, "No connections available");
        return STATUS_ERROR;
    }
    
    // Attach client to connection
    conn->conn_socket_fd = client_fd;
    
    // Handle authentication
    handler->handle_authentication(conn);
    
    // Enter main message loop
    return handle_client_messages(conn, handler);
}
```

## Implementation Timeline

Following the ProjectPlan phases:

1. **Phase 19**: Basic network protocol and server
2. **Phase 25**: Listener/pool framework implementation (legacy Y-Valve)
3. **Enhancement**: Connection pooling and caching
4. **Future**: Multiplexing, migration, and advanced features

This specification provides a complete blueprint for ScratchBird's network layer, combining proven techniques with innovative enhancements for modern distributed systems.
