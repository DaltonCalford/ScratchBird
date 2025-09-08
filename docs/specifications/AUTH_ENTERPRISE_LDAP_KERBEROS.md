# ScratchBird Enterprise Authentication (LDAP/Kerberos)
## Complete Technical Specification

## Overview

This document provides comprehensive specifications for enterprise authentication methods including LDAP, Active Directory, and Kerberos/GSSAPI integration.

## 1. LDAP Authentication

### 1.1 LDAP Configuration

```c
// LDAP configuration structure
typedef struct ldap_config {
    // Server configuration
    char**              lc_server_urls;        // LDAP server URLs
    uint32_t            lc_server_count;       // Number of servers
    LDAPVersion         lc_version;            // LDAP version (2 or 3)
    
    // Bind configuration
    LDAPBindMethod      lc_bind_method;        // Bind method
    char*               lc_bind_dn;            // Bind DN
    char*               lc_bind_password;      // Bind password
    
    // Search configuration
    char*               lc_base_dn;            // Base DN for searches
    char*               lc_search_filter;      // User search filter
    LDAPScope           lc_search_scope;       // Search scope
    char**              lc_search_attributes;  // Attributes to retrieve
    
    // Group configuration
    char*               lc_group_base_dn;      // Group base DN
    char*               lc_group_filter;       // Group membership filter
    char*               lc_group_attribute;    // Group name attribute
    
    // TLS/SSL configuration
    bool                lc_use_tls;            // Use StartTLS
    bool                lc_use_ssl;            // Use LDAPS
    char*               lc_ca_cert_file;       // CA certificate file
    bool                lc_verify_cert;        // Verify server certificate
    
    // Connection pooling
    uint32_t            lc_pool_size;          // Connection pool size
    uint32_t            lc_pool_timeout;       // Pool timeout (seconds)
    
    // Timeouts
    uint32_t            lc_connect_timeout;    // Connect timeout
    uint32_t            lc_search_timeout;     // Search timeout
    uint32_t            lc_bind_timeout;       // Bind timeout
    
    // Cache
    bool                lc_cache_enabled;      // Enable result caching
    uint32_t            lc_cache_ttl;          // Cache TTL (seconds)
} LDAPConfig;

// LDAP bind methods
typedef enum ldap_bind_method {
    LDAP_BIND_SIMPLE,       // Simple bind
    LDAP_BIND_SASL,         // SASL bind
    LDAP_BIND_SASL_GSSAPI,  // SASL/GSSAPI (Kerberos)
    LDAP_BIND_SASL_DIGEST,  // SASL/DIGEST-MD5
    LDAP_BIND_SASL_EXTERNAL // SASL/EXTERNAL (TLS client cert)
} LDAPBindMethod;

// LDAP connection pool
typedef struct ldap_connection_pool {
    // Connections
    LDAP**              lcp_connections;       // Array of connections
    bool*               lcp_in_use;            // In-use flags
    TimestampTz*        lcp_last_used;         // Last used times
    uint32_t            lcp_size;              // Pool size
    
    // Statistics
    uint32_t            lcp_hits;              // Cache hits
    uint32_t            lcp_misses;            // Cache misses
    uint32_t            lcp_timeouts;          // Timeouts
    
    // Configuration
    LDAPConfig*         lcp_config;            // LDAP configuration
    
    // Synchronization
    pthread_mutex_t     lcp_mutex;             // Mutex
} LDAPConnectionPool;

// Initialize LDAP connection
LDAP* init_ldap_connection(LDAPConfig* config) {
    LDAP* ld = NULL;
    int rc;
    
    // Try each server in order
    for (uint32_t i = 0; i < config->lc_server_count; i++) {
        rc = ldap_initialize(&ld, config->lc_server_urls[i]);
        
        if (rc == LDAP_SUCCESS) {
            log_debug("Connected to LDAP server: %s", config->lc_server_urls[i]);
            break;
        }
        
        log_warning("Failed to connect to %s: %s",
                   config->lc_server_urls[i], ldap_err2string(rc));
    }
    
    if (ld == NULL) {
        return NULL;
    }
    
    // Set LDAP version
    int version = (config->lc_version == LDAP_VERSION_3) ? 
                  LDAP_VERSION3 : LDAP_VERSION2;
    ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &version);
    
    // Set timeouts
    struct timeval tv;
    tv.tv_sec = config->lc_connect_timeout;
    tv.tv_usec = 0;
    ldap_set_option(ld, LDAP_OPT_NETWORK_TIMEOUT, &tv);
    
    // Configure TLS
    if (config->lc_use_tls) {
        // Set TLS options
        if (config->lc_ca_cert_file) {
            ldap_set_option(ld, LDAP_OPT_X_TLS_CACERTFILE, 
                          config->lc_ca_cert_file);
        }
        
        int require_cert = config->lc_verify_cert ? 
                          LDAP_OPT_X_TLS_DEMAND : LDAP_OPT_X_TLS_ALLOW;
        ldap_set_option(ld, LDAP_OPT_X_TLS_REQUIRE_CERT, &require_cert);
        
        // Start TLS
        rc = ldap_start_tls_s(ld, NULL, NULL);
        if (rc != LDAP_SUCCESS) {
            log_error("StartTLS failed: %s", ldap_err2string(rc));
            ldap_unbind_ext_s(ld, NULL, NULL);
            return NULL;
        }
    }
    
    return ld;
}
```

