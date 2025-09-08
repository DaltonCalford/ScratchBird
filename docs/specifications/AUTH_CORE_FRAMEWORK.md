# ScratchBird Authentication Core Framework
## Complete Technical Specification

## Overview

This document defines the complete authentication framework for ScratchBird, providing a pluggable, secure, and extensible authentication system supporting multiple methods with enterprise-grade security features.

## 1. Authentication Architecture

### 1.1 Core Authentication Manager

```c
// Master authentication manager
typedef struct sb_auth_manager {
    // Configuration
    AuthConfig*         am_config;             // Authentication configuration
    
    // Method registry
    HashTable*          am_methods;            // Registered auth methods
    AuthMethod**        am_method_array;       // Ordered by priority
    uint32_t            am_method_count;       // Number of methods
    
    // Session management
    SessionManager*     am_session_mgr;        // Active sessions
    TokenManager*       am_token_mgr;          // Token management
    
    // Security
    RateLimiter*        am_rate_limiter;       // Brute force protection
    AuditLogger*        am_audit_logger;       // Security audit logging
    
    // Cache
    AuthCache*          am_auth_cache;         // Successful auth cache
    FailureCache*       am_failure_cache;      // Failed attempt tracking
    
    // Statistics
    AuthStats           am_stats;              // Authentication statistics
} SBAuthManager;

// Authentication method interface
typedef struct auth_method {
    // Method identification
    AuthType            method_type;           // Method type enum
    char                method_name[64];       // Human-readable name
    uint32_t            method_priority;       // Priority (lower = higher priority)
    
    // Capabilities
    AuthCapabilities    method_capabilities;   // Method capabilities flags
    
    // Operations
    AuthMethodOps*      method_ops;            // Method operations
    
    // Configuration
    void*               method_config;         // Method-specific config
    
    // State
    void*               method_state;          // Method-specific state
} AuthMethod;

// Authentication method operations
typedef struct auth_method_ops {
    // Initialization
    Status (*init)(AuthMethod* method, void* config);
    Status (*shutdown)(AuthMethod* method);
    
    // Authentication flow
    Status (*start_auth)(AuthMethod* method, AuthContext* ctx);
    Status (*continue_auth)(AuthMethod* method, AuthContext* ctx, 
                           const void* data, size_t len);
    Status (*finish_auth)(AuthMethod* method, AuthContext* ctx);
    
    // Validation
    bool (*validate_credentials)(AuthMethod* method, 
                                const Credentials* creds);
    
    // User management
    Status (*create_user)(AuthMethod* method, const UserInfo* user);
    Status (*update_credentials)(AuthMethod* method, 
                                const char* username,
                                const Credentials* new_creds);
    Status (*delete_user)(AuthMethod* method, const char* username);
    
    // Token operations (if supported)
    Token* (*generate_token)(AuthMethod* method, const AuthContext* ctx);
    Status (*validate_token)(AuthMethod* method, const Token* token);
    Status (*revoke_token)(AuthMethod* method, const Token* token);
    
    // Configuration
    Status (*reload_config)(AuthMethod* method, void* new_config);
    void* (*get_config)(AuthMethod* method);
} AuthMethodOps;
```

### 1.2 Authentication Context

```c
// Authentication context for a connection
typedef struct auth_context {
    // Connection info
    ConnectionId        ctx_conn_id;           // Connection identifier
    IPAddress           ctx_client_ip;         // Client IP address
    uint16_t            ctx_client_port;       // Client port
    
    // User info
    char                ctx_username[256];     // Username attempting auth
    char                ctx_database[256];     // Target database
    char                ctx_application[256];  // Application name
    
    // Method info
    AuthType            ctx_auth_type;         // Current auth method
    AuthState           ctx_state;             // Current auth state
    
    // Security
    uint32_t            ctx_attempt_count;     // Number of attempts
    TimestampTz         ctx_start_time;        // Auth start time
    TimestampTz         ctx_last_attempt;      // Last attempt time
    
    // Challenge data
    uint8_t             ctx_server_nonce[32];  // Server nonce
    uint8_t             ctx_client_nonce[32];  // Client nonce
    void*               ctx_challenge_data;    // Method-specific challenge
    
    // Result
    AuthResult          ctx_result;            // Authentication result
    char                ctx_error_msg[512];    // Error message if failed
    
    // Session info (after success)
    SessionId           ctx_session_id;        // Generated session ID
    UserId              ctx_user_id;           // Internal user ID
    RoleList*           ctx_roles;             // User roles
    PrivilegeList*      ctx_privileges;        // Computed privileges
} AuthContext;

// Authentication states
typedef enum auth_state {
    AUTH_STATE_INIT,            // Initial state
    AUTH_STATE_NEGOTIATING,     // Negotiating method
    AUTH_STATE_CHALLENGING,     // Challenge sent
    AUTH_STATE_VERIFYING,       // Verifying response
    AUTH_STATE_FINALIZING,      // Final checks
    AUTH_STATE_SUCCESS,         // Authentication successful
    AUTH_STATE_FAILED,          // Authentication failed
    AUTH_STATE_ERROR            // Error occurred
} AuthState;

// Authentication result
typedef struct auth_result {
    bool                success;               // Success flag
    AuthFailureReason   failure_reason;        // Reason if failed
    uint32_t            remaining_attempts;    // Remaining attempts
    TimestampTz         lockout_until;         // Lockout time if applicable
    char                message[512];          // Result message
} AuthResult;
```

