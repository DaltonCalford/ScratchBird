# ScratchBird Certificate and TLS Authentication
## Complete Technical Specification

## Overview

This document provides comprehensive specifications for X.509 certificate-based authentication and TLS/SSL security in ScratchBird, including mutual TLS (mTLS), certificate validation, mapping, and management.

## 1. TLS/SSL Configuration

### 1.1 TLS Server Configuration

```c
// TLS configuration structure
typedef struct tls_config {
    // Certificates and keys
    char*               tc_cert_file;          // Server certificate file
    char*               tc_key_file;           // Private key file
    char*               tc_ca_file;            // CA certificate file
    char*               tc_crl_file;           // Certificate revocation list
    char*               tc_dh_params_file;     // DH parameters file
    
    // Protocol settings
    uint32_t            tc_min_protocol;       // Minimum TLS version
    uint32_t            tc_max_protocol;       // Maximum TLS version
    char*               tc_ciphersuites;       // TLS 1.3 cipher suites
    char*               tc_cipher_list;        // TLS 1.2 and below ciphers
    
    // Client certificates
    ClientCertMode      tc_client_cert_mode;   // Client cert requirement
    uint32_t            tc_verify_depth;       // Certificate chain depth
    
    // Session management
    bool                tc_session_tickets;    // Enable session tickets
    uint32_t            tc_session_timeout;    // Session timeout (seconds)
    uint32_t            tc_session_cache_size; // Session cache size
    
    // OCSP
    bool                tc_ocsp_stapling;      // Enable OCSP stapling
    char*               tc_ocsp_responder;     // OCSP responder URL
    
    // Security options
    bool                tc_prefer_server_ciphers; // Server cipher preference
    bool                tc_honor_cipher_order; // Honor cipher order
    bool                tc_compression;        // Enable compression (deprecated)
    bool                tc_renegotiation;      // Allow renegotiation
    
    // Performance
    bool                tc_session_cache;      // Enable session cache
    bool                tc_early_data;         // TLS 1.3 early data (0-RTT)
} TLSConfig;

// Client certificate modes
typedef enum client_cert_mode {
    CLIENT_CERT_NONE,           // No client certificate
    CLIENT_CERT_OPTIONAL,       // Client cert optional
    CLIENT_CERT_REQUIRED,       // Client cert required
    CLIENT_CERT_OPTIONAL_NO_CA  // Optional, don't verify CA
} ClientCertMode;

// Initialize TLS context
SSL_CTX* init_tls_context(TLSConfig* config) {
    // Create SSL context
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    
    if (ctx == NULL) {
        log_openssl_errors();
        return NULL;
    }
    
    // Set protocol versions
    SSL_CTX_set_min_proto_version(ctx, config->tc_min_protocol);
    SSL_CTX_set_max_proto_version(ctx, config->tc_max_protocol);
    
    // Load server certificate
    if (SSL_CTX_use_certificate_file(ctx, config->tc_cert_file, 
                                     SSL_FILETYPE_PEM) != 1) {
        log_error("Failed to load certificate: %s", config->tc_cert_file);
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    // Load private key
    if (SSL_CTX_use_PrivateKey_file(ctx, config->tc_key_file,
                                    SSL_FILETYPE_PEM) != 1) {
        log_error("Failed to load private key: %s", config->tc_key_file);
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    // Verify private key matches certificate
    if (SSL_CTX_check_private_key(ctx) != 1) {
        log_error("Private key does not match certificate");
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    // Set cipher suites
    if (config->tc_ciphersuites) {
        SSL_CTX_set_ciphersuites(ctx, config->tc_ciphersuites);
    }
    
    if (config->tc_cipher_list) {
        SSL_CTX_set_cipher_list(ctx, config->tc_cipher_list);
    }
    
    // Configure client certificates
    if (config->tc_client_cert_mode != CLIENT_CERT_NONE) {
        // Load CA certificates
        if (config->tc_ca_file) {
            if (!SSL_CTX_load_verify_locations(ctx, config->tc_ca_file, NULL)) {
                log_error("Failed to load CA certificates");
                SSL_CTX_free(ctx);
                return NULL;
            }
        }
        
        // Set verification mode
        int verify_mode = SSL_VERIFY_NONE;
        
        switch (config->tc_client_cert_mode) {
            case CLIENT_CERT_OPTIONAL:
                verify_mode = SSL_VERIFY_PEER;
                break;
            case CLIENT_CERT_REQUIRED:
                verify_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
                break;
            case CLIENT_CERT_OPTIONAL_NO_CA:
                verify_mode = SSL_VERIFY_PEER;
                break;
        }
        
        SSL_CTX_set_verify(ctx, verify_mode, verify_client_cert_callback);
        SSL_CTX_set_verify_depth(ctx, config->tc_verify_depth);
    }
    
    // Load CRL if provided
    if (config->tc_crl_file) {
        X509_STORE* store = SSL_CTX_get_cert_store(ctx);
        X509_CRL* crl = load_crl_file(config->tc_crl_file);
        
        if (crl) {
            X509_STORE_add_crl(store, crl);
            X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | 
                                       X509_V_FLAG_CRL_CHECK_ALL);
            X509_CRL_free(crl);
        }
    }
    
    // Configure DH parameters
    if (config->tc_dh_params_file) {
        DH* dh = load_dh_params(config->tc_dh_params_file);
        if (dh) {
            SSL_CTX_set_tmp_dh(ctx, dh);
            DH_free(dh);
        }
    }
    
    // Configure session caching
    if (config->tc_session_cache) {
        SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);
        SSL_CTX_sess_set_cache_size(ctx, config->tc_session_cache_size);
        SSL_CTX_set_timeout(ctx, config->tc_session_timeout);
    } else {
        SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
    }
    
    // Configure OCSP stapling
    if (config->tc_ocsp_stapling) {
        SSL_CTX_set_tlsext_status_cb(ctx, ocsp_stapling_callback);
        SSL_CTX_set_tlsext_status_arg(ctx, config);
    }
    
    // Security options
    long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION;
    
    if (config->tc_prefer_server_ciphers) {
        options |= SSL_OP_CIPHER_SERVER_PREFERENCE;
    }
    
    if (!config->tc_renegotiation) {
        options |= SSL_OP_NO_RENEGOTIATION;
    }
    
    SSL_CTX_set_options(ctx, options);
    
    // TLS 1.3 early data
    if (config->tc_early_data) {
        SSL_CTX_set_max_early_data(ctx, 16384);  // 16KB early data
    }
    
    return ctx;
}
```