### 1.2 LDAP Authentication Flow

```c
// LDAP authentication method
typedef struct ldap_auth_method {
    AuthMethod          lam_base;              // Base method
    LDAPConfig          lam_config;            // LDAP configuration
    LDAPConnectionPool* lam_pool;              // Connection pool
    LDAPCache*          lam_cache;             // Result cache
} LDAPAuthMethod;

// LDAP authentication
Status ldap_authenticate(
    LDAPAuthMethod* method,
    const char* username,
    const char* password)
{
    // Get connection from pool
    LDAP* ld = get_ldap_connection(method->lam_pool);
    if (ld == NULL) {
        return STATUS_LDAP_CONNECT_FAILED;
    }
    
    // Search for user
    char* user_dn = NULL;
    Status status = ldap_search_user(ld, &method->lam_config, 
                                    username, &user_dn);
    
    if (status != STATUS_OK) {
        return_ldap_connection(method->lam_pool, ld);
        return status;
    }
    
    // Attempt bind with user credentials
    struct berval cred;
    cred.bv_val = (char*)password;
    cred.bv_len = strlen(password);
    
    int rc = ldap_sasl_bind_s(ld, user_dn, LDAP_SASL_SIMPLE, 
                              &cred, NULL, NULL, NULL);
    
    if (rc != LDAP_SUCCESS) {
        log_info("LDAP bind failed for %s: %s", 
                username, ldap_err2string(rc));
        free(user_dn);
        return_ldap_connection(method->lam_pool, ld);
        return STATUS_AUTHENTICATION_FAILED;
    }
    
    // Get user groups if configured
    char** groups = NULL;
    uint32_t group_count = 0;
    
    if (method->lam_config.lc_group_filter) {
        ldap_get_user_groups(ld, &method->lam_config, 
                           user_dn, &groups, &group_count);
    }
    
    // Cache successful authentication
    if (method->lam_cache && method->lam_config.lc_cache_enabled) {
        cache_ldap_auth(method->lam_cache, username, 
                       password, groups, group_count);
    }
    
    free(user_dn);
    free_string_array(groups, group_count);
    return_ldap_connection(method->lam_pool, ld);
    
    return STATUS_OK;
}

// Search for user in LDAP
Status ldap_search_user(
    LDAP* ld,
    LDAPConfig* config,
    const char* username,
    char** user_dn)
{
    // Build search filter
    char filter[1024];
    snprintf(filter, sizeof(filter), config->lc_search_filter, username);
    
    // Perform search
    LDAPMessage* result = NULL;
    struct timeval tv = {
        .tv_sec = config->lc_search_timeout,
        .tv_usec = 0
    };
    
    int rc = ldap_search_ext_s(
        ld,
        config->lc_base_dn,
        config->lc_search_scope,
        filter,
        config->lc_search_attributes,
        0,  // attrs only
        NULL,  // server controls
        NULL,  // client controls
        &tv,
        1,  // size limit
        &result
    );
    
    if (rc != LDAP_SUCCESS) {
        log_error("LDAP search failed: %s", ldap_err2string(rc));
        return STATUS_LDAP_SEARCH_FAILED;
    }
    
    // Check number of entries
    int count = ldap_count_entries(ld, result);
    if (count == 0) {
        ldap_msgfree(result);
        return STATUS_USER_NOT_FOUND;
    }
    
    if (count > 1) {
        log_warning("Multiple LDAP entries found for %s", username);
        ldap_msgfree(result);
        return STATUS_AMBIGUOUS_USER;
    }
    
    // Get user DN
    LDAPMessage* entry = ldap_first_entry(ld, result);
    *user_dn = ldap_get_dn(ld, entry);
    
    ldap_msgfree(result);
    return STATUS_OK;
}
```