### 1.3 Host-Based Authentication (HBA)

```c
// HBA configuration entry
typedef struct hba_entry {
    // Match criteria
    HBAType             hba_type;              // local, host, hostssl, hostnossl
    IPAddress           hba_address;           // IP address
    uint32_t            hba_netmask;           // Network mask
    char                hba_database[256];     // Database pattern
    char                hba_user[256];         // User pattern
    
    // Authentication method
    AuthType            hba_auth_method;       // Authentication method
    void*               hba_auth_options;      // Method-specific options
    
    // Additional filters
    char                hba_filter[512];       // Additional LDAP-style filter
    
    // Priority
    uint32_t            hba_priority;          // Rule priority
    
    // Chain
    struct hba_entry*   hba_next;              // Next entry
} HBAEntry;

// HBA configuration
typedef struct hba_config {
    HBAEntry*           hba_entries;           // Linked list of entries
    uint32_t            hba_count;             // Number of entries
    
    // Default policy
    AuthType            hba_default_method;    // Default if no match
    bool                hba_default_deny;      // Deny by default
    
    // File info
    char                hba_file_path[PATH_MAX]; // Config file path
    TimestampTz         hba_last_reload;       // Last reload time
    
    // Mutex for reload
    pthread_rwlock_t    hba_lock;              // Read-write lock
} HBAConfig;

// HBA matching
AuthType match_hba_entry(
    HBAConfig* config,
    const IPAddress* client_ip,
    const char* database,
    const char* username)
{
    pthread_rwlock_rdlock(&config->hba_lock);
    
    HBAEntry* entry = config->hba_entries;
    while (entry != NULL) {
        if (hba_entry_matches(entry, client_ip, database, username)) {
            AuthType method = entry->hba_auth_method;
            pthread_rwlock_unlock(&config->hba_lock);
            return method;
        }
        entry = entry->hba_next;
    }
    
    pthread_rwlock_unlock(&config->hba_lock);
    return config->hba_default_method;
}
```

## 2. Authentication Protocol Flow

### 2.1 Wire Protocol Messages

```c
// Authentication request types
typedef enum auth_request_type {
    AUTH_REQ_OK = 0,            // Authentication successful
    AUTH_REQ_PASSWORD = 3,      // Clear-text password
    AUTH_REQ_MD5 = 5,          // MD5 password
    AUTH_REQ_SCRAM = 10,       // SCRAM-SHA-256
    AUTH_REQ_SCRAM_SHA_256 = 11,
    AUTH_REQ_SCRAM_SHA_512 = 12,
    AUTH_REQ_CERT = 13,        // Certificate
    AUTH_REQ_GSS = 7,          // GSSAPI
    AUTH_REQ_SSPI = 9,         // SSPI (Windows)
    AUTH_REQ_SASL = 14,        // SASL initial
    AUTH_REQ_SASL_CONT = 15,   // SASL continue
    AUTH_REQ_SASL_FIN = 16     // SASL final
} AuthRequestType;

// Authentication message
typedef struct auth_message {
    uint8_t             msg_type;              // Message type ('R' for auth)
    uint32_t            msg_length;            // Message length
    AuthRequestType     msg_auth_type;         // Authentication type
    uint8_t             msg_data[];            // Type-specific data
} AuthMessage;

// SASL mechanism list
typedef struct sasl_mechanisms {
    uint32_t            mech_count;            // Number of mechanisms
    char**              mech_names;            // Mechanism names
} SASLMechanisms;

// Authentication flow
Status handle_authentication(Connection* conn) {
    // Step 1: Receive startup message
    StartupMessage* startup = receive_startup_message(conn);
    
    // Step 2: Determine authentication method
    AuthType auth_type = match_hba_entry(
        g_hba_config,
        &conn->client_address,
        startup->database,
        startup->user
    );
    
    // Step 3: Create authentication context
    AuthContext* ctx = create_auth_context(conn, startup);
    ctx->ctx_auth_type = auth_type;
    
    // Step 4: Get authentication method
    AuthMethod* method = get_auth_method(g_auth_manager, auth_type);
    
    // Step 5: Start authentication
    Status status = method->method_ops->start_auth(method, ctx);
    
    // Step 6: Authentication loop
    while (ctx->ctx_state != AUTH_STATE_SUCCESS && 
           ctx->ctx_state != AUTH_STATE_FAILED) {
        
        // Send challenge/request
        send_auth_request(conn, ctx);
        
        // Receive response
        AuthResponse* response = receive_auth_response(conn);
        
        // Continue authentication
        status = method->method_ops->continue_auth(
            method, ctx, 
            response->data, response->length
        );
        
        free_auth_response(response);
    }
    
    // Step 7: Finalize
    if (ctx->ctx_state == AUTH_STATE_SUCCESS) {
        // Create session
        create_session(ctx);
        
        // Send success
        send_auth_success(conn);
        
        // Log success
        audit_log_success(ctx);
    } else {
        // Send failure
        send_auth_failure(conn, ctx->ctx_error_msg);
        
        // Log failure
        audit_log_failure(ctx);
        
        // Update failure cache
        update_failure_cache(ctx);
    }
    
    free_auth_context(ctx);
    return status;
}
```