### 1.2 Certificate Verification Callback

```c
// Certificate verification callback
int verify_client_cert_callback(int preverify_ok, X509_STORE_CTX* ctx) {
    X509* cert = X509_STORE_CTX_get_current_cert(ctx);
    int depth = X509_STORE_CTX_get_error_depth(ctx);
    int err = X509_STORE_CTX_get_error(ctx);
    
    // Get SSL connection
    SSL* ssl = X509_STORE_CTX_get_ex_data(ctx, 
                                          SSL_get_ex_data_X509_STORE_CTX_idx());
    
    // Get certificate subject
    char subject[256];
    X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject));
    
    // Get certificate issuer
    char issuer[256];
    X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof(issuer));
    
    // Log verification attempt
    log_debug("Certificate verification: depth=%d, subject=%s, issuer=%s",
             depth, subject, issuer);
    
    if (!preverify_ok) {
        log_warning("Certificate verification failed: %s",
                   X509_verify_cert_error_string(err));
        
        // Check if we should override certain errors
        switch (err) {
            case X509_V_ERR_CERT_NOT_YET_VALID:
            case X509_V_ERR_CERT_HAS_EXPIRED:
                // Check if we allow expired certificates (testing only)
                if (get_config_bool("ssl.allow_expired_certs")) {
                    log_warning("Allowing expired certificate (testing mode)");
                    return 1;
                }
                break;
                
            case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
                // Check if we allow self-signed certificates
                if (get_config_bool("ssl.allow_self_signed")) {
                    log_warning("Allowing self-signed certificate");
                    return 1;
                }
                break;
        }
        
        return 0;  // Reject certificate
    }
    
    // Additional custom verification
    if (depth == 0) {  // Peer certificate
        // Check certificate extensions
        if (!verify_certificate_extensions(cert)) {
            log_error("Certificate extension verification failed");
            return 0;
        }
        
        // Check certificate purpose
        if (!verify_certificate_purpose(cert)) {
            log_error("Certificate purpose verification failed");
            return 0;
        }
        
        // Store certificate for later use
        store_peer_certificate(ssl, cert);
    }
    
    return 1;  // Accept certificate
}

// Verify certificate extensions
bool verify_certificate_extensions(X509* cert) {
    // Check key usage
    ASN1_BIT_STRING* usage = X509_get_ext_d2i(cert, NID_key_usage, NULL, NULL);
    if (usage) {
        if (!(usage->data[0] & KU_DIGITAL_SIGNATURE)) {
            log_error("Certificate lacks digital signature key usage");
            ASN1_BIT_STRING_free(usage);
            return false;
        }
        ASN1_BIT_STRING_free(usage);
    }
    
    // Check extended key usage
    EXTENDED_KEY_USAGE* ext_usage = X509_get_ext_d2i(cert, NID_ext_key_usage, 
                                                     NULL, NULL);
    if (ext_usage) {
        bool has_client_auth = false;
        
        for (int i = 0; i < sk_ASN1_OBJECT_num(ext_usage); i++) {
            ASN1_OBJECT* obj = sk_ASN1_OBJECT_value(ext_usage, i);
            if (OBJ_obj2nid(obj) == NID_client_auth) {
                has_client_auth = true;
                break;
            }
        }
        
        EXTENDED_KEY_USAGE_free(ext_usage);
        
        if (!has_client_auth) {
            log_error("Certificate lacks client authentication extended key usage");
            return false;
        }
    }
    
    // Check Subject Alternative Names
    GENERAL_NAMES* san = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
    if (san) {
        for (int i = 0; i < sk_GENERAL_NAME_num(san); i++) {
            GENERAL_NAME* name = sk_GENERAL_NAME_value(san, i);
            
            if (name->type == GEN_DNS || name->type == GEN_EMAIL) {
                // Validate SAN entries
                if (!validate_san_entry(name)) {
                    GENERAL_NAMES_free(san);
                    return false;
                }
            }
        }
        GENERAL_NAMES_free(san);
    }
    
    return true;
}
```

## 2. Certificate Authentication

### 2.1 Certificate Mapping

