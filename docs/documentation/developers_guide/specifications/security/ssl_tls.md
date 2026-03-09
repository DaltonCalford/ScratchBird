# Specification: SSL/TLS

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/ssl_tls |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/security/tls_context.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/tls_config.h`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_security_issues.cpp`

## Synopsis

This specification defines SSL/TLS configuration, certificate management, and secure connection handling in ScratchBird, including server and client TLS contexts, certificate validation, and cipher suite configuration.

## Scope

### In Scope

- TLS configuration for server and client
- Certificate and key management
- Cipher suite selection
- Certificate validation
- Client certificate authentication (mTLS)
- Session resumption

### Out of Scope

- Application-level encryption (see `encryption.md`)
- Authentication methods (see `auth_plugins.md`)
- Network protocol details

## Background

ScratchBird uses OpenSSL for TLS implementation, supporting TLS 1.2 and TLS 1.3 with configurable cipher suites and certificate validation.

## Specification

### Data Structures

```cpp
// TLS Version Enumeration
enum class TLSVersion {
    TLS_1_0 = 0,  // Deprecated, not recommended
    TLS_1_1 = 1,  // Deprecated, not recommended
    TLS_1_2 = 2,  // Minimum recommended
    TLS_1_3 = 3   // Preferred
};

// Certificate verification mode
enum class VerifyMode {
    NONE = 0,       // No verification (insecure)
    PEER = 1,       // Verify peer certificate
    FAIL_IF_NO_PEER_CERT = 2,  // Require client cert
    CLIENT_ONCE = 4  // Verify client once per session
};

// Server TLS Configuration
struct TLSConfig {
    bool enabled = false;
    
    // Certificate files
    std::string cert_file;      // Server certificate chain
    std::string key_file;       // Private key
    std::string ca_file;        // CA bundle for client verification
    std::string ca_path;        // Directory with CA certs
    
    // Protocol version
    TLSVersion min_version = TLSVersion::TLS_1_2;
    TLSVersion max_version = TLSVersion::TLS_1_3;
    
    // Cipher configuration
    std::string ciphers;        // TLS 1.2 cipher list
    std::string cipher_suites;  // TLS 1.3 cipher suites
    
    // Verification
    VerifyMode verify_mode = VerifyMode::NONE;
    bool verify_peer = false;
    std::string crl_file;       // Certificate revocation list
    
    // Session configuration
    bool session_tickets = true;
    int session_timeout = 7200;  // 2 hours
    
    // Advanced options
    bool prefer_server_ciphers = true;
    bool ecdh_auto = true;
    std::string dh_param_file;  // DH parameters
    
    Status validate(ErrorContext* ctx) const;
    static std::string getDefaultCiphers();
    static std::string getDefaultCipherSuites();
};

// Client TLS Configuration
struct TLSClientConfig {
    bool enabled = false;
    
    // Certificate files (for mTLS)
    std::string cert_file;
    std::string key_file;
    
    // CA configuration
    std::string ca_file;
    std::string ca_path;
    bool use_system_ca = true;
    
    // Protocol version
    TLSVersion min_version = TLSVersion::TLS_1_2;
    TLSVersion max_version = TLSVersion::TLS_1_3;
    
    // Cipher configuration
    std::string ciphers;
    std::string cipher_suites;
    
    // Verification
    bool verify_server = true;
    std::string verify_hostname;  // Expected hostname
    
    // Session configuration
    bool session_resumption = true;
    
    Status validate(ErrorContext* ctx) const;
};
```

### Default Cipher Configuration

```cpp
// From tls_context.cpp:130-141

std::string TLSConfig::getDefaultCiphers() {
    // Strong cipher list for TLS 1.2
    // Prioritizes: ECDHE > DHE, GCM > CBC, AES > 3DES
    return "ECDHE+AESGCM:DHE+AESGCM:ECDHE+CHACHA20:DHE+CHACHA20:"
           "ECDHE+AES256:DHE+AES256:ECDHE+AES128:DHE+AES128:"
           "!aNULL:!eNULL:!EXPORT:!DES:!RC4:!3DES:!MD5:!PSK";
}

std::string TLSConfig::getDefaultCipherSuites() {
    // TLS 1.3 cipher suites (order matters)
    return "TLS_AES_256_GCM_SHA384:"
           "TLS_CHACHA20_POLY1305_SHA256:"
           "TLS_AES_128_GCM_SHA256";
}
```