### 2.2 Session Management

```c
// Session structure
typedef struct session {
    // Identity
    SessionId           sess_id;               // Unique session ID
    UserId              sess_user_id;          // User ID
    char                sess_username[256];    // Username
    
    // Connection
    ConnectionId        sess_conn_id;          // Associated connection
    IPAddress           sess_client_ip;        // Client IP
    
    // Authentication
    AuthType            sess_auth_method;      // Auth method used
    TimestampTz         sess_auth_time;        // Authentication time
    
    // Privileges
    RoleList*           sess_roles;            // Active roles
    PrivilegeList*      sess_privileges;       // Computed privileges
    
    // State
    SessionState        sess_state;            // Current state
    TimestampTz         sess_last_activity;    // Last activity time
    
    // Security
    bool                sess_is_superuser;     // Superuser flag
    bool                sess_is_replication;   // Replication connection
    SecurityLabel*      sess_security_label;   // Security label (MAC)
    
    // Limits
    uint32_t            sess_max_queries;      // Query limit
    uint32_t            sess_query_count;      // Queries executed
    TimestampTz         sess_expires_at;       // Session expiration
    
    // Token (if applicable)
    Token*              sess_token;            // Session token
} Session;

// Session manager
typedef struct session_manager {
    // Storage
    HashTable*          sm_sessions;           // Active sessions
    uint32_t            sm_max_sessions;       // Maximum sessions
    uint32_t            sm_active_count;       // Active session count
    
    // Cleanup
    pthread_t           sm_cleanup_thread;     // Cleanup thread
    uint32_t            sm_timeout_seconds;    // Idle timeout
    uint32_t            sm_max_lifetime;       // Max session lifetime
    
    // Statistics
    SessionStats        sm_stats;              // Session statistics
    
    // Synchronization
    pthread_rwlock_t    sm_lock;               // Read-write lock
} SessionManager;

// Session operations
Session* create_session(AuthContext* ctx) {
    Session* session = allocate(sizeof(Session));
    
    // Generate session ID
    session->sess_id = generate_session_id();
    
    // Copy user info
    session->sess_user_id = ctx->ctx_user_id;
    strncpy(session->sess_username, ctx->ctx_username, 
            sizeof(session->sess_username));
    
    // Copy connection info
    session->sess_conn_id = ctx->ctx_conn_id;
    session->sess_client_ip = ctx->ctx_client_ip;
    
    // Set authentication info
    session->sess_auth_method = ctx->ctx_auth_type;
    session->sess_auth_time = GetCurrentTimestamp();
    
    // Copy privileges
    session->sess_roles = copy_role_list(ctx->ctx_roles);
    session->sess_privileges = copy_privilege_list(ctx->ctx_privileges);
    
    // Set state
    session->sess_state = SESSION_STATE_ACTIVE;
    session->sess_last_activity = GetCurrentTimestamp();
    
    // Add to session manager
    add_session(g_session_manager, session);
    
    return session;
}
```

## 3. Security Features

### 3.1 Rate Limiting and Brute Force Protection