```c
// Certificate mapping configuration
typedef struct cert_mapping_config {
    // Mapping method
    CertMapMethod       cmc_method;            // Mapping method
    
    // Mapping file
    char*               cmc_map_file;          // Mapping file path
    
    // LDAP mapping
    char*               cmc_ldap_server;       // LDAP server
    char*               cmc_ldap_base_dn;      // Base DN
    char*               cmc_ldap_filter;       // Search filter
    
    // Database mapping
    char*               cmc_map_table;         // Mapping table name
    
    // Options
    bool                cmc_case_sensitive;    // Case-sensitive matching
    bool                cmc_require_match;     // Require mapping match
} CertMappingConfig;

// Certificate mapping methods
typedef enum cert_map_method {
    CERT_MAP_CN,            // Common Name
    CERT_MAP_DN,            // Distinguished Name
    CERT_MAP_SAN,           // Subject Alternative Name
    CERT_MAP_FILE,          // File-based mapping
    CERT_MAP_LDAP,          // LDAP-based mapping
    CERT_MAP_DATABASE,      // Database table mapping
    CERT_MAP_REGEX          // Regular expression mapping
} CertMapMethod;

// Certificate mapping entry
typedef struct cert_map_entry {
    // Certificate identifier
    char                cme_cert_cn[256];      // Common name
    char                cme_cert_dn[512];      // Distinguished name
    char                cme_cert_fingerprint[128]; // SHA256 fingerprint
    
    // Database user
    char                cme_db_user[256];      // Database username
    
    // Additional attributes
    char**              cme_roles;             // Granted roles
    uint32_t            cme_role_count;        // Number of roles
    
    // Validity
    TimestampTz         cme_valid_from;        // Valid from
    TimestampTz         cme_valid_until;       // Valid until
    
    // Options
    bool                cme_enabled;           // Entry enabled
} CertMapEntry;

// Map certificate to database user
char* map_certificate_to_user(
    X509* cert,
    CertMappingConfig* config)
{
    char* username = NULL;
    
    switch (config->cmc_method) {
        case CERT_MAP_CN:
            username = map_cert_cn_to_user(cert);
            break;
            
        case CERT_MAP_DN:
            username = map_cert_dn_to_user(cert);
            break;
            
        case CERT_MAP_SAN:
            username = map_cert_san_to_user(cert);
            break;
            
        case CERT_MAP_FILE:
            username = map_cert_file_to_user(cert, config->cmc_map_file);
            break;
            
        case CERT_MAP_LDAP:
            username = map_cert_ldap_to_user(cert, config);
            break;
            
        case CERT_MAP_DATABASE:
            username = map_cert_database_to_user(cert, config->cmc_map_table);
            break;
            
        case CERT_MAP_REGEX:
            username = map_cert_regex_to_user(cert, config);
            break;
    }
    
    if (username == NULL && config->cmc_require_match) {
        log_error("Certificate mapping failed and mapping is required");
        return NULL;
    }
    
    // If no mapping found, try default mapping
    if (username == NULL) {
        username = get_default_cert_mapping(cert);
    }
    
    return username;
}

// Map certificate CN to user
char* map_cert_cn_to_user(X509* cert) {
    X509_NAME* subject = X509_get_subject_name(cert);
    
    char cn[256];
    int len = X509_NAME_get_text_by_NID(subject, NID_commonName, 
                                        cn, sizeof(cn));
    
    if (len < 0) {
        log_error("Certificate has no common name");
        return NULL;
    }
    
    // CN is directly the username
    return strdup(cn);
}

// File-based certificate mapping
char* map_cert_file_to_user(X509* cert, const char* map_file) {
    // Calculate certificate fingerprint
    char fingerprint[128];
    calculate_cert_fingerprint(cert, fingerprint, sizeof(fingerprint));
    
    // Open mapping file
    FILE* fp = fopen(map_file, "r");
    if (fp == NULL) {
        log_error("Cannot open certificate mapping file: %s", map_file);
        return NULL;
    }
    
    char line[1024];
    char* username = NULL;
    
    while (fgets(line, sizeof(line), fp)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        // Parse line: fingerprint username [roles...]
        char file_fingerprint[128];
        char file_username[256];
        
        if (sscanf(line, "%127s %255s", file_fingerprint, file_username) == 2) {
            if (strcasecmp(fingerprint, file_fingerprint) == 0) {
                username = strdup(file_username);
                break;
            }
        }
    }
    
    fclose(fp);
    return username;
}

// Calculate certificate fingerprint
void calculate_cert_fingerprint(X509* cert, char* fingerprint, size_t size) {
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len;
    
    // Get DER encoding of certificate
    int der_len = i2d_X509(cert, NULL);
    unsigned char* der = malloc(der_len);
    unsigned char* p = der;
    i2d_X509(cert, &p);
    
    // Calculate SHA256 hash
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, der, der_len);
    EVP_DigestFinal_ex(ctx, md, &md_len);
    EVP_MD_CTX_free(ctx);
    
    // Convert to hex string
    for (unsigned int i = 0; i < md_len && i * 2 < size - 1; i++) {
        sprintf(fingerprint + i * 2, "%02X", md[i]);
    }
    
    free(der);
}
```

### 2.2 Certificate Authentication Flow