### 1.3 Active Directory Integration

```c
// Active Directory specific configuration
typedef struct ad_config {
    // Base LDAP config
    LDAPConfig          ac_ldap_config;        // LDAP configuration
    
    // AD-specific settings
    char*               ac_domain;             // AD domain
    char*               ac_default_domain;     // Default domain for users
    bool                ac_use_upn;            // Use UPN for authentication
    
    // Global Catalog
    bool                ac_use_gc;             // Use Global Catalog
    uint16_t            ac_gc_port;            // GC port (3268/3269)
    
    // Nested groups
    bool                ac_resolve_nested;     // Resolve nested groups
    uint32_t            ac_max_nesting;        // Max nesting level
    
    // Account flags
    bool                ac_check_account_flags; // Check userAccountControl
    uint32_t            ac_required_flags;     // Required flags
    uint32_t            ac_forbidden_flags;    // Forbidden flags
} ADConfig;

// Active Directory authentication
Status ad_authenticate(
    ADConfig* config,
    const char* username,
    const char* password)
{
    // Format username as UPN if needed
    char formatted_username[512];
    
    if (config->ac_use_upn && !strchr(username, '@')) {
        snprintf(formatted_username, sizeof(formatted_username),
                "%s@%s", username, config->ac_default_domain);
    } else {
        strncpy(formatted_username, username, sizeof(formatted_username));
    }
    
    // Connect to AD
    LDAP* ld = init_ldap_connection(&config->ac_ldap_config);
    if (ld == NULL) {
        return STATUS_LDAP_CONNECT_FAILED;
    }
    
    // Bind with user credentials
    struct berval cred;
    cred.bv_val = (char*)password;
    cred.bv_len = strlen(password);
    
    int rc = ldap_sasl_bind_s(ld, formatted_username, LDAP_SASL_SIMPLE,
                              &cred, NULL, NULL, NULL);
    
    if (rc != LDAP_SUCCESS) {
        log_info("AD bind failed for %s: %s",
                formatted_username, ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL);
        return STATUS_AUTHENTICATION_FAILED;
    }
    
    // Check account flags if configured
    if (config->ac_check_account_flags) {
        uint32_t account_flags;
        Status status = get_ad_account_flags(ld, formatted_username, 
                                            &account_flags);
        
        if (status != STATUS_OK) {
            ldap_unbind_ext_s(ld, NULL, NULL);
            return status;
        }
        
        // Check if account is disabled
        if (account_flags & ADS_UF_ACCOUNTDISABLE) {
            log_warning("AD account disabled: %s", formatted_username);
            ldap_unbind_ext_s(ld, NULL, NULL);
            return STATUS_ACCOUNT_DISABLED;
        }
        
        // Check if password expired
        if (account_flags & ADS_UF_PASSWORD_EXPIRED) {
            log_warning("AD password expired: %s", formatted_username);
            ldap_unbind_ext_s(ld, NULL, NULL);
            return STATUS_PASSWORD_EXPIRED;
        }
        
        // Check required flags
        if ((account_flags & config->ac_required_flags) != 
            config->ac_required_flags) {
            ldap_unbind_ext_s(ld, NULL, NULL);
            return STATUS_INSUFFICIENT_PRIVILEGES;
        }
        
        // Check forbidden flags
        if (account_flags & config->ac_forbidden_flags) {
            ldap_unbind_ext_s(ld, NULL, NULL);
            return STATUS_FORBIDDEN_ACCOUNT_TYPE;
        }
    }
    
    ldap_unbind_ext_s(ld, NULL, NULL);
    return STATUS_OK;
}

// Get AD user groups with nested group resolution
Status get_ad_user_groups(
    LDAP* ld,
    ADConfig* config,
    const char* user_dn,
    char*** groups,
    uint32_t* group_count)
{
    if (!config->ac_resolve_nested) {
        // Simple group membership
        return ldap_get_user_groups(ld, &config->ac_ldap_config,
                                   user_dn, groups, group_count);
    }
    
    // Use LDAP_MATCHING_RULE_IN_CHAIN for nested groups
    char filter[1024];
    snprintf(filter, sizeof(filter),
            "(member:1.2.840.113556.1.4.1941:=%s)", user_dn);
    
    LDAPMessage* result = NULL;
    int rc = ldap_search_ext_s(
        ld,
        config->ac_ldap_config.lc_group_base_dn,
        LDAP_SCOPE_SUBTREE,
        filter,
        NULL,  // All attributes
        0,
        NULL, NULL, NULL,
        LDAP_NO_LIMIT,
        &result
    );
    
    if (rc != LDAP_SUCCESS) {
        return STATUS_LDAP_SEARCH_FAILED;
    }
    
    // Process results
    *group_count = ldap_count_entries(ld, result);
    *groups = calloc(*group_count, sizeof(char*));
    
    LDAPMessage* entry;
    int i = 0;
    
    for (entry = ldap_first_entry(ld, result);
         entry != NULL;
         entry = ldap_next_entry(ld, entry)) {
        
        char* dn = ldap_get_dn(ld, entry);
        (*groups)[i++] = extract_cn_from_dn(dn);
        ldap_memfree(dn);
    }
    
    ldap_msgfree(result);
    return STATUS_OK;
}
```