```c
// Rate limiter configuration
typedef struct rate_limiter_config {
    // Per-IP limits
    uint32_t            rlc_max_attempts_per_ip;      // Max attempts per IP
    uint32_t            rlc_ip_window_seconds;        // Time window for IP
    uint32_t            rlc_ip_lockout_seconds;       // Lockout duration
    
    // Per-user limits
    uint32_t            rlc_max_attempts_per_user;    // Max attempts per user
    uint32_t            rlc_user_window_seconds;      // Time window for user
    uint32_t            rlc_user_lockout_seconds;     // Lockout duration
    
    // Global limits
    uint32_t            rlc_max_global_attempts;      // Max global attempts
    uint32_t            rlc_global_window_seconds;    // Global window
    
    // Exponential backoff
    bool                rlc_exponential_backoff;      // Enable backoff
    double              rlc_backoff_multiplier;       // Backoff multiplier
    uint32_t            rlc_max_backoff_seconds;      // Max backoff
} RateLimiterConfig;

// Rate limiter
typedef struct rate_limiter {
    RateLimiterConfig   rl_config;             // Configuration
    
    // Tracking
    HashTable*          rl_ip_attempts;        // IP -> attempt count
    HashTable*          rl_user_attempts;      // User -> attempt count
    HashTable*          rl_lockouts;           // Active lockouts
    
    // Global counter
    AtomicCounter       rl_global_attempts;    // Global attempt counter
    TimestampTz         rl_global_window_start; // Window start time
    
    // Cleanup
    pthread_t           rl_cleanup_thread;     // Cleanup thread
    
    // Synchronization
    pthread_mutex_t     rl_mutex;              // Mutex
} RateLimiter;

// Check rate limit
RateLimitResult check_rate_limit(
    RateLimiter* limiter,
    const IPAddress* ip,
    const char* username)
{
    pthread_mutex_lock(&limiter->rl_mutex);
    
    RateLimitResult result = {
        .allowed = true,
        .remaining_attempts = 0,
        .retry_after_seconds = 0
    };
    
    // Check IP lockout
    Lockout* ip_lockout = find_lockout(limiter->rl_lockouts, ip);
    if (ip_lockout && !is_lockout_expired(ip_lockout)) {
        result.allowed = false;
        result.retry_after_seconds = get_remaining_lockout_time(ip_lockout);
        pthread_mutex_unlock(&limiter->rl_mutex);
        return result;
    }
    
    // Check user lockout
    if (username) {
        Lockout* user_lockout = find_user_lockout(limiter->rl_lockouts, username);
        if (user_lockout && !is_lockout_expired(user_lockout)) {
            result.allowed = false;
            result.retry_after_seconds = get_remaining_lockout_time(user_lockout);
            pthread_mutex_unlock(&limiter->rl_mutex);
            return result;
        }
    }
    
    // Check IP attempts
    AttemptTracker* ip_tracker = get_or_create_tracker(
        limiter->rl_ip_attempts, ip);
    
    if (ip_tracker->attempt_count >= limiter->rl_config.rlc_max_attempts_per_ip) {
        // Create lockout
        create_lockout(limiter, ip, NULL, 
                      limiter->rl_config.rlc_ip_lockout_seconds);
        result.allowed = false;
        result.retry_after_seconds = limiter->rl_config.rlc_ip_lockout_seconds;
    } else {
        result.remaining_attempts = 
            limiter->rl_config.rlc_max_attempts_per_ip - ip_tracker->attempt_count;
    }
    
    pthread_mutex_unlock(&limiter->rl_mutex);
    return result;
}

// Record authentication attempt
void record_auth_attempt(
    RateLimiter* limiter,
    const IPAddress* ip,
    const char* username,
    bool success)
{
    pthread_mutex_lock(&limiter->rl_mutex);
    
    if (success) {
        // Clear attempts on success
        clear_ip_attempts(limiter, ip);
        if (username) {
            clear_user_attempts(limiter, username);
        }
    } else {
        // Increment attempt counters
        increment_ip_attempts(limiter, ip);
        if (username) {
            increment_user_attempts(limiter, username);
        }
        increment_global_attempts(limiter);
        
        // Check for lockout conditions
        check_and_apply_lockouts(limiter, ip, username);
    }
    
    pthread_mutex_unlock(&limiter->rl_mutex);
}
```

### 3.2 Audit Logging