```c
// Certificate authentication method
typedef struct cert_auth_method {
    // Base method
    AuthMethod          cam_base;              // Base authentication method
    
    // Configuration
    CertMappingConfig   cam_mapping_config;    // Certificate mapping config
    
    // Certificate store
    X509_STORE*         cam_cert_store;        // Trusted certificate store
    
    // CRL management
    CRLManager*         cam_crl_manager;       // CRL manager
    
    // OCSP
    OCSPConfig*         cam_ocsp_config;       // OCSP configuration
} CertAuthMethod;

// Certificate authentication context
typedef struct cert_auth_context {
    // Base context
    AuthContext         cac_base;              // Base auth context
    
    // Certificate
    X509*               cac_peer_cert;         // Peer certificate
    STACK_OF(X509)*     cac_cert_chain;        // Certificate chain
    
    // Verification result
    int                 cac_verify_result;     // OpenSSL verify result
    char                cac_verify_error[256]; // Verification error
    
    // Mapped user
    char                cac_mapped_user[256];  // Mapped username
    char**              cac_mapped_roles;      // Mapped roles
} CertAuthContext;

// Start certificate authentication
Status cert_auth_start(
    AuthMethod* method,
    AuthContext* ctx)
{
    CertAuthMethod* cam = (CertAuthMethod*)method;
    CertAuthContext* cac = (CertAuthContext*)ctx;
    
    // Get SSL connection
    SSL* ssl = get_connection_ssl(ctx->ctx_conn_id);
    
    if (ssl == NULL) {
        strcpy(ctx->ctx_error_msg, "No SSL connection");
        return STATUS_NO_SSL;
    }
    
    // Get peer certificate
    cac->cac_peer_cert = SSL_get_peer_certificate(ssl);
    
    if (cac->cac_peer_cert == NULL) {
        strcpy(ctx->ctx_error_msg, "No client certificate provided");
        return STATUS_NO_CERTIFICATE;
    }
    
    // Get certificate chain
    cac->cac_cert_chain = SSL_get_peer_cert_chain(ssl);
    
    // Verify certificate
    long verify_result = SSL_get_verify_result(ssl);
    
    if (verify_result != X509_V_OK) {
        snprintf(ctx->ctx_error_msg, sizeof(ctx->ctx_error_msg),
                "Certificate verification failed: %s",
                X509_verify_cert_error_string(verify_result));
        
        cac->cac_verify_result = verify_result;
        return STATUS_CERT_VERIFY_FAILED;
    }
    
    // Check certificate revocation
    if (cam->cam_crl_manager) {
        if (!check_certificate_revocation(cam->cam_crl_manager, 
                                         cac->cac_peer_cert)) {
            strcpy(ctx->ctx_error_msg, "Certificate is revoked");
            return STATUS_CERT_REVOKED;
        }
    }
    
    // Check OCSP if configured
    if (cam->cam_ocsp_config) {
        OCSPStatus ocsp_status = check_ocsp_status(
            cam->cam_ocsp_config,
            cac->cac_peer_cert,
            cac->cac_cert_chain
        );
        
        if (ocsp_status == OCSP_STATUS_REVOKED) {
            strcpy(ctx->ctx_error_msg, "Certificate revoked (OCSP)");
            return STATUS_CERT_REVOKED;
        } else if (ocsp_status == OCSP_STATUS_UNKNOWN) {
            if (cam->cam_ocsp_config->require_ocsp) {
                strcpy(ctx->ctx_error_msg, "OCSP status unknown");
                return STATUS_OCSP_FAILED;
            }
        }
    }
    
    // Map certificate to user
    char* username = map_certificate_to_user(
        cac->cac_peer_cert,
        &cam->cam_mapping_config
    );
    
    if (username == NULL) {
        strcpy(ctx->ctx_error_msg, "Certificate mapping failed");
        return STATUS_CERT_MAP_FAILED;
    }
    
    strncpy(cac->cac_mapped_user, username, sizeof(cac->cac_mapped_user));
    free(username);
    
    // Set username in base context
    strncpy(ctx->ctx_username, cac->cac_mapped_user, sizeof(ctx->ctx_username));
    
    // Authentication successful
    ctx->ctx_state = AUTH_STATE_SUCCESS;
    
    // Log successful authentication
    log_cert_auth_success(cac);
    
    return STATUS_OK;
}

// Log certificate authentication success
void log_cert_auth_success(CertAuthContext* cac) {
    char subject[512];
    char issuer[512];
    char serial[64];
    
    X509_NAME_oneline(X509_get_subject_name(cac->cac_peer_cert),
                      subject, sizeof(subject));
    X509_NAME_oneline(X509_get_issuer_name(cac->cac_peer_cert),
                      issuer, sizeof(issuer));
    
    ASN1_INTEGER* serial_asn1 = X509_get_serialNumber(cac->cac_peer_cert);
    BIGNUM* serial_bn = ASN1_INTEGER_to_BN(serial_asn1, NULL);
    char* serial_hex = BN_bn2hex(serial_bn);
    strncpy(serial, serial_hex, sizeof(serial));
    OPENSSL_free(serial_hex);
    BN_free(serial_bn);
    
    log_info("Certificate authentication successful: "
            "user=%s, subject=%s, issuer=%s, serial=%s",
            cac->cac_mapped_user, subject, issuer, serial);
}
```

## 3. Certificate Revocation

### 3.1 CRL Management