## 2. Kerberos/GSSAPI Authentication

### 2.1 Kerberos Configuration

```c
// Kerberos configuration
typedef struct krb5_config {
    // Realm configuration
    char*               kc_default_realm;      // Default realm
    char**              kc_realms;             // Configured realms
    uint32_t            kc_realm_count;        // Number of realms
    
    // KDC configuration
    char**              kc_kdc_servers;        // KDC servers
    uint32_t            kc_kdc_count;          // Number of KDCs
    
    // Service principal
    char*               kc_service_principal;  // Service principal name
    char*               kc_keytab_file;        // Keytab file path
    
    // Options
    bool                kc_forwardable;        // Forwardable tickets
    bool                kc_proxiable;          // Proxiable tickets
    bool                kc_renewable;          // Renewable tickets
    uint32_t            kc_renew_lifetime;     // Renew lifetime
    uint32_t            kc_ticket_lifetime;    // Ticket lifetime
    
    // Cross-realm
    bool                kc_cross_realm;        // Allow cross-realm
    char**              kc_trusted_realms;     // Trusted realms
    
    // Cache
    char*               kc_ccache_dir;         // Credential cache directory
    CcacheType          kc_ccache_type;        // Cache type (FILE, MEMORY)
    
    // Mapping
    char*               kc_principal_map_file; // Principal to user mapping
    bool                kc_strip_realm;        // Strip realm from principal
} Krb5Config;

// GSSAPI context
typedef struct gssapi_context {
    // GSSAPI handles
    gss_ctx_id_t        gc_context;            // Security context
    gss_cred_id_t       gc_server_creds;       // Server credentials
    gss_name_t          gc_client_name;        // Client name
    gss_name_t          gc_service_name;       // Service name
    
    // Flags
    OM_uint32           gc_ret_flags;          // Returned flags
    OM_uint32           gc_req_flags;          // Requested flags
    
    // State
    GssapiState         gc_state;              // Current state
    
    // Delegation
    gss_cred_id_t       gc_delegated_creds;    // Delegated credentials
    
    // Error handling
    OM_uint32           gc_major_status;       // Major status code
    OM_uint32           gc_minor_status;       // Minor status code
    char                gc_error_string[512];  // Error description
} GssapiContext;

// Initialize Kerberos context
krb5_context init_krb5_context(Krb5Config* config) {
    krb5_context ctx;
    krb5_error_code ret;
    
    // Initialize context
    ret = krb5_init_context(&ctx);
    if (ret) {
        log_error("krb5_init_context failed: %s", 
                 krb5_get_error_message(ctx, ret));
        return NULL;
    }
    
    // Set default realm
    if (config->kc_default_realm) {
        ret = krb5_set_default_realm(ctx, config->kc_default_realm);
        if (ret) {
            log_error("krb5_set_default_realm failed: %s",
                     krb5_get_error_message(ctx, ret));
            krb5_free_context(ctx);
            return NULL;
        }
    }
    
    // Load keytab
    krb5_keytab keytab;
    if (config->kc_keytab_file) {
        ret = krb5_kt_resolve(ctx, config->kc_keytab_file, &keytab);
        if (ret) {
            log_error("krb5_kt_resolve failed: %s",
                     krb5_get_error_message(ctx, ret));
            krb5_free_context(ctx);
            return NULL;
        }
        
        krb5_kt_close(ctx, keytab);
    }
    
    return ctx;
}
```