```c
// Audit event types
typedef enum audit_event_type {
    AUDIT_AUTH_SUCCESS,         // Successful authentication
    AUDIT_AUTH_FAILURE,         // Failed authentication
    AUDIT_AUTH_LOCKOUT,         // Account locked out
    AUDIT_SESSION_START,        // Session started
    AUDIT_SESSION_END,          // Session ended
    AUDIT_PRIVILEGE_GRANT,      // Privilege granted
    AUDIT_PRIVILEGE_REVOKE,     // Privilege revoked
    AUDIT_PASSWORD_CHANGE,      // Password changed
    AUDIT_USER_CREATE,          // User created
    AUDIT_USER_DELETE,          // User deleted
    AUDIT_ROLE_CHANGE,          // Role membership changed
    AUDIT_CONFIG_CHANGE,        // Configuration changed
    AUDIT_SECURITY_VIOLATION    // Security violation detected
} AuditEventType;

// Audit log entry
typedef struct audit_log_entry {
    // Event info
    AuditEventType      ale_event_type;        // Event type
    TimestampTz         ale_timestamp;         // Event timestamp
    
    // Actor info
    UserId              ale_user_id;           // User ID (if applicable)
    char                ale_username[256];     // Username
    SessionId           ale_session_id;        // Session ID
    
    // Connection info
    IPAddress           ale_client_ip;         // Client IP
    uint16_t            ale_client_port;       // Client port
    char                ale_application[256];  // Application name
    
    // Target info
    char                ale_database[256];     // Database name
    char                ale_object_name[512];  // Object name (if applicable)
    
    // Details
    AuthType            ale_auth_method;       // Auth method (for auth events)
    char                ale_details[1024];     // Event details
    char                ale_error_msg[512];    // Error message (if failure)
    
    // Result
    bool                ale_success;           // Success flag
    int32_t             ale_error_code;        // Error code
} AuditLogEntry;

// Audit logger
typedef struct audit_logger {
    // Configuration
    AuditConfig         al_config;             // Audit configuration
    
    // Output
    FILE*               al_file;               // Log file
    char                al_file_path[PATH_MAX]; // File path
    
    // Buffer
    AuditLogEntry*      al_buffer;             // Entry buffer
    uint32_t            al_buffer_size;        // Buffer size
    uint32_t            al_buffer_pos;         // Current position
    
    // Rotation
    uint64_t            al_max_file_size;      // Max file size
    uint32_t            al_max_files;          // Max number of files
    
    // Statistics
    AuditStats          al_stats;              // Audit statistics
    
    // Synchronization
    pthread_mutex_t     al_mutex;              // Mutex
} AuditLogger;

// Log authentication event
void audit_log_auth(
    AuditLogger* logger,
    AuthContext* ctx,
    bool success)
{
    AuditLogEntry entry = {
        .ale_event_type = success ? AUDIT_AUTH_SUCCESS : AUDIT_AUTH_FAILURE,
        .ale_timestamp = GetCurrentTimestamp(),
        .ale_username = {0},
        .ale_client_ip = ctx->ctx_client_ip,
        .ale_client_port = ctx->ctx_client_port,
        .ale_database = {0},
        .ale_auth_method = ctx->ctx_auth_type,
        .ale_success = success
    };
    
    strncpy(entry.ale_username, ctx->ctx_username, sizeof(entry.ale_username));
    strncpy(entry.ale_database, ctx->ctx_database, sizeof(entry.ale_database));
    strncpy(entry.ale_application, ctx->ctx_application, 
            sizeof(entry.ale_application));
    
    if (!success) {
        strncpy(entry.ale_error_msg, ctx->ctx_error_msg, 
                sizeof(entry.ale_error_msg));
    }
    
    write_audit_log(logger, &entry);
}
```

### 3.3 Encryption and Secure Storage

```c
// Credential storage
typedef struct credential_storage {
    // Encryption
    EncryptionMethod    cs_encryption_method;  // Encryption method
    uint8_t             cs_master_key[32];     // Master key (encrypted)
    uint8_t             cs_key_salt[16];       // Key derivation salt
    
    // Storage backend
    StorageBackend      cs_backend;            // Storage backend type
    void*               cs_backend_config;     // Backend configuration
    
    // Cache
    CredentialCache*    cs_cache;              // Decrypted credential cache
    uint32_t            cs_cache_ttl_seconds;  // Cache TTL
    
    // Key rotation
    uint32_t            cs_key_version;        // Current key version
    TimestampTz         cs_last_rotation;      // Last rotation time
    uint32_t            cs_rotation_interval;  // Rotation interval (days)
} CredentialStorage;

// Store encrypted password
Status store_password(
    CredentialStorage* storage,
    const char* username,
    const char* password)
{
    // Generate salt for this password
    uint8_t salt[16];
    generate_random_bytes(salt, sizeof(salt));
    
    // Derive key using PBKDF2
    uint8_t derived_key[32];
    pbkdf2_sha256(
        password, strlen(password),
        salt, sizeof(salt),
        storage->cs_config.iterations,
        derived_key, sizeof(derived_key)
    );
    
    // Create storage entry
    PasswordEntry entry = {
        .username = username,
        .salt = salt,
        .hash = derived_key,
        .iterations = storage->cs_config.iterations,
        .algorithm = HASH_ALGO_PBKDF2_SHA256,
        .created_at = GetCurrentTimestamp(),
        .expires_at = calculate_password_expiry()
    };
    
    // Encrypt entry with master key
    uint8_t encrypted[sizeof(PasswordEntry) + 32];
    size_t encrypted_len;
    
    Status status = encrypt_data(
        storage->cs_master_key,
        &entry, sizeof(entry),
        encrypted, &encrypted_len
    );
    
    if (status != STATUS_OK) {
        return status;
    }
    
    // Store in backend
    return store_credential_entry(
        storage->cs_backend,
        username,
        encrypted,
        encrypted_len
    );
}

// Verify password
bool verify_password(
    CredentialStorage* storage,
    const char* username,
    const char* password)
{
    // Retrieve encrypted entry
    uint8_t encrypted[1024];
    size_t encrypted_len;
    
    Status status = retrieve_credential_entry(
        storage->cs_backend,
        username,
        encrypted,
        &encrypted_len
    );
    
    if (status != STATUS_OK) {
        return false;
    }
    
    // Decrypt entry
    PasswordEntry entry;
    size_t decrypted_len;
    
    status = decrypt_data(
        storage->cs_master_key,
        encrypted, encrypted_len,
        &entry, &decrypted_len
    );
    
    if (status != STATUS_OK) {
        return false;
    }
    
    // Check expiration
    if (entry.expires_at < GetCurrentTimestamp()) {
        audit_log_password_expired(username);
        return false;
    }
    
    // Derive key from provided password
    uint8_t derived_key[32];
    pbkdf2_sha256(
        password, strlen(password),
        entry.salt, sizeof(entry.salt),
        entry.iterations,
        derived_key, sizeof(derived_key)
    );
    
    // Compare in constant time
    return constant_time_compare(
        derived_key, 
        entry.hash, 
        sizeof(derived_key)
    );
}
```