### TLS Context Initialization

```
Algorithm: Initialize TLS Server Context

Input: TLSConfig
Output: SSL_CTX* or error

1. INITIALIZE OpenSSL
   SSL_load_error_strings()
   SSL_library_init()
   OpenSSL_add_all_algorithms()

2. CREATE context
   ctx = SSL_CTX_new(TLS_server_method())
   IF ctx == NULL:
     RETURN error("Failed to create SSL context")

3. SET protocol versions
   SSL_CTX_set_min_proto_version(ctx, tlsVersionToSSL(min_version))
   SSL_CTX_set_max_proto_version(ctx, tlsVersionToSSL(max_version))

4. LOAD certificate chain
   IF SSL_CTX_use_certificate_chain_file(ctx, cert_file) <= 0:
     RETURN error("Failed to load certificate")

5. LOAD private key
   IF SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0:
     RETURN error("Failed to load private key")

6. VERIFY key matches certificate
   IF !SSL_CTX_check_private_key(ctx):
     RETURN error("Private key does not match certificate")

7. CONFIGURE ciphers
   SSL_CTX_set_cipher_list(ctx, ciphers.c_str())
   SSL_CTX_set_ciphersuites(ctx, cipher_suites.c_str())

8. CONFIGURE verification (if enabled)
   IF verify_mode != NONE:
     SSL_CTX_load_verify_locations(ctx, ca_file.c_str(), ca_path.c_str())
     SSL_CTX_set_verify(ctx, verify_mode, verify_callback)

9. CONFIGURE session resumption
   IF session_tickets:
     SSL_CTX_set_session_ticket_cb(ctx, ...)
   SSL_CTX_set_timeout(ctx, session_timeout)

10. RETURN ctx
```

### Certificate Validation

```cpp
// Server certificate validation callback
int verify_callback(int preverify_ok, X509_STORE_CTX* ctx) {
    // preverify_ok: OpenSSL's initial verification result
    // ctx: verification context
    
    if (!preverify_ok) {
        int err = X509_STORE_CTX_get_error(ctx);
        int depth = X509_STORE_CTX_get_error_depth(ctx);
        X509* cert = X509_STORE_CTX_get_current_cert(ctx);
        
        // Log verification failure
        LOG_ERROR("Certificate verification failed: %s at depth %d",
                  X509_verify_cert_error_string(err), depth);
        
        // Can override specific errors here if needed
        // But generally should reject
    }
    
    return preverify_ok;
}
```

### mTLS (Mutual TLS) Authentication

```
Client Certificate Verification Flow:

┌──────────┐                    ┌──────────┐
│  Client  │                    │  Server  │
└────┬─────┘                    └────┬─────┘
     │                               │
     │  1. ClientHello               │
     │──────────────────────────────>│
     │                               │
     │  2. ServerHello + Certificate │
     │  3. CertificateRequest        │
     │<──────────────────────────────│
     │                               │
     │  4. Client Certificate        │
     │  5. Client Key Exchange       │
     │  6. CertificateVerify         │
     │──────────────────────────────>│
     │                               │
     │  7. [Server verifies cert]    │
     │  8. Finished                  │
     │<──────────────────────────────│
     │                               │
     │  9. Finished                  │
     │──────────────────────────────>│
     │                               │
     │  10. Encrypted Application    │
     │      Data                     │
     │<═════════════════════════════>│
```

### HBA Integration

```conf
# pg_hba.conf - SSL requirement examples

# Require SSL for remote connections
hostssl    all    all    0.0.0.0/0    scram-sha-256

# Require mTLS for sensitive operations
hostssl    production    service    10.0.0.0/24    cert

# Reject non-SSL connections
hostnossl  all    all    0.0.0.0/0    reject

# Local connections (Unix socket) - no SSL needed
local      all    all                 peer
```

### Configuration File Format