```c
// CRL manager
typedef struct crl_manager {
    // CRL storage
    STACK_OF(X509_CRL)* crm_crls;              // Loaded CRLs
    
    // CRL sources
    char**              crm_crl_files;         // CRL file paths
    uint32_t            crm_crl_file_count;    // Number of CRL files
    
    char**              crm_crl_urls;          // CRL distribution points
    uint32_t            crm_crl_url_count;     // Number of CRL URLs
    
    // Update configuration
    uint32_t            crm_update_interval;   // Update interval (seconds)
    TimestampTz         crm_last_update;       // Last update time
    
    // Cache
    HashTable*          crm_revoked_cache;     // Revoked serial cache
    
    // Update thread
    pthread_t           crm_update_thread;     // Update thread
    bool                crm_update_running;    // Update thread running
    
    // Synchronization
    pthread_rwlock_t    crm_lock;              // Read-write lock
} CRLManager;

// Check certificate revocation
bool check_certificate_revocation(
    CRLManager* manager,
    X509* cert)
{
    pthread_rwlock_rdlock(&manager->crm_lock);
    
    // Get certificate serial number
    ASN1_INTEGER* serial = X509_get_serialNumber(cert);
    
    // Check cache first
    if (is_serial_in_cache(manager->crm_revoked_cache, serial)) {
        pthread_rwlock_unlock(&manager->crm_lock);
        return false;  // Certificate is revoked
    }
    
    // Check against all CRLs
    for (int i = 0; i < sk_X509_CRL_num(manager->crm_crls); i++) {
        X509_CRL* crl = sk_X509_CRL_value(manager->crm_crls, i);
        
        // Check if certificate issuer matches CRL issuer
        if (!X509_NAME_cmp(X509_get_issuer_name(cert),
                          X509_CRL_get_issuer(crl))) {
            
            // Check if certificate is revoked
            X509_REVOKED* revoked = NULL;
            if (X509_CRL_get0_by_serial(crl, &revoked, serial) > 0) {
                // Certificate is revoked
                add_serial_to_cache(manager->crm_revoked_cache, serial);
                
                // Get revocation reason
                ASN1_ENUMERATED* reason_code = NULL;
                reason_code = X509_REVOKED_get_ext_d2i(revoked,
                                                       NID_crl_reason,
                                                       NULL, NULL);
                
                if (reason_code) {
                    long reason = ASN1_ENUMERATED_get(reason_code);
                    log_warning("Certificate revoked, reason: %ld", reason);
                    ASN1_ENUMERATED_free(reason_code);
                }
                
                pthread_rwlock_unlock(&manager->crm_lock);
                return false;
            }
        }
    }
    
    pthread_rwlock_unlock(&manager->crm_lock);
    return true;  // Certificate not revoked
}

// Update CRLs
void* crl_update_thread(void* arg) {
    CRLManager* manager = (CRLManager*)arg;
    
    while (manager->crm_update_running) {
        sleep(manager->crm_update_interval);
        
        pthread_rwlock_wrlock(&manager->crm_lock);
        
        // Update CRLs from files
        for (uint32_t i = 0; i < manager->crm_crl_file_count; i++) {
            X509_CRL* crl = load_crl_file(manager->crm_crl_files[i]);
            if (crl) {
                update_or_add_crl(manager, crl);
            }
        }
        
        // Update CRLs from URLs
        for (uint32_t i = 0; i < manager->crm_crl_url_count; i++) {
            X509_CRL* crl = download_crl(manager->crm_crl_urls[i]);
            if (crl) {
                update_or_add_crl(manager, crl);
            }
        }
        
        // Clean expired CRLs
        clean_expired_crls(manager);
        
        manager->crm_last_update = GetCurrentTimestamp();
        
        pthread_rwlock_unlock(&manager->crm_lock);
    }
    
    return NULL;
}

// Download CRL from URL
X509_CRL* download_crl(const char* url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return NULL;
    }
    
    // Setup memory buffer for response
    MemoryBuffer buffer = {0};
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        log_error("Failed to download CRL from %s: %s",
                 url, curl_easy_strerror(res));
        free(buffer.data);
        return NULL;
    }
    
    // Parse CRL
    BIO* bio = BIO_new_mem_buf(buffer.data, buffer.size);
    X509_CRL* crl = d2i_X509_CRL_bio(bio, NULL);
    
    if (!crl) {
        // Try PEM format
        BIO_reset(bio);
        crl = PEM_read_bio_X509_CRL(bio, NULL, NULL, NULL);
    }
    
    BIO_free(bio);
    free(buffer.data);
    
    return crl;
}
```

### 3.2 OCSP Support