### 2.2 GSSAPI Authentication Flow

```c
// GSSAPI authentication method
typedef struct gssapi_auth_method {
    AuthMethod          gam_base;              // Base method
    Krb5Config          gam_krb5_config;       // Kerberos config
    krb5_context        gam_krb5_context;      // Kerberos context
    gss_cred_id_t       gam_server_creds;      // Server credentials
} GssapiAuthMethod;

// Initialize GSSAPI server credentials
Status init_gssapi_server_creds(
    GssapiAuthMethod* method)
{
    OM_uint32 major, minor;
    gss_buffer_desc name_buf;
    gss_name_t server_name;
    
    // Import service name
    name_buf.value = method->gam_krb5_config.kc_service_principal;
    name_buf.length = strlen(name_buf.value);
    
    major = gss_import_name(&minor, &name_buf,
                           GSS_C_NT_HOSTBASED_SERVICE,
                           &server_name);
    
    if (GSS_ERROR(major)) {
        log_gssapi_error("gss_import_name", major, minor);
        return STATUS_GSSAPI_ERROR;
    }
    
    // Acquire credentials
    major = gss_acquire_cred(&minor,
                            server_name,
                            GSS_C_INDEFINITE,
                            GSS_C_NO_OID_SET,
                            GSS_C_ACCEPT,
                            &method->gam_server_creds,
                            NULL,
                            NULL);
    
    gss_release_name(&minor, &server_name);
    
    if (GSS_ERROR(major)) {
        log_gssapi_error("gss_acquire_cred", major, minor);
        return STATUS_GSSAPI_ERROR;
    }
    
    return STATUS_OK;
}

// Process GSSAPI authentication
Status gssapi_authenticate(
    GssapiAuthMethod* method,
    GssapiContext* ctx,
    const void* input_token,
    size_t input_len,
    void** output_token,
    size_t* output_len)
{
    OM_uint32 major, minor;
    gss_buffer_desc input_buf, output_buf;
    
    // Setup input buffer
    input_buf.value = (void*)input_token;
    input_buf.length = input_len;
    
    // Accept security context
    major = gss_accept_sec_context(
        &minor,
        &ctx->gc_context,
        method->gam_server_creds,
        &input_buf,
        GSS_C_NO_CHANNEL_BINDINGS,
        &ctx->gc_client_name,
        NULL,  // mech_type
        &output_buf,
        &ctx->gc_ret_flags,
        NULL,  // time_rec
        &ctx->gc_delegated_creds
    );
    
    if (output_buf.length > 0) {
        *output_token = malloc(output_buf.length);
        memcpy(*output_token, output_buf.value, output_buf.length);
        *output_len = output_buf.length;
        gss_release_buffer(&minor, &output_buf);
    } else {
        *output_token = NULL;
        *output_len = 0;
    }
    
    if (GSS_ERROR(major)) {
        log_gssapi_error("gss_accept_sec_context", major, minor);
        ctx->gc_major_status = major;
        ctx->gc_minor_status = minor;
        return STATUS_GSSAPI_ERROR;
    }
    
    if (major == GSS_S_COMPLETE) {
        // Authentication complete
        ctx->gc_state = GSSAPI_STATE_COMPLETE;
        
        // Get client principal
        gss_buffer_desc name_buf;
        major = gss_display_name(&minor, ctx->gc_client_name,
                                &name_buf, NULL);
        
        if (GSS_ERROR(major)) {
            log_gssapi_error("gss_display_name", major, minor);
            return STATUS_GSSAPI_ERROR;
        }
        
        // Map principal to database user
        char* username = map_principal_to_user(
            name_buf.value,
            &method->gam_krb5_config
        );
        
        gss_release_buffer(&minor, &name_buf);
        
        if (username == NULL) {
            return STATUS_PRINCIPAL_MAP_FAILED;
        }
        
        // Store username in context
        strncpy(ctx->gc_username, username, sizeof(ctx->gc_username));
        free(username);
        
        // Check delegation if configured
        if (ctx->gc_delegated_creds != GSS_C_NO_CREDENTIAL) {
            handle_delegated_credentials(ctx);
        }
        
        return STATUS_OK;
    } else if (major == GSS_S_CONTINUE_NEEDED) {
        // More tokens needed
        ctx->gc_state = GSSAPI_STATE_CONTINUE;
        return STATUS_CONTINUE;
    }
    
    return STATUS_GSSAPI_ERROR;
}

// Map Kerberos principal to database user
char* map_principal_to_user(
    const char* principal,
    Krb5Config* config)
{
    char* username = NULL;
    
    // Strip realm if configured
    if (config->kc_strip_realm) {
        char* at = strchr(principal, '@');
        if (at) {
            size_t len = at - principal;
            username = malloc(len + 1);
            strncpy(username, principal, len);
            username[len] = '\0';
        } else {
            username = strdup(principal);
        }
    } else {
        username = strdup(principal);
    }
    
    // Check mapping file if configured
    if (config->kc_principal_map_file) {
        char* mapped = lookup_principal_mapping(
            config->kc_principal_map_file,
            principal
        );
        
        if (mapped) {
            free(username);
            username = mapped;
        }
    }
    
    return username;
}

// Handle delegated credentials
Status handle_delegated_credentials(GssapiContext* ctx) {
    OM_uint32 major, minor;
    krb5_context krb_ctx;
    krb5_ccache ccache;
    krb5_error_code ret;
    
    // Initialize Kerberos context
    ret = krb5_init_context(&krb_ctx);
    if (ret) {
        return STATUS_KRB5_ERROR;
    }
    
    // Create credential cache for delegated credentials
    char ccache_name[256];
    snprintf(ccache_name, sizeof(ccache_name),
            "MEMORY:delegated_%s_%ld",
            ctx->gc_username, (long)getpid());
    
    ret = krb5_cc_resolve(krb_ctx, ccache_name, &ccache);
    if (ret) {
        krb5_free_context(krb_ctx);
        return STATUS_KRB5_ERROR;
    }
    
    // Store delegated credentials
    major = gss_krb5_copy_ccache(&minor, ctx->gc_delegated_creds, ccache);
    
    if (GSS_ERROR(major)) {
        krb5_cc_close(krb_ctx, ccache);
        krb5_free_context(krb_ctx);
        return STATUS_GSSAPI_ERROR;
    }
    
    // Store ccache name for later use
    strncpy(ctx->gc_ccache_name, ccache_name, sizeof(ctx->gc_ccache_name));
    
    krb5_cc_close(krb_ctx, ccache);
    krb5_free_context(krb_ctx);
    
    return STATUS_OK;
}
```