## 4. Authentication Cache

```c
// Authentication cache for performance
typedef struct auth_cache {
    // Configuration
    uint32_t            ac_max_entries;        // Maximum cache entries
    uint32_t            ac_ttl_seconds;        // Entry TTL
    bool                ac_enabled;            // Cache enabled flag
    
    // Storage
    HashTable*          ac_entries;            // Cache entries
    LRUList*            ac_lru;                // LRU list for eviction
    
    // Statistics
    CacheStats          ac_stats;              // Cache statistics
    
    // Synchronization
    pthread_rwlock_t    ac_lock;               // Read-write lock
} AuthCache;

// Cache entry
typedef struct auth_cache_entry {
    // Key
    char                ace_username[256];     // Username
    uint8_t             ace_password_hash[32]; // Password hash (salted)
    
    // Value
    UserId              ace_user_id;           // User ID
    RoleList*           ace_roles;             // User roles
    PrivilegeList*      ace_privileges;        // Computed privileges
    
    // Metadata
    TimestampTz         ace_created_at;        // Creation time
    TimestampTz         ace_expires_at;        // Expiration time
    uint32_t            ace_hit_count;         // Hit count
    
    // LRU
    struct auth_cache_entry* ace_lru_next;     // Next in LRU
    struct auth_cache_entry* ace_lru_prev;     // Previous in LRU
} AuthCacheEntry;

// Check cache
AuthCacheEntry* check_auth_cache(
    AuthCache* cache,
    const char* username,
    const uint8_t* password_hash)
{
    if (!cache->ac_enabled) {
        return NULL;
    }
    
    pthread_rwlock_rdlock(&cache->ac_lock);
    
    // Create lookup key
    CacheKey key = {
        .username = username,
        .password_hash = password_hash
    };
    
    // Lookup in hash table
    AuthCacheEntry* entry = hash_table_lookup(cache->ac_entries, &key);
    
    if (entry != NULL) {
        // Check expiration
        if (entry->ace_expires_at < GetCurrentTimestamp()) {
            // Expired - remove
            pthread_rwlock_unlock(&cache->ac_lock);
            pthread_rwlock_wrlock(&cache->ac_lock);
            
            remove_cache_entry(cache, entry);
            entry = NULL;
        } else {
            // Hit - update LRU
            entry->ace_hit_count++;
            move_to_lru_head(cache->ac_lru, entry);
            cache->ac_stats.hits++;
        }
    } else {
        cache->ac_stats.misses++;
    }
    
    pthread_rwlock_unlock(&cache->ac_lock);
    return entry;
}

// Add to cache
void add_to_auth_cache(
    AuthCache* cache,
    const char* username,
    const uint8_t* password_hash,
    UserId user_id,
    RoleList* roles,
    PrivilegeList* privileges)
{
    if (!cache->ac_enabled) {
        return;
    }
    
    pthread_rwlock_wrlock(&cache->ac_lock);
    
    // Check if at capacity
    if (hash_table_count(cache->ac_entries) >= cache->ac_max_entries) {
        // Evict LRU entry
        AuthCacheEntry* lru = cache->ac_lru->tail;
        if (lru != NULL) {
            remove_cache_entry(cache, lru);
        }
    }
    
    // Create new entry
    AuthCacheEntry* entry = allocate(sizeof(AuthCacheEntry));
    strncpy(entry->ace_username, username, sizeof(entry->ace_username));
    memcpy(entry->ace_password_hash, password_hash, 32);
    entry->ace_user_id = user_id;
    entry->ace_roles = copy_role_list(roles);
    entry->ace_privileges = copy_privilege_list(privileges);
    entry->ace_created_at = GetCurrentTimestamp();
    entry->ace_expires_at = entry->ace_created_at + cache->ac_ttl_seconds;
    entry->ace_hit_count = 0;
    
    // Add to hash table
    hash_table_insert(cache->ac_entries, entry);
    
    // Add to LRU head
    add_to_lru_head(cache->ac_lru, entry);
    
    cache->ac_stats.entries++;
    
    pthread_rwlock_unlock(&cache->ac_lock);
}
```