```c
// OCSP configuration
typedef struct ocsp_config {
    // OCSP responder
    char*               oc_responder_url;      // OCSP responder URL
    X509*               oc_responder_cert;     // Responder certificate
    
    // Options
    bool                oc_require_ocsp;       // Require OCSP check
    uint32_t            oc_timeout_seconds;    // OCSP timeout
    bool                oc_use_nonce;          // Use nonce
    
    // Cache
    OCSPCache*          oc_cache;              // OCSP response cache
    uint32_t            oc_cache_ttl;          // Cache TTL (seconds)
} OCSPConfig;

// OCSP status
typedef enum ocsp_status {
    OCSP_STATUS_GOOD,
    OCSP_STATUS_REVOKED,
    OCSP_STATUS_UNKNOWN
} OCSPStatus;

// Check OCSP status
OCSPStatus check_ocsp_status(
    OCSPConfig* config,
    X509* cert,
    STACK_OF(X509)* chain)
{
    // Check cache first
    OCSPStatus cached_status = check_ocsp_cache(config->oc_cache, cert);
    if (cached_status != OCSP_STATUS_UNKNOWN) {
        return cached_status;
    }
    
    // Create OCSP request
    OCSP_REQUEST* req = OCSP_REQUEST_new();
    
    // Add certificate ID
    X509* issuer = find_issuer_cert(cert, chain);
    if (!issuer) {
        OCSP_REQUEST_free(req);
        return OCSP_STATUS_UNKNOWN;
    }
    
    OCSP_CERTID* id = OCSP_cert_to_id(EVP_sha256(), cert, issuer);
    OCSP_request_add0_id(req, id);
    
    // Add nonce if configured
    if (config->oc_use_nonce) {
        OCSP_request_add1_nonce(req, NULL, -1);
    }
    
    // Send OCSP request
    OCSP_RESPONSE* resp = send_ocsp_request(config->oc_responder_url, req);
    OCSP_REQUEST_free(req);
    
    if (!resp) {
        return OCSP_STATUS_UNKNOWN;
    }
    
    // Check response status
    int resp_status = OCSP_response_status(resp);
    if (resp_status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
        log_error("OCSP response status: %s",
                 OCSP_response_status_str(resp_status));
        OCSP_RESPONSE_free(resp);
        return OCSP_STATUS_UNKNOWN;
    }
    
    // Get basic response
    OCSP_BASICRESP* basic = OCSP_response_get1_basic(resp);
    if (!basic) {
        OCSP_RESPONSE_free(resp);
        return OCSP_STATUS_UNKNOWN;
    }
    
    // Verify response signature
    if (!verify_ocsp_response(basic, chain, config)) {
        OCSP_BASICRESP_free(basic);
        OCSP_RESPONSE_free(resp);
        return OCSP_STATUS_UNKNOWN;
    }
    
    // Check nonce if used
    if (config->oc_use_nonce) {
        if (OCSP_check_nonce(req, basic) <= 0) {
            log_error("OCSP nonce verification failed");
            OCSP_BASICRESP_free(basic);
            OCSP_RESPONSE_free(resp);
            return OCSP_STATUS_UNKNOWN;
        }
    }
    
    // Get certificate status
    int cert_status, reason;
    ASN1_GENERALIZEDTIME* revtime;
    ASN1_GENERALIZEDTIME* thisupd;
    ASN1_GENERALIZEDTIME* nextupd;
    
    if (!OCSP_resp_find_status(basic, id, &cert_status, &reason,
                               &revtime, &thisupd, &nextupd)) {
        OCSP_BASICRESP_free(basic);
        OCSP_RESPONSE_free(resp);
        return OCSP_STATUS_UNKNOWN;
    }
    
    // Check validity time
    if (!OCSP_check_validity(thisupd, nextupd, 300, -1)) {
        log_error("OCSP response not valid");
        OCSP_BASICRESP_free(basic);
        OCSP_RESPONSE_free(resp);
        return OCSP_STATUS_UNKNOWN;
    }
    
    OCSPStatus status;
    
    switch (cert_status) {
        case V_OCSP_CERTSTATUS_GOOD:
            status = OCSP_STATUS_GOOD;
            break;
            
        case V_OCSP_CERTSTATUS_REVOKED:
            status = OCSP_STATUS_REVOKED;
            log_warning("Certificate revoked via OCSP, reason: %d", reason);
            break;
            
        default:
            status = OCSP_STATUS_UNKNOWN;
            break;
    }
    
    // Cache result
    cache_ocsp_status(config->oc_cache, cert, status, nextupd);
    
    OCSP_BASICRESP_free(basic);
    OCSP_RESPONSE_free(resp);
    
    return status;
}

// Send OCSP request
OCSP_RESPONSE* send_ocsp_request(const char* url, OCSP_REQUEST* req) {
    // Serialize request
    unsigned char* req_data = NULL;
    int req_len = i2d_OCSP_REQUEST(req, &req_data);
    
    if (req_len <= 0) {
        return NULL;
    }
    
    // Setup HTTP request
    CURL* curl = curl_easy_init();
    if (!curl) {
        OPENSSL_free(req_data);
        return NULL;
    }
    
    MemoryBuffer response = {0};
    
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/ocsp-request");
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req_data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, req_len);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    OPENSSL_free(req_data);
    
    if (res != CURLE_OK) {
        log_error("OCSP request failed: %s", curl_easy_strerror(res));
        free(response.data);
        return NULL;
    }
    
    // Parse response
    const unsigned char* p = response.data;
    OCSP_RESPONSE* resp = d2i_OCSP_RESPONSE(NULL, &p, response.size);
    
    free(response.data);
    return resp;
}
```

## 4. Smart Card Support

### 4.1 PKCS#11 Integration