### 2.3 SPNEGO Negotiation

```c
// SPNEGO (Simple and Protected GSSAPI Negotiation)
typedef struct spnego_context {
    // Negotiation state
    SpnegoState         sc_state;              // Current state
    
    // Supported mechanisms
    gss_OID_set         sc_mech_set;           // Supported mechanisms
    gss_OID             sc_selected_mech;      // Selected mechanism
    
    // Sub-context
    void*               sc_mech_context;       // Mechanism-specific context
    
    // Tokens
    uint8_t*            sc_neg_token_init;     // NegTokenInit
    size_t              sc_neg_token_init_len; // Token length
} SpnegoContext;

// Process SPNEGO token
Status process_spnego_token(
    SpnegoContext* ctx,
    const uint8_t* input_token,
    size_t input_len,
    uint8_t** output_token,
    size_t* output_len)
{
    // Parse SPNEGO token
    SpnegoToken* token = parse_spnego_token(input_token, input_len);
    
    if (token == NULL) {
        return STATUS_INVALID_TOKEN;
    }
    
    switch (token->type) {
        case SPNEGO_NEG_TOKEN_INIT:
            return process_neg_token_init(ctx, token, 
                                         output_token, output_len);
            
        case SPNEGO_NEG_TOKEN_RESP:
            return process_neg_token_resp(ctx, token,
                                        output_token, output_len);
            
        default:
            free_spnego_token(token);
            return STATUS_INVALID_TOKEN_TYPE;
    }
}

// Process NegTokenInit
Status process_neg_token_init(
    SpnegoContext* ctx,
    SpnegoToken* token,
    uint8_t** output_token,
    size_t* output_len)
{
    // Select mechanism
    gss_OID selected = NULL;
    
    for (int i = 0; i < token->mech_types_count; i++) {
        if (is_supported_mech(token->mech_types[i])) {
            selected = token->mech_types[i];
            break;
        }
    }
    
    if (selected == NULL) {
        // No supported mechanism
        return create_neg_token_resp(
            ctx,
            SPNEGO_REJECT,
            NULL,
            output_token,
            output_len
        );
    }
    
    ctx->sc_selected_mech = selected;
    
    // Process mechanism token if present
    if (token->mech_token && token->mech_token_len > 0) {
        // Create mechanism-specific context
        if (oid_equals(selected, GSS_KRB5_OID)) {
            ctx->sc_mech_context = create_krb5_context();
        } else if (oid_equals(selected, GSS_NTLM_OID)) {
            ctx->sc_mech_context = create_ntlm_context();
        }
        
        // Process token with selected mechanism
        uint8_t* mech_output = NULL;
        size_t mech_output_len = 0;
        
        Status status = process_mech_token(
            ctx->sc_mech_context,
            selected,
            token->mech_token,
            token->mech_token_len,
            &mech_output,
            &mech_output_len
        );
        
        // Create response
        SpnegoResult result;
        
        if (status == STATUS_OK) {
            result = SPNEGO_ACCEPT_COMPLETED;
            ctx->sc_state = SPNEGO_STATE_COMPLETE;
        } else if (status == STATUS_CONTINUE) {
            result = SPNEGO_ACCEPT_INCOMPLETE;
            ctx->sc_state = SPNEGO_STATE_CONTINUE;
        } else {
            result = SPNEGO_REJECT;
            ctx->sc_state = SPNEGO_STATE_FAILED;
        }
        
        return create_neg_token_resp(
            ctx,
            result,
            mech_output,
            output_token,
            output_len
        );
    }
    
    // Request mechanism token
    return create_neg_token_resp(
        ctx,
        SPNEGO_ACCEPT_INCOMPLETE,
        NULL,
        output_token,
        output_len
    );
}
```

