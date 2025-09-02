# Database Internals: Network Layer Details

## Table of Contents
1. [FirebirdSQL Network Layer](#firebirdsql-network-layer)
2. [PostgreSQL Network Layer](#postgresql-network-layer)  
3. [MySQL/MariaDB Network Layer](#mysqlmariadb-network-layer)
4. [Microsoft SQL Server Network Layer](#microsoft-sql-server-network-layer)

---

# FirebirdSQL Network Layer

## Connection Pooling

### Firebird Connection Pool Implementation
```c
// Connection pool structure
typedef struct connection_pool {
    PooledConnection*   pool_connections;      // Array of pooled connections
    ULONG              pool_size;             // Maximum pool size
    ULONG              pool_count;            // Current connections in pool
    ULONG              pool_active;           // Active connections
    ULONG              pool_idle_timeout;     // Idle timeout in seconds
    ULONG              pool_lifetime;         // Connection lifetime
    Mutex              pool_mutex;            // Pool synchronization
    Semaphore          pool_semaphore;        // Available connection signal
    ThreadPool*        pool_workers;          // Worker threads
} ConnectionPool;

// Pooled connection
typedef struct pooled_connection {
    rem_port*          conn_port;             // Network port
    Database*          conn_database;         // Database handle
    Attachment*        conn_attachment;       // Attachment handle
    time_t             conn_created;          // Creation time
    time_t             conn_last_used;        // Last use time
    ULONG              conn_use_count;        // Usage counter
    bool               conn_in_use;           // Currently in use
    bool               conn_valid;            // Connection valid
    struct pooled_connection* conn_next;      // Next in list
} PooledConnection;

// Get connection from pool
PooledConnection* POOL_get_connection(
    ConnectionPool* pool,
    const char*     database_name,
    const char*     user_name,
    const char*     password)
{
    MutexLockGuard guard(pool->pool_mutex);
    
    // Look for available connection
    for (ULONG i = 0; i < pool->pool_count; i++) {
        PooledConnection* conn = &pool->pool_connections[i];
        
        if (!conn->conn_in_use && conn->conn_valid) {
            // Check if connection matches requirements
            if (strcmp(conn->conn_database->dbb_filename, database_name) == 0) {
                // Validate connection is still alive
                if (POOL_validate_connection(conn)) {
                    // Check lifetime and idle timeout
                    time_t now = time(NULL);
                    
                    if (now - conn->conn_created > pool->pool_lifetime ||
                        now - conn->conn_last_used > pool->pool_idle_timeout) {
                        // Connection expired - close it
                        POOL_close_connection(conn);
                        continue;
                    }
                    
                    // Mark as in use
                    conn->conn_in_use = true;
                    conn->conn_last_used = now;
                    conn->conn_use_count++;
                    pool->pool_active++;
                    
                    return conn;
                } else {
                    // Connection dead - remove from pool
                    POOL_close_connection(conn);
                }
            }
        }
    }
    
    // No available connection - create new if pool not full
    if (pool->pool_count < pool->pool_size) {
        PooledConnection* conn = POOL_create_connection(
            database_name, user_name, password);
        
        if (conn) {
            // Add to pool
            pool->pool_connections[pool->pool_count++] = *conn;
            conn->conn_in_use = true;
            pool->pool_active++;
            
            return conn;
        }
    }
    
    // Pool full - wait for available connection
    guard.release();
    
    if (pool->pool_semaphore.wait(POOL_WAIT_TIMEOUT)) {
        // Retry
        return POOL_get_connection(pool, database_name, user_name, password);
    }
    
    return NULL;  // Timeout
}

// Return connection to pool
void POOL_return_connection(
    ConnectionPool*     pool,
    PooledConnection*   conn)
{
    MutexLockGuard guard(pool->pool_mutex);
    
    // Reset connection state
    if (conn->conn_attachment) {
        // Rollback any active transaction
        if (conn->conn_attachment->att_transaction) {
            TRA_rollback(conn->conn_attachment->att_transaction, false);
        }
        
        // Clear temporary tables
        POOL_cleanup_connection(conn);
    }
    
    // Mark as available
    conn->conn_in_use = false;
    conn->conn_last_used = time(NULL);
    pool->pool_active--;
    
    // Signal waiting threads
    pool->pool_semaphore.signal();
}

// Connection validation
bool POOL_validate_connection(PooledConnection* conn)
{
    // Send ping packet
    PACKET packet;
    packet.p_operation = op_ping;
    
    if (!REMOTE_send_packet(conn->conn_port, &packet)) {
        return false;
    }
    
    // Wait for response
    if (!REMOTE_receive_packet(conn->conn_port, &packet)) {
        return false;
    }
    
    return packet.p_operation == op_response;
}
```

## SSL/TLS Configuration

### Firebird SSL Implementation
```c
// SSL configuration
typedef struct ssl_config {
    bool            ssl_enabled;           // SSL enabled
    SSL_METHOD*     ssl_method;           // SSL method (TLS 1.2/1.3)
    SSL_CTX*        ssl_context;          // SSL context
    char*           ssl_certificate;      // Server certificate path
    char*           ssl_private_key;      // Private key path
    char*           ssl_ca_certificate;   // CA certificate path
    char*           ssl_cipher_list;      // Allowed ciphers
    bool            ssl_verify_client;    // Require client certificate
    ULONG           ssl_verify_depth;     // Certificate chain depth
} SSLConfig;

// Initialize SSL
void SSL_initialize(SSLConfig* config)
{
    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    // Create SSL context
    config->ssl_method = TLS_server_method();  // Support TLS 1.2+
    config->ssl_context = SSL_CTX_new(config->ssl_method);
    
    if (!config->ssl_context) {
        ERR_error("Failed to create SSL context");
    }
    
    // Set minimum TLS version
    SSL_CTX_set_min_proto_version(config->ssl_context, TLS1_2_VERSION);
    
    // Load server certificate
    if (SSL_CTX_use_certificate_file(config->ssl_context,
                                    config->ssl_certificate,
                                    SSL_FILETYPE_PEM) <= 0) {
        ERR_error("Failed to load certificate");
    }
    
    // Load private key
    if (SSL_CTX_use_PrivateKey_file(config->ssl_context,
                                   config->ssl_private_key,
                                   SSL_FILETYPE_PEM) <= 0) {
        ERR_error("Failed to load private key");
    }
    
    // Verify private key
    if (!SSL_CTX_check_private_key(config->ssl_context)) {
        ERR_error("Private key does not match certificate");
    }
    
    // Set cipher list (prefer modern ciphers)
    const char* cipher_list = config->ssl_cipher_list ? 
        config->ssl_cipher_list :
        "ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES256-GCM-SHA384:"
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305:"
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256";
    
    if (SSL_CTX_set_cipher_list(config->ssl_context, cipher_list) != 1) {
        ERR_error("Failed to set cipher list");
    }
    
    // Configure client certificate verification
    if (config->ssl_verify_client) {
        SSL_CTX_set_verify(config->ssl_context,
                          SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                          SSL_verify_callback);
        
        // Load CA certificate
        if (SSL_CTX_load_verify_locations(config->ssl_context,
                                         config->ssl_ca_certificate,
                                         NULL) != 1) {
            ERR_error("Failed to load CA certificate");
        }
        
        SSL_CTX_set_verify_depth(config->ssl_context, config->ssl_verify_depth);
    }
    
    // Enable session caching
    SSL_CTX_set_session_cache_mode(config->ssl_context, SSL_SESS_CACHE_SERVER);
    SSL_CTX_sess_set_cache_size(config->ssl_context, 128);
}

// Establish SSL connection
rem_port* SSL_connect(
    rem_port*       port,
    SSLConfig*      config)
{
    // Create SSL structure
    port->port_ssl = SSL_new(config->ssl_context);
    if (!port->port_ssl) {
        return NULL;
    }
    
    // Set socket
    SSL_set_fd(port->port_ssl, port->port_handle);
    
    // Perform SSL handshake
    int ret = SSL_accept(port->port_ssl);
    if (ret <= 0) {
        int ssl_error = SSL_get_error(port->port_ssl, ret);
        
        switch (ssl_error) {
            case SSL_ERROR_ZERO_RETURN:
                ERR_post("SSL connection closed");
                break;
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                // Non-blocking I/O - retry
                return SSL_connect(port, config);
            default:
                ERR_post("SSL handshake failed");
        }
        
        SSL_free(port->port_ssl);
        port->port_ssl = NULL;
        return NULL;
    }
    
    // Verify client certificate if required
    if (config->ssl_verify_client) {
        X509* client_cert = SSL_get_peer_certificate(port->port_ssl);
        
        if (!client_cert) {
            ERR_post("No client certificate provided");
            SSL_free(port->port_ssl);
            return NULL;
        }
        
        // Verify certificate
        if (SSL_get_verify_result(port->port_ssl) != X509_V_OK) {
            ERR_post("Client certificate verification failed");
            X509_free(client_cert);
            SSL_free(port->port_ssl);
            return NULL;
        }
        
        X509_free(client_cert);
    }
    
    // Get negotiated protocol and cipher
    port->port_protocol_version = SSL_get_version(port->port_ssl);
    port->port_cipher = SSL_get_cipher_name(port->port_ssl);
    
    return port;
}
```

## Y-Valve Router Implementation

### Firebird Y-Valve Architecture
```c
// Y-Valve provider interface
typedef struct yvalve_provider {
    const char*     yvp_name;              // Provider name
    ProviderType    yvp_type;              // Provider type
    void*           yvp_handle;            // Provider handle
    
    // Provider functions
    ISC_STATUS (*yvp_attach_database)(ISC_STATUS*, const char*, isc_db_handle*, 
                                      USHORT, const UCHAR*);
    ISC_STATUS (*yvp_detach_database)(ISC_STATUS*, isc_db_handle*);
    ISC_STATUS (*yvp_start_transaction)(ISC_STATUS*, isc_tr_handle*, 
                                        USHORT, ...);
    ISC_STATUS (*yvp_commit_transaction)(ISC_STATUS*, isc_tr_handle*);
    ISC_STATUS (*yvp_rollback_transaction)(ISC_STATUS*, isc_tr_handle*);
    ISC_STATUS (*yvp_prepare_statement)(ISC_STATUS*, isc_db_handle*, 
                                        isc_tr_handle*, isc_stmt_handle*, 
                                        USHORT, const char*, USHORT, XSQLDA*);
    ISC_STATUS (*yvp_execute_statement)(ISC_STATUS*, isc_tr_handle*, 
                                        isc_stmt_handle*, USHORT, XSQLDA*);
} YValveProvider;

// Y-Valve router
typedef struct yvalve_router {
    YValveProvider**    yvr_providers;     // Array of providers
    ULONG              yvr_provider_count; // Number of providers
    ProviderType       yvr_default_provider; // Default provider
    Mutex              yvr_mutex;          // Router mutex
} YValveRouter;

// Route database attachment
ISC_STATUS YVALVE_attach_database(
    ISC_STATUS*     status_vector,
    const char*     database_name,
    isc_db_handle*  db_handle,
    USHORT          dpb_length,
    const UCHAR*    dpb)
{
    YValveRouter* router = get_yvalve_router();
    YValveProvider* provider = NULL;
    
    // Parse connection string to determine provider
    ProviderType provider_type = YVALVE_parse_provider(database_name);
    
    if (provider_type == PROVIDER_AUTO) {
        // Auto-detect provider
        provider_type = YVALVE_detect_provider(database_name);
    }
    
    // Get provider
    provider = YVALVE_get_provider(router, provider_type);
    
    if (!provider) {
        // Use default provider
        provider = YVALVE_get_provider(router, router->yvr_default_provider);
    }
    
    if (!provider) {
        return error(status_vector, "No provider available");
    }
    
    // Route to provider
    ISC_STATUS result = provider->yvp_attach_database(
        status_vector, database_name, db_handle, dpb_length, dpb);
    
    if (result == 0) {
        // Store provider info in handle
        YVALVE_store_provider_info(*db_handle, provider);
    }
    
    return result;
}

// Load provider
YValveProvider* YVALVE_load_provider(const char* provider_name)
{
    YValveProvider* provider = FB_NEW YValveProvider();
    
    // Load provider library
    char library_name[256];
    sprintf(library_name, "lib%s.so", provider_name);
    
    provider->yvp_handle = dlopen(library_name, RTLD_NOW | RTLD_LOCAL);
    
    if (!provider->yvp_handle) {
        delete provider;
        return NULL;
    }
    
    // Get provider functions
    provider->yvp_attach_database = (ISC_STATUS (*)(ISC_STATUS*, const char*, 
        isc_db_handle*, USHORT, const UCHAR*))
        dlsym(provider->yvp_handle, "isc_attach_database");
    
    provider->yvp_detach_database = (ISC_STATUS (*)(ISC_STATUS*, isc_db_handle*))
        dlsym(provider->yvp_handle, "isc_detach_database");
    
    provider->yvp_start_transaction = (ISC_STATUS (*)(ISC_STATUS*, 
        isc_tr_handle*, USHORT, ...))
        dlsym(provider->yvp_handle, "isc_start_transaction");
    
    // ... load other functions
    
    provider->yvp_name = provider_name;
    
    return provider;
}

// Provider detection
ProviderType YVALVE_detect_provider(const char* database_name)
{
    // Check for network protocol prefix
    if (strncmp(database_name, "inet://", 7) == 0 ||
        strncmp(database_name, "inet4://", 8) == 0 ||
        strncmp(database_name, "inet6://", 8) == 0) {
        return PROVIDER_REMOTE;
    }
    
    // Check for embedded prefix
    if (strncmp(database_name, "embedded:", 9) == 0) {
        return PROVIDER_EMBEDDED;
    }
    
    // Check for loopback
    if (strncmp(database_name, "xnet://", 7) == 0 ||
        strncmp(database_name, "localhost:", 10) == 0) {
        return PROVIDER_LOOPBACK;
    }
    
    // Check file extension
    const char* ext = strrchr(database_name, '.');
    if (ext) {
        if (strcmp(ext, ".fdb") == 0 || strcmp(ext, ".gdb") == 0) {
            // Local database file
            if (access(database_name, F_OK) == 0) {
                return PROVIDER_ENGINE13;  // Local engine
            }
        }
    }
    
    // Default to remote
    return PROVIDER_REMOTE;
}
```

---

# PostgreSQL Network Layer

## Connection Pooling

### PostgreSQL Connection Pooling (PgBouncer style)
```c
// Connection pool modes
typedef enum {
    POOL_MODE_SESSION,      // Pool sessions
    POOL_MODE_TRANSACTION,  // Pool transactions
    POOL_MODE_STATEMENT     // Pool statements
} PoolMode;

// Client connection state
typedef struct PgSocket {
    SocketState     state;          // Connection state
    PgPool         *pool;           // Associated pool
    PgSocket       *link;           // Server connection link
    
    SBuf            sbuf;           // Socket buffer
    IOState         io_state;       // I/O state
    
    time_t          connect_time;   // Connection time
    time_t          request_time;   // Last request time
    usec_t          query_start;    // Query start time
    
    PgAddr          remote_addr;    // Client address
    PgAddr          local_addr;     // Local address
    
    char           *client_encoding; // Client encoding
    char           *datestyle;      // Date style
    char           *timezone;       // Time zone
    
    bool            is_transaction; // In transaction?
    bool            ready;          // Ready for query?
    bool            suspended;      // Suspended?
    bool            admin_user;     // Admin connection?
} PgSocket;

// Server connection pool
typedef struct PgPool {
    struct List     head;           // List node
    char           *database;       // Database name
    char           *user;           // User name
    
    PgDatabase     *db;            // Database config
    PgUser         *user_obj;      // User config
    
    struct StatList active_client_list;  // Active clients
    struct StatList waiting_client_list; // Waiting clients
    struct StatList active_server_list;  // Active servers
    struct StatList idle_server_list;    // Idle servers
    struct StatList tested_server_list;  // Being tested
    struct StatList new_server_list;     // Newly connected
    
    int             pool_size;      // Current pool size
    int             min_pool_size;  // Minimum pool size
    int             res_pool_size;  // Reserve pool size
    
    VarCache        orig_vars;      // Original variables
    
    usec_t          last_lifetime_disconnect; // Last lifetime disconnect
} PgPool;

// Get server connection from pool
PgSocket *get_server_connection(PgSocket *client)
{
    PgPool *pool = client->pool;
    PgSocket *server = NULL;
    
    // Try to get idle server
    while (statlist_count(&pool->idle_server_list) > 0) {
        server = container_of(statlist_pop(&pool->idle_server_list),
                            PgSocket, head);
        
        // Check server lifetime
        if (server_lifetime > 0) {
            usec_t age = get_cached_time() - server->connect_time;
            if (age > server_lifetime) {
                disconnect_server(server, true, "server_lifetime");
                server = NULL;
                continue;
            }
        }
        
        // Check server idle time
        if (server_idle_timeout > 0) {
            usec_t idle = get_cached_time() - server->request_time;
            if (idle > server_idle_timeout) {
                disconnect_server(server, true, "server_idle_timeout");
                server = NULL;
                continue;
            }
        }
        
        // Test server connection
        if (server_check_delay > 0) {
            usec_t delay = get_cached_time() - server->request_time;
            if (delay > server_check_delay) {
                // Move to test list
                statlist_append(&pool->tested_server_list, &server->head);
                
                // Send test query
                if (!send_test_query(server)) {
                    disconnect_server(server, true, "test failed");
                    server = NULL;
                    continue;
                }
            }
        }
        
        break;
    }
    
    // No idle server - check pool limits
    if (!server) {
        int total = pool_server_count(pool);
        
        if (total < pool->pool_size) {
            // Can create new connection
            server = launch_new_server(pool);
        } else if (statlist_count(&pool->waiting_client_list) == 0 &&
                  pool->res_pool_size > 0 &&
                  total < pool->pool_size + pool->res_pool_size) {
            // Use reserve pool
            server = launch_new_server(pool);
        }
    }
    
    if (server) {
        // Link client and server
        server->link = client;
        client->link = server;
        
        // Move to active list
        statlist_append(&pool->active_server_list, &server->head);
        
        // Copy client variables to server
        varcache_apply(&client->vars, server);
    }
    
    return server;
}

// Return server to pool
void return_server_to_pool(PgSocket *server)
{
    PgPool *pool = server->pool;
    PgSocket *client = server->link;
    
    // Unlink
    server->link = NULL;
    if (client) {
        client->link = NULL;
    }
    
    // Check pool mode
    switch (pool_mode) {
        case POOL_MODE_SESSION:
            // Keep server assigned to client session
            if (client && !client->suspended) {
                // Client disconnected - close server too
                disconnect_server(server, false, "client disconnect");
                return;
            }
            break;
            
        case POOL_MODE_TRANSACTION:
            // Return to pool if not in transaction
            if (!server->is_transaction) {
                statlist_append(&pool->idle_server_list, &server->head);
                server->request_time = get_cached_time();
                
                // Process waiting clients
                process_waiting_clients(pool);
                return;
            }
            break;
            
        case POOL_MODE_STATEMENT:
            // Always return to pool
            statlist_append(&pool->idle_server_list, &server->head);
            server->request_time = get_cached_time();
            
            process_waiting_clients(pool);
            return;
    }
}
```

## SSL/TLS Configuration

### PostgreSQL SSL Implementation
```c
// SSL configuration
typedef struct pg_ssl_config {
    char       *ssl_cert_file;      // Server certificate
    char       *ssl_key_file;       // Server private key
    char       *ssl_ca_file;        // CA certificate
    char       *ssl_crl_file;       // Certificate revocation list
    char       *ssl_ciphers;        // Cipher list
    bool        ssl_prefer_server_ciphers; // Prefer server ciphers
    int         ssl_min_protocol_version;  // Minimum TLS version
    int         ssl_max_protocol_version;  // Maximum TLS version
    char       *ssl_dh_params_file; // DH parameters
    char       *ssl_ecdh_curve;     // ECDH curve
    bool        ssl_passphrase_command_supports_reload;
} pg_ssl_config;

// Initialize SSL context
static SSL_CTX *
initialize_ssl_context(pg_ssl_config *config)
{
    SSL_CTX *context;
    
    // Create SSL context
    context = SSL_CTX_new(TLS_method());
    if (!context) {
        ereport(FATAL,
               (errmsg("could not create SSL context: %s",
                      SSLerrfree(ERR_get_error()))));
    }
    
    // Set allowed protocols
    SSL_CTX_set_min_proto_version(context, config->ssl_min_protocol_version);
    SSL_CTX_set_max_proto_version(context, config->ssl_max_protocol_version);
    
    // Disable SSL session tickets for PFS
    SSL_CTX_set_options(context, SSL_OP_NO_TICKET);
    
    // Set cipher list
    if (config->ssl_ciphers && strlen(config->ssl_ciphers) > 0) {
        if (SSL_CTX_set_cipher_list(context, config->ssl_ciphers) != 1) {
            ereport(FATAL,
                   (errmsg("could not set SSL cipher list: %s",
                          SSLerrfree(ERR_get_error()))));
        }
    }
    
    // Prefer server cipher order
    if (config->ssl_prefer_server_ciphers) {
        SSL_CTX_set_options(context, SSL_OP_CIPHER_SERVER_PREFERENCE);
    }
    
    // Load server certificate
    if (SSL_CTX_use_certificate_chain_file(context, config->ssl_cert_file) != 1) {
        ereport(FATAL,
               (errmsg("could not load server certificate: %s",
                      SSLerrfree(ERR_get_error()))));
    }
    
    // Load private key
    if (SSL_CTX_use_PrivateKey_file(context, config->ssl_key_file,
                                   SSL_FILETYPE_PEM) != 1) {
        ereport(FATAL,
               (errmsg("could not load private key: %s",
                      SSLerrfree(ERR_get_error()))));
    }
    
    // Check private key matches certificate
    if (SSL_CTX_check_private_key(context) != 1) {
        ereport(FATAL,
               (errmsg("private key does not match certificate")));
    }
    
    // Set up DH parameters for ephemeral DH
    if (config->ssl_dh_params_file) {
        FILE *fp = fopen(config->ssl_dh_params_file, "r");
        if (fp) {
            DH *dh = PEM_read_DHparams(fp, NULL, NULL, NULL);
            fclose(fp);
            
            if (dh) {
                SSL_CTX_set_tmp_dh(context, dh);
                DH_free(dh);
            }
        }
    }
    
    // Set ECDH curve
    if (config->ssl_ecdh_curve) {
        EC_KEY *ecdh = EC_KEY_new_by_curve_name(
            OBJ_sn2nid(config->ssl_ecdh_curve));
        if (ecdh) {
            SSL_CTX_set_tmp_ecdh(context, ecdh);
            EC_KEY_free(ecdh);
        }
    }
    
    // Set up client certificate verification
    if (config->ssl_ca_file) {
        if (SSL_CTX_load_verify_locations(context, config->ssl_ca_file, NULL) != 1) {
            ereport(FATAL,
                   (errmsg("could not load CA certificate: %s",
                          SSLerrfree(ERR_get_error()))));
        }
        
        SSL_CTX_set_verify(context,
                          SSL_VERIFY_PEER |
                          SSL_VERIFY_FAIL_IF_NO_PEER_CERT |
                          SSL_VERIFY_CLIENT_ONCE,
                          verify_cb);
        
        // Load CRL if specified
        if (config->ssl_crl_file) {
            X509_STORE *store = SSL_CTX_get_cert_store(context);
            X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK |
                                       X509_V_FLAG_CRL_CHECK_ALL);
            
            if (!load_crl(store, config->ssl_crl_file)) {
                ereport(FATAL,
                       (errmsg("could not load CRL file")));
            }
        }
    }
    
    // Set session cache
    SSL_CTX_set_session_cache_mode(context, SSL_SESS_CACHE_OFF);
    
    return context;
}

// Perform SSL handshake
static int
secure_open_server(Port *port)
{
    int         r;
    int         err;
    SSL        *ssl;
    
    Assert(!port->ssl);
    Assert(!port->peer);
    
    // Create SSL structure
    if (!(ssl = SSL_new(SSL_context))) {
        ereport(COMMERROR,
               (errmsg("could not create SSL structure: %s",
                      SSLerrfree(ERR_get_error()))));
        return -1;
    }
    
    // Set socket
    if (!SSL_set_fd(ssl, port->sock)) {
        ereport(COMMERROR,
               (errmsg("could not set SSL socket: %s",
                      SSLerrfree(ERR_get_error()))));
        SSL_free(ssl);
        return -1;
    }
    
    // Set server name indication (SNI)
    SSL_set_tlsext_host_name(ssl, port->ssl_sni);
    
    // Perform handshake
aloop:
    ERR_clear_error();
    r = SSL_accept(ssl);
    if (r <= 0) {
        err = SSL_get_error(ssl, r);
        
        switch (err) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                // Would block - retry
                goto aloop;
                
            case SSL_ERROR_SYSCALL:
                if (r < 0) {
                    ereport(COMMERROR,
                           (errmsg("SSL accept error: %m")));
                } else {
                    ereport(COMMERROR,
                           (errmsg("SSL accept EOF")));
                }
                break;
                
            case SSL_ERROR_SSL:
                ereport(COMMERROR,
                       (errmsg("SSL accept error: %s",
                              SSLerrfree(ERR_get_error()))));
                break;
                
            case SSL_ERROR_ZERO_RETURN:
                ereport(COMMERROR,
                       (errmsg("SSL connection closed")));
                break;
                
            default:
                ereport(COMMERROR,
                       (errmsg("unrecognized SSL error code: %d", err)));
                break;
        }
        
        SSL_free(ssl);
        return -1;
    }
    
    // Store SSL info
    port->ssl = ssl;
    port->ssl_in_use = true;
    
    // Get peer certificate
    port->peer = SSL_get_peer_certificate(ssl);
    
    // Get protocol and cipher info
    port->ssl_protocol = SSL_get_version(ssl);
    port->ssl_cipher = SSL_get_cipher(ssl);
    port->ssl_bits = SSL_get_cipher_bits(ssl, NULL);
    
    // Verify client certificate if present
    if (port->peer) {
        if (SSL_get_verify_result(ssl) != X509_V_OK) {
            ereport(COMMERROR,
                   (errmsg("SSL certificate verification failed")));
            SSL_free(ssl);
            return -1;
        }
    }
    
    return 0;
}
```

---

# MySQL/MariaDB Network Layer

## Connection Pooling

### MySQL Thread Pool Implementation
```c
// Thread pool structure
typedef struct thread_pool {
    thread_group_t *thread_groups;     // Array of thread groups
    uint            thread_group_count; // Number of groups
    uint            thread_count;       // Total thread count
    uint            active_thread_count; // Active threads
    uint            idle_thread_timeout; // Idle timeout
    uint            max_threads;        // Maximum threads
    uint            oversubscribe;      // Oversubscription factor
} thread_pool_t;

// Thread group
typedef struct thread_group {
    mysql_mutex_t   mutex;              // Group mutex
    mysql_cond_t    cond;               // Condition variable
    worker_thread_t *threads;           // Worker threads
    uint            thread_count;       // Threads in group
    uint            active_thread_count; // Active threads
    connection_queue queue;             // Connection queue
    pollfd         *pollfd_array;      // Poll descriptors
    uint            pollfd_count;       // Number of descriptors
    bool            shutdown;           // Shutdown flag
    ulonglong       thread_creations;   // Thread creation count
    ulonglong       wakeups;            // Wakeup count
    ulonglong       stalled_count;      // Stall count
} thread_group_t;

// Worker thread
typedef struct worker_thread {
    thread_group_t *thread_group;      // Parent group
    THD            *thd;                // Thread descriptor
    pthread_t       pthread_id;         // System thread ID
    bool            woken;              // Woken flag
    ulonglong       event_count;        // Events processed
    struct timespec last_event_time;    // Last event time
} worker_thread_t;

// Handle new connection
void thread_pool_add_connection(THD *thd)
{
    thread_group_t *group;
    uint group_id;
    
    // Assign to thread group (round-robin or hash-based)
    group_id = thd->thread_id % thread_pool.thread_group_count;
    group = &thread_pool.thread_groups[group_id];
    
    mysql_mutex_lock(&group->mutex);
    
    // Add to connection queue
    queue_push(&group->queue, thd);
    
    // Wake up idle thread or create new one
    if (group->active_thread_count < group->thread_count) {
        // Wake idle thread
        mysql_cond_signal(&group->cond);
    } else if (group->thread_count < thread_pool.max_threads / 
              thread_pool.thread_group_count) {
        // Create new thread
        create_worker_thread(group);
    } else if (group->active_thread_count == group->thread_count &&
              thread_pool.oversubscribe > 0) {
        // All threads busy - check for stalls
        check_for_stalled_threads(group);
    }
    
    mysql_mutex_unlock(&group->mutex);
}

// Worker thread main loop
void *worker_thread_main(void *arg)
{
    worker_thread_t *worker = (worker_thread_t *)arg;
    thread_group_t *group = worker->thread_group;
    THD *thd;
    
    mysql_mutex_lock(&group->mutex);
    
    while (!group->shutdown) {
        // Get connection from queue
        thd = queue_pop(&group->queue);
        
        if (!thd) {
            // No work - wait for signal or timeout
            struct timespec timeout;
            set_timespec(&timeout, thread_pool.idle_thread_timeout);
            
            int ret = mysql_cond_timedwait(&group->cond, &group->mutex, &timeout);
            
            if (ret == ETIMEDOUT && 
                group->thread_count > MIN_THREAD_COUNT) {
                // Idle timeout - exit thread
                group->thread_count--;
                break;
            }
            
            continue;
        }
        
        // Process connection
        group->active_thread_count++;
        mysql_mutex_unlock(&group->mutex);
        
        // Handle client request
        process_client_request(thd);
        
        mysql_mutex_lock(&group->mutex);
        group->active_thread_count--;
        
        // Check if connection should be returned to pool
        if (thd->net.vio && !thd->killed) {
            // Add back to queue for next request
            queue_push(&group->queue, thd);
        } else {
            // Connection closed
            end_connection(thd);
        }
    }
    
    mysql_mutex_unlock(&group->mutex);
    
    return NULL;
}

// Monitor for stalled threads
void check_for_stalled_threads(thread_group_t *group)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    for (uint i = 0; i < group->thread_count; i++) {
        worker_thread_t *worker = &group->threads[i];
        
        if (worker->thd && worker->thd->proc_info) {
            // Check if thread is stalled
            long long diff = timespec_diff_ms(&now, &worker->last_event_time);
            
            if (diff > STALL_THRESHOLD_MS) {
                // Thread is stalled - create helper thread
                if (group->thread_count < thread_pool.max_threads / 
                    thread_pool.thread_group_count * 
                    (1 + thread_pool.oversubscribe)) {
                    
                    create_worker_thread(group);
                    group->stalled_count++;
                }
            }
        }
    }
}
```

## SSL/TLS Configuration

### MySQL SSL Implementation
```c
// SSL context structure
typedef struct st_ssl_context {
    SSL_CTX        *ssl_ctx;            // OpenSSL context
    SSL_METHOD     *ssl_method;         // SSL method
    char           *cert_file;          // Certificate file
    char           *key_file;           // Private key file
    char           *ca_file;            // CA certificate
    char           *ca_path;            // CA path
    char           *cipher_list;        // Cipher list
    char           *crl_file;           // CRL file
    char           *crl_path;           // CRL path
    ulong           tls_version;        // TLS version flags
    bool            verify_cert;        // Verify client cert
} SSL_CONTEXT;

// Initialize SSL
int ssl_initialize(SSL_CONTEXT *ssl_context)
{
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    // Determine SSL method based on version flags
    if (ssl_context->tls_version == 0) {
        // Auto-negotiate
        ssl_context->ssl_method = TLS_server_method();
    } else {
        // Specific version
        if (ssl_context->tls_version & TLS_VERSION_TLS1_3) {
            ssl_context->ssl_method = TLSv1_3_server_method();
        } else if (ssl_context->tls_version & TLS_VERSION_TLS1_2) {
            ssl_context->ssl_method = TLSv1_2_server_method();
        }
    }
    
    // Create context
    ssl_context->ssl_ctx = SSL_CTX_new(ssl_context->ssl_method);
    if (!ssl_context->ssl_ctx) {
        sql_print_error("SSL: Unable to create SSL context");
        return 1;
    }
    
    // Set options
    SSL_CTX_set_options(ssl_context->ssl_ctx, 
                       SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                       SSL_OP_SINGLE_DH_USE |
                       SSL_OP_SINGLE_ECDH_USE);
    
    // Set cipher list
    if (ssl_context->cipher_list) {
        if (!SSL_CTX_set_cipher_list(ssl_context->ssl_ctx, 
                                    ssl_context->cipher_list)) {
            sql_print_error("SSL: Failed to set cipher list: %s",
                          ssl_context->cipher_list);
            return 1;
        }
    } else {
        // Default secure ciphers
        SSL_CTX_set_cipher_list(ssl_context->ssl_ctx,
                               "ECDHE+AESGCM:ECDHE+AES256:!aNULL:!MD5:!DSS");
    }
    
    // Load server certificate
    if (SSL_CTX_use_certificate_file(ssl_context->ssl_ctx,
                                    ssl_context->cert_file,
                                    SSL_FILETYPE_PEM) <= 0) {
        sql_print_error("SSL: Unable to load certificate file: %s",
                       ssl_context->cert_file);
        return 1;
    }
    
    // Load private key
    if (SSL_CTX_use_PrivateKey_file(ssl_context->ssl_ctx,
                                   ssl_context->key_file,
                                   SSL_FILETYPE_PEM) <= 0) {
        sql_print_error("SSL: Unable to load private key file: %s",
                       ssl_context->key_file);
        return 1;
    }
    
    // Verify private key
    if (!SSL_CTX_check_private_key(ssl_context->ssl_ctx)) {
        sql_print_error("SSL: Private key does not match certificate");
        return 1;
    }
    
    // Set up client certificate verification
    if (ssl_context->ca_file || ssl_context->ca_path) {
        if (!SSL_CTX_load_verify_locations(ssl_context->ssl_ctx,
                                         ssl_context->ca_file,
                                         ssl_context->ca_path)) {
            sql_print_error("SSL: Failed to load CA certificates");
            return 1;
        }
        
        if (ssl_context->verify_cert) {
            SSL_CTX_set_verify(ssl_context->ssl_ctx,
                             SSL_VERIFY_PEER |
                             SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                             NULL);
        }
    }
    
    // Set up session caching
    SSL_CTX_set_session_cache_mode(ssl_context->ssl_ctx,
                                  SSL_SESS_CACHE_SERVER);
    SSL_CTX_sess_set_cache_size(ssl_context->ssl_ctx, 128);
    SSL_CTX_set_timeout(ssl_context->ssl_ctx, 300);
    
    // Set up ECDH
    EC_KEY *ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (ecdh) {
        SSL_CTX_set_tmp_ecdh(ssl_context->ssl_ctx, ecdh);
        EC_KEY_free(ecdh);
    }
    
    return 0;
}

// Accept SSL connection
int ssl_accept_connection(Vio *vio, SSL_CONTEXT *ssl_context, 
                         ulong timeout)
{
    SSL *ssl;
    my_socket sd = mysql_socket_getfd(vio->mysql_socket);
    
    // Create SSL object
    if (!(ssl = SSL_new(ssl_context->ssl_ctx))) {
        return 1;
    }
    
    // Set socket
    SSL_set_fd(ssl, sd);
    
    // Set accept timeout
    SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
    
    // Perform handshake
    int ret = SSL_accept(ssl);
    
    if (ret <= 0) {
        int ssl_error = SSL_get_error(ssl, ret);
        
        switch (ssl_error) {
            case SSL_ERROR_ZERO_RETURN:
                // Connection closed
                break;
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                // Would block
                break;
            case SSL_ERROR_SYSCALL:
                // System error
                break;
            case SSL_ERROR_SSL:
                // Protocol error
                ERR_print_errors_fp(stderr);
                break;
        }
        
        SSL_free(ssl);
        return 1;
    }
    
    // Verify client certificate if required
    if (ssl_context->verify_cert) {
        X509 *cert = SSL_get_peer_certificate(ssl);
        
        if (!cert) {
            SSL_free(ssl);
            return 1;
        }
        
        if (SSL_get_verify_result(ssl) != X509_V_OK) {
            X509_free(cert);
            SSL_free(ssl);
            return 1;
        }
        
        X509_free(cert);
    }
    
    // Switch VIO to SSL mode
    vio->ssl_arg = ssl;
    vio->read = vio_ssl_read;
    vio->write = vio_ssl_write;
    
    return 0;
}
```

---

# Microsoft SQL Server Network Layer

## Connection Pooling

### SQL Server Connection Pool
```c
// Connection pool manager
typedef struct CONNECTION_POOL_MANAGER {
    HASH_TABLE     *pools;              // Hash table of pools
    CRITICAL_SECTION lock;              // Pool manager lock
    ULONG           max_pool_size;      // Maximum pool size
    ULONG           min_pool_size;      // Minimum pool size
    ULONG           connection_lifetime; // Connection lifetime
    ULONG           connection_timeout;  // Connection timeout
    ULONG           load_balance_timeout; // Load balance timeout
    BOOL            pooling_enabled;     // Pooling enabled
} CONNECTION_POOL_MANAGER;

// Connection pool
typedef struct CONNECTION_POOL {
    WCHAR          *connection_string;  // Connection string
    LIST_ENTRY      free_connections;   // Free connections
    LIST_ENTRY      busy_connections;   // Busy connections
    CRITICAL_SECTION lock;              // Pool lock
    HANDLE          cleanup_timer;      // Cleanup timer
    ULONG           current_size;       // Current pool size
    ULONG           high_water_mark;    // High water mark
    ULONG           total_created;      // Total created
    SYSTEMTIME      created_time;       // Creation time
} CONNECTION_POOL;

// Pooled connection
typedef struct POOLED_CONNECTION {
    LIST_ENTRY      list_entry;         // List entry
    CONNECTION_POOL *pool;              // Parent pool
    SQLCONNECTION   *sql_connection;    // SQL connection
    SYSTEMTIME      created_time;       // Creation time
    SYSTEMTIME      last_used_time;     // Last used time
    ULONG           use_count;          // Use count
    BOOL            enlisted;           // Enlisted in transaction
    BOOL            reset_required;     // Reset required
    GUID            connection_id;      // Connection ID
} POOLED_CONNECTION;

// Get connection from pool
POOLED_CONNECTION *GetPooledConnection(
    const WCHAR *connection_string,
    ULONG timeout)
{
    CONNECTION_POOL_MANAGER *manager = GetPoolManager();
    CONNECTION_POOL *pool;
    POOLED_CONNECTION *connection = NULL;
    
    EnterCriticalSection(&manager->lock);
    
    // Find or create pool
    pool = FindPool(manager, connection_string);
    if (!pool) {
        pool = CreatePool(manager, connection_string);
    }
    
    LeaveCriticalSection(&manager->lock);
    
    if (!pool) {
        return NULL;
    }
    
    EnterCriticalSection(&pool->lock);
    
    // Try to get free connection
    while (!IsListEmpty(&pool->free_connections)) {
        PLIST_ENTRY entry = RemoveHeadList(&pool->free_connections);
        connection = CONTAINING_RECORD(entry, POOLED_CONNECTION, list_entry);
        
        // Validate connection
        if (ValidateConnection(connection)) {
            // Check lifetime
            SYSTEMTIME now;
            GetSystemTime(&now);
            
            ULONGLONG age = GetTimeDifference(&now, &connection->created_time);
            
            if (age > manager->connection_lifetime) {
                // Connection too old
                CloseConnection(connection);
                connection = NULL;
                continue;
            }
            
            // Reset connection if needed
            if (connection->reset_required) {
                if (!ResetConnection(connection)) {
                    CloseConnection(connection);
                    connection = NULL;
                    continue;
                }
            }
            
            // Mark as busy
            InsertTailList(&pool->busy_connections, &connection->list_entry);
            connection->last_used_time = now;
            connection->use_count++;
            
            break;
        } else {
            // Invalid connection
            CloseConnection(connection);
            connection = NULL;
        }
    }
    
    // Create new connection if needed
    if (!connection && pool->current_size < manager->max_pool_size) {
        connection = CreateNewConnection(pool);
        
        if (connection) {
            InsertTailList(&pool->busy_connections, &connection->list_entry);
            pool->current_size++;
            pool->total_created++;
            
            if (pool->current_size > pool->high_water_mark) {
                pool->high_water_mark = pool->current_size;
            }
        }
    }
    
    LeaveCriticalSection(&pool->lock);
    
    // Wait if no connection available
    if (!connection && timeout > 0) {
        HANDLE wait_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        
        // Register wait
        RegisterPoolWait(pool, wait_event);
        
        DWORD result = WaitForSingleObject(wait_event, timeout);
        
        if (result == WAIT_OBJECT_0) {
            // Retry
            return GetPooledConnection(connection_string, 0);
        }
        
        CloseHandle(wait_event);
    }
    
    return connection;
}

// Return connection to pool
void ReturnPooledConnection(POOLED_CONNECTION *connection)
{
    CONNECTION_POOL *pool = connection->pool;
    
    EnterCriticalSection(&pool->lock);
    
    // Remove from busy list
    RemoveEntryList(&connection->list_entry);
    
    // Check if connection is still valid
    if (connection->sql_connection->killed || 
        connection->enlisted) {
        // Connection cannot be reused
        CloseConnection(connection);
        pool->current_size--;
    } else {
        // Mark for reset
        connection->reset_required = TRUE;
        
        // Add to free list
        InsertTailList(&pool->free_connections, &connection->list_entry);
        
        // Signal waiting threads
        SignalPoolWaiters(pool);
    }
    
    LeaveCriticalSection(&pool->lock);
}

// Connection pool cleanup
VOID CALLBACK PoolCleanupTimer(
    PVOID lpParameter,
    BOOLEAN TimerOrWaitFired)
{
    CONNECTION_POOL *pool = (CONNECTION_POOL *)lpParameter;
    CONNECTION_POOL_MANAGER *manager = GetPoolManager();
    LIST_ENTRY expired_list;
    SYSTEMTIME now;
    
    InitializeListHead(&expired_list);
    GetSystemTime(&now);
    
    EnterCriticalSection(&pool->lock);
    
    // Check free connections
    PLIST_ENTRY entry = pool->free_connections.Flink;
    
    while (entry != &pool->free_connections) {
        POOLED_CONNECTION *connection = 
            CONTAINING_RECORD(entry, POOLED_CONNECTION, list_entry);
        PLIST_ENTRY next = entry->Flink;
        
        // Check idle time
        ULONGLONG idle = GetTimeDifference(&now, &connection->last_used_time);
        
        if (idle > IDLE_CONNECTION_TIMEOUT ||
            GetTimeDifference(&now, &connection->created_time) > 
            manager->connection_lifetime) {
            
            // Remove from free list
            RemoveEntryList(entry);
            
            // Add to expired list
            InsertTailList(&expired_list, entry);
            pool->current_size--;
        }
        
        entry = next;
    }
    
    // Maintain minimum pool size
    while (pool->current_size < manager->min_pool_size) {
        POOLED_CONNECTION *connection = CreateNewConnection(pool);
        
        if (connection) {
            InsertTailList(&pool->free_connections, &connection->list_entry);
            pool->current_size++;
        } else {
            break;
        }
    }
    
    LeaveCriticalSection(&pool->lock);
    
    // Close expired connections
    while (!IsListEmpty(&expired_list)) {
        PLIST_ENTRY entry = RemoveHeadList(&expired_list);
        POOLED_CONNECTION *connection = 
            CONTAINING_RECORD(entry, POOLED_CONNECTION, list_entry);
        
        CloseConnection(connection);
    }
}
```

## SSL/TLS Configuration

### SQL Server SSL/TLS (Schannel)
```c
// Schannel security context
typedef struct SCHANNEL_CONTEXT {
    CredHandle      server_cred;        // Server credentials
    CtxtHandle      security_context;   // Security context
    PCCERT_CONTEXT  server_cert;        // Server certificate
    SCHANNEL_CRED   schannel_cred;      // Schannel credentials
    SecPkgContext_StreamSizes stream_sizes; // Stream sizes
    BOOL            client_cert_required; // Require client cert
    DWORD           enabled_protocols;   // Enabled protocols
} SCHANNEL_CONTEXT;

// Initialize Schannel
BOOL InitializeSchannel(SCHANNEL_CONTEXT *context, 
                       const WCHAR *cert_thumbprint)
{
    SECURITY_STATUS status;
    HCERTSTORE cert_store;
    
    // Open certificate store
    cert_store = CertOpenStore(CERT_STORE_PROV_SYSTEM,
                              0,
                              NULL,
                              CERT_SYSTEM_STORE_LOCAL_MACHINE,
                              L"MY");
    
    if (!cert_store) {
        return FALSE;
    }
    
    // Find certificate by thumbprint
    CRYPT_HASH_BLOB hash_blob;
    BYTE thumbprint[20];
    HexStringToBytes(cert_thumbprint, thumbprint, sizeof(thumbprint));
    
    hash_blob.cbData = sizeof(thumbprint);
    hash_blob.pbData = thumbprint;
    
    context->server_cert = CertFindCertificateInStore(
        cert_store,
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        0,
        CERT_FIND_HASH,
        &hash_blob,
        NULL);
    
    if (!context->server_cert) {
        CertCloseStore(cert_store, 0);
        return FALSE;
    }
    
    // Initialize Schannel credentials
    ZeroMemory(&context->schannel_cred, sizeof(SCHANNEL_CRED));
    context->schannel_cred.dwVersion = SCHANNEL_CRED_VERSION;
    
    if (context->server_cert) {
        context->schannel_cred.cCreds = 1;
        context->schannel_cred.paCred = &context->server_cert;
    }
    
    // Set protocols (TLS 1.2 and 1.3)
    context->schannel_cred.grbitEnabledProtocols = 
        SP_PROT_TLS1_2_SERVER | SP_PROT_TLS1_3_SERVER;
    
    // Set cipher suites
    ALG_ID alg_list[] = {
        CALG_AES_256,
        CALG_AES_128,
        CALG_3DES,
        CALG_SHA256,
        CALG_SHA384,
        CALG_SHA512,
        CALG_ECDH_EPHEM,
        CALG_RSA_KEYX
    };
    
    context->schannel_cred.cSupportedAlgs = sizeof(alg_list) / sizeof(ALG_ID);
    context->schannel_cred.palgSupportedAlgs = alg_list;
    
    // Set flags
    context->schannel_cred.dwFlags = SCH_CRED_NO_SYSTEM_MAPPER;
    
    if (context->client_cert_required) {
        context->schannel_cred.dwFlags |= SCH_CRED_REVOCATION_CHECK_CHAIN;
    }
    
    // Acquire credentials
    status = AcquireCredentialsHandle(
        NULL,
        UNISP_NAME,
        SECPKG_CRED_INBOUND,
        NULL,
        &context->schannel_cred,
        NULL,
        NULL,
        &context->server_cred,
        NULL);
    
    CertCloseStore(cert_store, 0);
    
    return (status == SEC_E_OK);
}

// Perform SSL handshake
BOOL PerformSSLHandshake(SOCKET client_socket, 
                        SCHANNEL_CONTEXT *context)
{
    SecBufferDesc input_desc, output_desc;
    SecBuffer input_buffers[2], output_buffer;
    SECURITY_STATUS status;
    DWORD flags_in, flags_out;
    TimeStamp expiry;
    BYTE *input_data;
    DWORD input_size = 0;
    BOOL first_call = TRUE;
    
    // Allocate input buffer
    input_data = (BYTE *)malloc(16384);
    
    flags_in = ASC_REQ_SEQUENCE_DETECT |
               ASC_REQ_REPLAY_DETECT |
               ASC_REQ_CONFIDENTIALITY |
               ASC_REQ_EXTENDED_ERROR |
               ASC_REQ_STREAM;
    
    if (context->client_cert_required) {
        flags_in |= ASC_REQ_MUTUAL_AUTH;
    }
    
    // Handshake loop
    while (TRUE) {
        // Receive data from client
        if (input_size == 0 || status == SEC_E_INCOMPLETE_MESSAGE) {
            int bytes = recv(client_socket, 
                           input_data + input_size,
                           16384 - input_size,
                           0);
            
            if (bytes <= 0) {
                free(input_data);
                return FALSE;
            }
            
            input_size += bytes;
        }
        
        // Set up input buffers
        input_buffers[0].pvBuffer = input_data;
        input_buffers[0].cbBuffer = input_size;
        input_buffers[0].BufferType = SECBUFFER_TOKEN;
        
        input_buffers[1].pvBuffer = NULL;
        input_buffers[1].cbBuffer = 0;
        input_buffers[1].BufferType = SECBUFFER_EMPTY;
        
        input_desc.cBuffers = 2;
        input_desc.pBuffers = input_buffers;
        input_desc.ulVersion = SECBUFFER_VERSION;
        
        // Set up output buffer
        output_buffer.pvBuffer = NULL;
        output_buffer.cbBuffer = 0;
        output_buffer.BufferType = SECBUFFER_TOKEN;
        
        output_desc.cBuffers = 1;
        output_desc.pBuffers = &output_buffer;
        output_desc.ulVersion = SECBUFFER_VERSION;
        
        // Call AcceptSecurityContext
        status = AcceptSecurityContext(
            &context->server_cred,
            first_call ? NULL : &context->security_context,
            &input_desc,
            flags_in,
            SECURITY_NETWORK_DREP,
            first_call ? &context->security_context : NULL,
            &output_desc,
            &flags_out,
            &expiry);
        
        first_call = FALSE;
        
        // Send output to client if needed
        if (output_buffer.cbBuffer > 0 && output_buffer.pvBuffer) {
            send(client_socket, 
                output_buffer.pvBuffer,
                output_buffer.cbBuffer,
                0);
            
            FreeContextBuffer(output_buffer.pvBuffer);
        }
        
        // Check status
        if (status == SEC_E_OK) {
            // Handshake complete
            break;
        } else if (status == SEC_I_CONTINUE_NEEDED) {
            // Continue handshake
            if (input_buffers[1].BufferType == SECBUFFER_EXTRA) {
                // Extra data - move to beginning
                memmove(input_data,
                       input_data + (input_size - input_buffers[1].cbBuffer),
                       input_buffers[1].cbBuffer);
                input_size = input_buffers[1].cbBuffer;
            } else {
                input_size = 0;
            }
        } else if (status == SEC_E_INCOMPLETE_MESSAGE) {
            // Need more data
            continue;
        } else {
            // Error
            free(input_data);
            return FALSE;
        }
    }
    
    // Get stream sizes
    status = QueryContextAttributes(&context->security_context,
                                   SECPKG_ATTR_STREAM_SIZES,
                                   &context->stream_sizes);
    
    free(input_data);
    
    return (status == SEC_E_OK);
}
```