## 5. Configuration

### 5.1 Authentication Configuration File

```yaml
# scratchbird_auth.conf
authentication:
  # Global settings
  global:
    default_method: scram-sha-256
    timeout_seconds: 30
    max_attempts: 5
    lockout_duration_seconds: 300
    
  # Method configurations
  methods:
    - name: password
      enabled: true
      priority: 3
      config:
        algorithm: pbkdf2-sha256
        iterations: 10000
        min_length: 8
        require_uppercase: true
        require_lowercase: true
        require_digit: true
        require_special: true
        max_age_days: 90
        history_count: 5
        
    - name: scram-sha-256
      enabled: true
      priority: 1
      config:
        iterations: 4096
        channel_binding: true
        
    - name: certificate
      enabled: true
      priority: 2
      config:
        ca_file: /etc/scratchbird/ca.crt
        crl_file: /etc/scratchbird/crl.pem
        verify_depth: 3
        require_client_cert: true
        map_file: /etc/scratchbird/cert_map.conf
        
    - name: ldap
      enabled: false
      priority: 4
      config:
        servers:
          - ldaps://ldap1.example.com:636
          - ldaps://ldap2.example.com:636
        base_dn: "dc=example,dc=com"
        bind_dn: "cn=scratchbird,dc=example,dc=com"
        bind_password_file: /secure/ldap.pwd
        search_filter: "(&(objectClass=user)(sAMAccountName=%u))"
        
  # Host-based authentication
  hba_rules:
    - type: local
      database: all
      user: all
      method: trust
      
    - type: host
      address: 127.0.0.1/32
      database: all
      user: all
      method: scram-sha-256
      
    - type: hostssl
      address: 192.168.0.0/16
      database: all
      user: all
      method: certificate
      options:
        clientcert: required
        
    - type: host
      address: 10.0.0.0/8
      database: all
      user: all
      method: ldap
      
  # Security settings
  security:
    audit:
      enabled: true
      log_file: /var/log/scratchbird/auth.log
      log_successful: true
      log_failed: true
      
    rate_limiting:
      enabled: true
      per_ip_limit: 10
      per_user_limit: 5
      window_seconds: 60
      
    session:
      idle_timeout_minutes: 30
      max_lifetime_hours: 24
      max_sessions_per_user: 10
```

### 5.2 Configuration Parser

```c
// Parse authentication configuration
AuthConfig* parse_auth_config(const char* config_file) {
    AuthConfig* config = allocate(sizeof(AuthConfig));
    
    // Parse YAML configuration
    YAMLDocument* doc = yaml_parse_file(config_file);
    
    // Parse global settings
    YAMLNode* global = yaml_get_node(doc, "authentication.global");
    if (global) {
        config->default_method = parse_auth_method(
            yaml_get_string(global, "default_method"));
        config->timeout_seconds = yaml_get_int(global, "timeout_seconds");
        config->max_attempts = yaml_get_int(global, "max_attempts");
        config->lockout_duration = yaml_get_int(global, "lockout_duration_seconds");
    }
    
    // Parse method configurations
    YAMLNode* methods = yaml_get_node(doc, "authentication.methods");
    if (methods) {
        config->methods = parse_auth_methods(methods);
    }
    
    // Parse HBA rules
    YAMLNode* hba = yaml_get_node(doc, "authentication.hba_rules");
    if (hba) {
        config->hba_config = parse_hba_rules(hba);
    }
    
    // Parse security settings
    YAMLNode* security = yaml_get_node(doc, "authentication.security");
    if (security) {
        config->audit_config = parse_audit_config(
            yaml_get_node(security, "audit"));
        config->rate_limit_config = parse_rate_limit_config(
            yaml_get_node(security, "rate_limiting"));
        config->session_config = parse_session_config(
            yaml_get_node(security, "session"));
    }
    
    yaml_free_document(doc);
    return config;
}
```

## 6. Integration Points

### 6.1 C API Integration