## 3. Configuration Examples

### 3.1 LDAP Configuration

```yaml
# LDAP authentication configuration
ldap:
  servers:
    - ldaps://ldap1.example.com:636
    - ldaps://ldap2.example.com:636
  
  version: 3
  
  # Bind configuration
  bind:
    method: simple  # simple, sasl, sasl-gssapi
    dn: cn=scratchbird,ou=services,dc=example,dc=com
    password_file: /secure/ldap_bind.pwd
  
  # User search
  search:
    base_dn: ou=users,dc=example,dc=com
    filter: "(&(objectClass=person)(uid=%s))"
    scope: subtree  # base, onelevel, subtree
    attributes:
      - uid
      - cn
      - mail
      - memberOf
  
  # Group configuration
  groups:
    base_dn: ou=groups,dc=example,dc=com
    filter: "(member=%s)"
    attribute: cn
    resolve_nested: false
  
  # TLS/SSL
  tls:
    enabled: true
    ca_cert: /etc/scratchbird/ca-bundle.crt
    verify_cert: true
    starttls: false  # Use LDAPS instead
  
  # Connection pooling
  pool:
    size: 10
    timeout: 300
    idle_timeout: 60
  
  # Timeouts (seconds)
  timeouts:
    connect: 10
    search: 30
    bind: 10
  
  # Cache
  cache:
    enabled: true
    ttl: 300
    max_entries: 1000

# Active Directory specific
active_directory:
  enabled: true
  domain: EXAMPLE.COM
  default_domain: example.com
  use_upn: true
  
  # Global Catalog
  use_global_catalog: true
  gc_port: 3269  # SSL
  
  # Nested groups
  resolve_nested_groups: true
  max_nesting_level: 5
  
  # Account validation
  check_account_flags: true
  required_flags: 0x00000200  # NORMAL_ACCOUNT
  forbidden_flags: 0x00000002  # ACCOUNTDISABLE
```