```ini
# scratchbird.conf - TLS Configuration

[ssl]
enabled = true
cert_file = /etc/scratchbird/server.crt
key_file = /etc/scratchbird/server.key
ca_file = /etc/scratchbird/ca.crt

# Protocol versions
min_version = TLSv1.2
max_version = TLSv1.3

# Cipher configuration
ciphers = ECDHE+AESGCM:DHE+AESGCM:ECDHE+CHACHA20:DHE+CHACHA20

# Client verification (for mTLS)
verify_mode = peer
verify_peer = true

# Session configuration
session_timeout = 7200
```

### Certificate Management

```
Certificate Lifecycle:

┌─────────────┐     Generate      ┌─────────────┐
│   Create    │──────────────────>│   Active    │
│   Request   │                   │  (in use)   │
└─────────────┘                   └──────┬──────┘
                                         │
                    ┌────────────────────┼────────────────────┐
                    │                    │                    │
                    ▼                    ▼                    ▼
             ┌─────────────┐      ┌─────────────┐      ┌─────────────┐
             │   Renew     │      │   Revoke    │      │   Expire    │
             │ (pre-expiry)│      │  (compromised│      │  (no renew) │
             └──────┬──────┘      └──────┬──────┘      └──────┬──────┘
                    │                    │                    │
                    ▼                    ▼                    ▼
             ┌─────────────┐      ┌─────────────┐      ┌─────────────┐
             │   Active    │      │   CRL/OCSP  │      │   Replace   │
             │  (renewed)  │      │   update    │      │  required   │
             └─────────────┘      └─────────────┘      └─────────────┘
```

### Certificate Chain Validation

```
Chain Validation:

Leaf Certificate (server.example.com)
    │
    ▼ [signed by]
Intermediate CA (Example CA Intermediate)
    │
    ▼ [signed by]
Root CA (Example Root CA)
    │
    ▼ [self-signed]
Trusted

Validation Steps:
1. Verify leaf certificate signature using intermediate's public key
2. Verify intermediate certificate signature using root's public key
3. Verify root certificate is in trust store
4. Check certificate validity dates
5. Check certificate revocation status (CRL/OCSP)
6. Verify hostname matches certificate subject/SAN
```

### Security Considerations

| Setting | Recommended | Rationale |
|---------|-------------|-----------|
| Min TLS Version | TLS 1.2 | TLS 1.0/1.1 have known vulnerabilities |
| Cipher List | ECDHE+AESGCM | Forward secrecy + authenticated encryption |
| Verify Mode | PEER | Always verify certificates |
| Session Tickets | Enabled | Performance improvement |
| Client Certs | Required for sensitive | Strong authentication |

### Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `SSL_ERROR_NONE` | No error | Continue |
| `SSL_ERROR_SSL` | SSL protocol error | Check certificate/versions |
| `SSL_ERROR_WANT_READ` | Need more data | Retry after receiving |
| `SSL_ERROR_WANT_WRITE` | Need to send data | Retry after sending |
| `SSL_ERROR_SYSCALL` | System call error | Check errno |
| `SSL_ERROR_ZERO_RETURN` | Clean shutdown | Close connection |

## Related Specifications

- `auth_plugins.md` - Certificate authentication plugin
- `encryption.md` - Application-level encryption
- `hba_rules.md` - HBA SSL configuration

## Appendix

### Certificate Generation Example

```bash
# Generate CA key and certificate
openssl genrsa -out ca.key 4096
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt \
    -subj "/C=US/O=Example/CN=Example Root CA"

# Generate server key and CSR
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr \
    -subj "/C=US/O=Example/CN=server.example.com"

# Sign server certificate
openssl x509 -req -days 365 -in server.csr -CA ca.crt -CAkey ca.key \
    -CAcreateserial -out server.crt

# Generate client certificate (for mTLS)
openssl genrsa -out client.key 2048
openssl req -new -key client.key -out client.csr \
    -subj "/C=US/O=Example/CN=client"
openssl x509 -req -days 365 -in client.csr -CA ca.crt -CAkey ca.key \
    -CAcreateserial -out client.crt
```

### Testing TLS Configuration

```bash
# Test TLS connection
openssl s_client -connect localhost:5432 -tls1_3

# Test certificate validation
openssl s_client -connect localhost:5432 \
    -CAfile /etc/scratchbird/ca.crt \
    -verify_return_error

# Test mTLS
openssl s_client -connect localhost:5432 \
    -cert client.crt -key client.key \
    -CAfile ca.crt
```

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