```c
// C API authentication functions
typedef struct sb_auth_api {
    // Connection with authentication
    sb_connection_t* (*connect_with_auth)(
        const char* host,
        uint16_t port,
        const char* database,
        const char* username,
        const char* password,
        const char* auth_method
    );
    
    // Token-based authentication
    sb_connection_t* (*connect_with_token)(
        const char* host,
        uint16_t port,
        const char* database,
        const char* token
    );
    
    // Certificate authentication
    sb_connection_t* (*connect_with_cert)(
        const char* host,
        uint16_t port,
        const char* database,
        const char* cert_file,
        const char* key_file
    );
    
    // Change password
    sb_status_t (*change_password)(
        sb_connection_t* conn,
        const char* old_password,
        const char* new_password
    );
    
    // Get authentication info
    sb_auth_info_t* (*get_auth_info)(sb_connection_t* conn);
    
    // Refresh token
    sb_status_t (*refresh_token)(
        sb_connection_t* conn,
        const char* refresh_token
    );
} sb_auth_api_t;
```

### 6.2 Wire Protocol Integration

```c
// Wire protocol authentication messages
typedef struct wire_auth_protocol {
    // Send authentication request
    Status (*send_auth_request)(
        Connection* conn,
        AuthRequestType type,
        const void* data,
        size_t length
    );
    
    // Receive authentication response
    Status (*receive_auth_response)(
        Connection* conn,
        void* buffer,
        size_t* length
    );
    
    // SASL exchange
    Status (*sasl_initial)(
        Connection* conn,
        const char* mechanism,
        const void* initial_response,
        size_t length
    );
    
    Status (*sasl_continue)(
        Connection* conn,
        const void* data,
        size_t length
    );
    
    Status (*sasl_final)(
        Connection* conn
    );
} WireAuthProtocol;
```

## 7. Testing and Validation

### 7.1 Test Framework

```c
// Authentication test suite
typedef struct auth_test_suite {
    // Test cases
    TestCase*           ats_test_cases;        // Array of test cases
    uint32_t            ats_test_count;        // Number of tests
    
    // Test data
    TestUsers*          ats_test_users;        // Test user accounts
    TestCertificates*   ats_test_certs;        // Test certificates
    
    // Results
    TestResults*        ats_results;           // Test results
    
    // Configuration
    TestConfig          ats_config;            // Test configuration
} AuthTestSuite;

// Run authentication tests
TestResults* run_auth_tests(AuthTestSuite* suite) {
    TestResults* results = create_test_results();
    
    for (uint32_t i = 0; i < suite->ats_test_count; i++) {
        TestCase* test = &suite->ats_test_cases[i];
        
        // Setup test environment
        setup_test_environment(test);
        
        // Run test
        TestResult result = run_single_test(test);
        
        // Record result
        record_test_result(results, test, result);
        
        // Cleanup
        cleanup_test_environment(test);
    }
    
    return results;
}

// Test cases
TestCase auth_test_cases[] = {
    // Password authentication
    { "password_valid", test_password_valid },
    { "password_invalid", test_password_invalid },
    { "password_expired", test_password_expired },
    { "password_complexity", test_password_complexity },
    
    // SCRAM authentication
    { "scram_sha256_valid", test_scram_sha256_valid },
    { "scram_sha256_invalid", test_scram_sha256_invalid },
    { "scram_channel_binding", test_scram_channel_binding },
    
    // Certificate authentication
    { "cert_valid", test_cert_valid },
    { "cert_expired", test_cert_expired },
    { "cert_revoked", test_cert_revoked },
    { "cert_chain", test_cert_chain },
    
    // Rate limiting
    { "rate_limit_per_ip", test_rate_limit_per_ip },
    { "rate_limit_per_user", test_rate_limit_per_user },
    { "rate_limit_lockout", test_rate_limit_lockout },
    
    // Session management
    { "session_timeout", test_session_timeout },
    { "session_max_lifetime", test_session_max_lifetime },
    { "session_concurrent", test_session_concurrent },
    
    // Security
    { "timing_attack", test_timing_attack_resistance },
    { "replay_attack", test_replay_attack_prevention },
    { "sql_injection", test_sql_injection_prevention }
};
```

## Implementation Notes

This authentication framework provides:

1. **Pluggable architecture** supporting multiple authentication methods
2. **Host-based authentication** for flexible access control
3. **Comprehensive security features** including rate limiting and audit logging
4. **Performance optimization** through caching and connection pooling
5. **Enterprise features** ready for LDAP, Kerberos, and certificate authentication
6. **Modern standards** support including SCRAM-SHA-256 and OAuth ready
7. **Complete testing framework** for validation and security testing

The framework is designed to be extensible, allowing new authentication methods to be added without modifying the core system.