### 3.2 Kerberos Configuration

```yaml
# Kerberos/GSSAPI configuration
kerberos:
  # Realm configuration
  default_realm: EXAMPLE.COM
  realms:
    - EXAMPLE.COM
    - TRUST.COM
  
  # KDC servers
  kdc_servers:
    - kdc1.example.com
    - kdc2.example.com
  
  # Service principal
  service_principal: postgres/db.example.com@EXAMPLE.COM
  keytab_file: /etc/scratchbird/scratchbird.keytab
  
  # Ticket options
  forwardable: true
  proxiable: false
  renewable: true
  renew_lifetime: 604800  # 7 days
  ticket_lifetime: 86400  # 1 day
  
  # Cross-realm
  cross_realm: true
  trusted_realms:
    - TRUST.COM
    - PARTNER.ORG
  
  # Credential cache
  ccache:
    type: FILE  # FILE, MEMORY
    directory: /var/run/scratchbird/ccache
  
  # Principal mapping
  principal_mapping:
    file: /etc/scratchbird/krb5_map.conf
    strip_realm: true
    
  # SPNEGO
  spnego:
    enabled: true
    mechanisms:
      - kerberos
      - ntlm  # For Windows clients
```

## 4. Testing

### 4.1 LDAP Test Suite

```c
// LDAP authentication tests
bool test_ldap_authentication() {
    LDAPConfig config = {
        .lc_server_urls = (char*[]){"ldap://localhost"},
        .lc_server_count = 1,
        .lc_base_dn = "dc=test,dc=com",
        .lc_search_filter = "(uid=%s)",
        .lc_bind_method = LDAP_BIND_SIMPLE
    };
    
    // Test successful authentication
    Status status = ldap_authenticate_user(&config, "testuser", "password");
    assert(status == STATUS_OK);
    
    // Test failed authentication
    status = ldap_authenticate_user(&config, "testuser", "wrongpass");
    assert(status == STATUS_AUTHENTICATION_FAILED);
    
    // Test user not found
    status = ldap_authenticate_user(&config, "nonexistent", "password");
    assert(status == STATUS_USER_NOT_FOUND);
    
    return true;
}

// Kerberos authentication tests
bool test_kerberos_authentication() {
    // Initialize test Kerberos environment
    setup_test_kdc();
    
    Krb5Config config = {
        .kc_default_realm = "TEST.COM",
        .kc_service_principal = "postgres/localhost@TEST.COM",
        .kc_keytab_file = "test.keytab"
    };
    
    // Create test context
    GssapiContext ctx = {0};
    
    // Get client token
    uint8_t* client_token = get_test_client_token("testuser@TEST.COM");
    size_t client_token_len = get_test_token_length();
    
    // Process authentication
    uint8_t* server_token = NULL;
    size_t server_token_len = 0;
    
    Status status = gssapi_authenticate(
        &config, &ctx,
        client_token, client_token_len,
        &server_token, &server_token_len
    );
    
    assert(status == STATUS_OK || status == STATUS_CONTINUE);
    
    cleanup_test_kdc();
    return true;
}
```

## Implementation Notes

This enterprise authentication implementation provides:

1. **Complete LDAP/Active Directory integration** with connection pooling and caching
2. **Full Kerberos/GSSAPI support** with delegation and cross-realm authentication  
3. **SPNEGO negotiation** for automatic mechanism selection
4. **Flexible principal/user mapping** with multiple mapping methods
5. **Nested group resolution** for Active Directory
6. **Comprehensive error handling** and logging
7. **Performance optimization** through pooling and caching

The system supports standard enterprise authentication infrastructure while maintaining security and performance.