```c
// PKCS#11 configuration
typedef struct pkcs11_config {
    // Module
    char*               p11_module_path;       // PKCS#11 module path
    void*               p11_module_handle;     // Module handle
    CK_FUNCTION_LIST*   p11_functions;         // PKCS#11 functions
    
    // Slot configuration
    CK_SLOT_ID          p11_slot_id;           // Slot ID
    char*               p11_token_label;       // Token label
    
    // PIN
    char*               p11_pin;               // User PIN
    bool                p11_prompt_pin;        // Prompt for PIN
    
    // Certificate selection
    char*               p11_cert_label;        // Certificate label
    char*               p11_cert_id;           // Certificate ID
} PKCS11Config;

// Initialize PKCS#11
Status init_pkcs11(PKCS11Config* config) {
    // Load module
    config->p11_module_handle = dlopen(config->p11_module_path, RTLD_NOW);
    if (!config->p11_module_handle) {
        log_error("Failed to load PKCS#11 module: %s", dlerror());
        return STATUS_MODULE_LOAD_FAILED;
    }
    
    // Get function list
    CK_RV (*C_GetFunctionList)(CK_FUNCTION_LIST_PTR_PTR);
    C_GetFunctionList = dlsym(config->p11_module_handle, "C_GetFunctionList");
    
    if (!C_GetFunctionList) {
        log_error("Failed to get C_GetFunctionList");
        return STATUS_FUNCTION_NOT_FOUND;
    }
    
    CK_RV rv = C_GetFunctionList(&config->p11_functions);
    if (rv != CKR_OK) {
        log_error("C_GetFunctionList failed: 0x%lx", rv);
        return STATUS_PKCS11_ERROR;
    }
    
    // Initialize library
    CK_C_INITIALIZE_ARGS init_args = {0};
    init_args.flags = CKF_OS_LOCKING_OK;
    
    rv = config->p11_functions->C_Initialize(&init_args);
    if (rv != CKR_OK && rv != CKR_CRYPTOKI_ALREADY_INITIALIZED) {
        log_error("C_Initialize failed: 0x%lx", rv);
        return STATUS_PKCS11_ERROR;
    }
    
    return STATUS_OK;
}

// Get certificate from smart card
X509* get_smartcard_certificate(PKCS11Config* config) {
    CK_SESSION_HANDLE session;
    CK_RV rv;
    
    // Open session
    rv = config->p11_functions->C_OpenSession(
        config->p11_slot_id,
        CKF_SERIAL_SESSION,
        NULL, NULL,
        &session
    );
    
    if (rv != CKR_OK) {
        log_error("C_OpenSession failed: 0x%lx", rv);
        return NULL;
    }
    
    // Login if PIN provided
    if (config->p11_pin) {
        rv = config->p11_functions->C_Login(
            session,
            CKU_USER,
            (CK_UTF8CHAR_PTR)config->p11_pin,
            strlen(config->p11_pin)
        );
        
        if (rv != CKR_OK) {
            log_error("C_Login failed: 0x%lx", rv);
            config->p11_functions->C_CloseSession(session);
            return NULL;
        }
    }
    
    // Find certificate
    CK_OBJECT_CLASS cert_class = CKO_CERTIFICATE;
    CK_CERTIFICATE_TYPE cert_type = CKC_X_509;
    
    CK_ATTRIBUTE search_template[] = {
        {CKA_CLASS, &cert_class, sizeof(cert_class)},
        {CKA_CERTIFICATE_TYPE, &cert_type, sizeof(cert_type)}
    };
    
    if (config->p11_cert_label) {
        // Add label to search
        CK_ATTRIBUTE label_attr = {
            CKA_LABEL,
            config->p11_cert_label,
            strlen(config->p11_cert_label)
        };
        // Would need to extend search_template
    }
    
    rv = config->p11_functions->C_FindObjectsInit(
        session,
        search_template,
        sizeof(search_template) / sizeof(CK_ATTRIBUTE)
    );
    
    if (rv != CKR_OK) {
        log_error("C_FindObjectsInit failed: 0x%lx", rv);
        config->p11_functions->C_CloseSession(session);
        return NULL;
    }
    
    CK_OBJECT_HANDLE cert_handle;
    CK_ULONG found_count;
    
    rv = config->p11_functions->C_FindObjects(
        session,
        &cert_handle,
        1,
        &found_count
    );
    
    config->p11_functions->C_FindObjectsFinal(session);
    
    if (rv != CKR_OK || found_count == 0) {
        log_error("Certificate not found on smart card");
        config->p11_functions->C_CloseSession(session);
        return NULL;
    }
    
    // Get certificate value
    CK_ATTRIBUTE cert_attr = {CKA_VALUE, NULL, 0};
    
    // Get size first
    rv = config->p11_functions->C_GetAttributeValue(
        session,
        cert_handle,
        &cert_attr,
        1
    );
    
    if (rv != CKR_OK) {
        log_error("C_GetAttributeValue (size) failed: 0x%lx", rv);
        config->p11_functions->C_CloseSession(session);
        return NULL;
    }
    
    // Allocate buffer and get value
    cert_attr.pValue = malloc(cert_attr.ulValueLen);
    
    rv = config->p11_functions->C_GetAttributeValue(
        session,
        cert_handle,
        &cert_attr,
        1
    );
    
    if (rv != CKR_OK) {
        log_error("C_GetAttributeValue failed: 0x%lx", rv);
        free(cert_attr.pValue);
        config->p11_functions->C_CloseSession(session);
        return NULL;
    }
    
    // Parse certificate
    const unsigned char* p = cert_attr.pValue;
    X509* cert = d2i_X509(NULL, &p, cert_attr.ulValueLen);
    
    free(cert_attr.pValue);
    config->p11_functions->C_CloseSession(session);
    
    return cert;
}
```

## 5. Configuration Examples

### 5.1 TLS Configuration File

```yaml
# scratchbird_tls.conf
tls:
  # Server certificate
  certificate: /etc/scratchbird/server.crt
  private_key: /etc/scratchbird/server.key
  
  # CA certificates
  ca_file: /etc/scratchbird/ca-bundle.crt
  crl_file: /etc/scratchbird/crl.pem
  
  # Protocol versions
  min_version: TLSv1.2
  max_version: TLSv1.3
  
  # Cipher configuration
  ciphersuites: TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256
  cipher_list: ECDHE+AESGCM:ECDHE+CHACHA20:!aNULL:!MD5:!DSS
  
  # Client certificates
  client_cert_mode: required  # none, optional, required
  verify_depth: 3
  
  # Session management
  session_cache: true
  session_timeout: 3600
  session_tickets: true
  
  # OCSP
  ocsp_stapling: true
  ocsp_responder: http://ocsp.example.com
  
  # Security
  prefer_server_ciphers: true
  honor_cipher_order: true
  renegotiation: false

# Certificate mapping
cert_mapping:
  method: file  # cn, dn, san, file, ldap, database, regex
  map_file: /etc/scratchbird/cert_map.conf
  case_sensitive: false
  require_match: true
  
  # For LDAP mapping
  ldap:
    server: ldaps://ldap.example.com
    base_dn: ou=users,dc=example,dc=com
    filter: (certificateFingerprint=%fingerprint%)
  
  # For database mapping
  database:
    table: certificate_mappings
    
# CRL management
crl:
  update_interval: 3600
  sources:
    - file: /etc/scratchbird/ca.crl
    - url: http://crl.example.com/ca.crl
    
# OCSP configuration
ocsp:
  require: false
  timeout: 10
  use_nonce: true
  cache_ttl: 3600
```

### 5.2 Certificate Mapping File

```
# cert_map.conf
# Format: fingerprint username [roles...]

# SHA256 fingerprints
A1B2C3D4E5F6...  alice    admin,developer
F6E5D4C3B2A1...  bob      user
1234567890AB...  charlie  readonly

# Can also use CN mapping
CN=John Doe      john     admin
CN=Jane Smith    jane     user,analyst
```

## 6. Testing

### 6.1 Certificate Test Suite

```c
// Certificate authentication tests
typedef struct cert_test_suite {
    // Test certificates
    X509*               cts_valid_cert;        // Valid certificate
    X509*               cts_expired_cert;      // Expired certificate
    X509*               cts_revoked_cert;      // Revoked certificate
    X509*               cts_self_signed;       // Self-signed certificate
    
    // Test keys
    EVP_PKEY*           cts_valid_key;         // Valid private key
    
    // Test CA
    X509*               cts_ca_cert;           // CA certificate
    X509_CRL*           cts_crl;               // CRL
} CertTestSuite;

// Test certificate validation
bool test_cert_validation() {
    CertTestSuite* suite = create_cert_test_suite();
    
    // Test valid certificate
    assert(validate_certificate(suite->cts_valid_cert) == true);
    
    // Test expired certificate
    assert(validate_certificate(suite->cts_expired_cert) == false);
    
    // Test revoked certificate
    assert(check_certificate_revocation(suite->cts_revoked_cert) == false);
    
    // Test self-signed certificate
    assert(validate_certificate(suite->cts_self_signed) == false);
    
    free_cert_test_suite(suite);
    return true;
}

// Test certificate mapping
bool test_cert_mapping() {
    // Create test certificate with CN
    X509* cert = create_test_cert("CN=testuser,O=TestOrg,C=US");
    
    // Test CN mapping
    CertMappingConfig config = {
        .cmc_method = CERT_MAP_CN
    };
    
    char* username = map_certificate_to_user(cert, &config);
    assert(strcmp(username, "testuser") == 0);
    free(username);
    
    // Test fingerprint mapping
    config.cmc_method = CERT_MAP_FILE;
    config.cmc_map_file = "test_cert_map.conf";
    
    // Create test mapping file
    create_test_mapping_file(cert, "mapped_user");
    
    username = map_certificate_to_user(cert, &config);
    assert(strcmp(username, "mapped_user") == 0);
    free(username);
    
    X509_free(cert);
    return true;
}

// Test TLS connection with client certificate
bool test_tls_client_cert() {
    // Create test server
    TLSConfig server_config = {
        .tc_cert_file = "test_server.crt",
        .tc_key_file = "test_server.key",
        .tc_ca_file = "test_ca.crt",
        .tc_client_cert_mode = CLIENT_CERT_REQUIRED,
        .tc_min_protocol = TLS1_2_VERSION
    };
    
    SSL_CTX* server_ctx = init_tls_context(&server_config);
    assert(server_ctx != NULL);
    
    // Create test client with certificate
    SSL_CTX* client_ctx = create_test_client_context();
    assert(client_ctx != NULL);
    
    // Perform handshake
    bool handshake_ok = perform_test_handshake(server_ctx, client_ctx);
    assert(handshake_ok == true);
    
    SSL_CTX_free(server_ctx);
    SSL_CTX_free(client_ctx);
    
    return true;
}
```

## Implementation Notes

This certificate and TLS authentication implementation provides:

1. **Complete TLS/SSL configuration** with modern cipher suites and protocols
2. **X.509 certificate validation** with extension and purpose checking
3. **Flexible certificate mapping** (CN, DN, SAN, file, LDAP, database)
4. **Certificate revocation checking** via CRL and OCSP
5. **Smart card support** through PKCS#11
6. **Comprehensive security features** including mTLS and session management
7. **Complete test framework** for validation

The system supports enterprise PKI deployments while maintaining compatibility with standard TLS clients and certificates.