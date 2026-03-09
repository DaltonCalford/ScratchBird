# Specification: Authentication Plugins

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/authentication/plugins |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/security/auth_plugin_manager.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/auth_plugin_manager.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/security/plugins/*`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_security_auth_provider_rule_chain_contract.cpp`

## Synopsis

This specification defines the 17+ authentication plugins available in ScratchBird, including their methods, configuration options, security properties, and integration patterns. The plugin system supports legacy compatibility methods, modern cryptographic authentication, and enterprise identity providers.

## Scope

### In Scope

- All 17 built-in authentication plugins
- Plugin method registration and dispatch
- Plugin configuration and initialization
- Security properties of each method
- Plugin chaining and fallback behavior

### Out of Scope

- HBA rule matching (see `hba_rules.md`)
- MFA orchestration (see `authentication_flow.md`)
- Custom plugin development API

## Background

ScratchBird implements a comprehensive authentication plugin system that supports both PostgreSQL-compatible methods and modern authentication standards. The plugin registry enables runtime selection of authentication methods based on HBA rules.

### Plugin Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Auth Plugin Manager                       │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │   Trust/    │  │   SCRAM     │  │    LDAP     │          │
│  │   Reject    │  │  SHA-256/512│  │             │          │
│  └─────────────┘  └─────────────┘  └─────────────┘          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │  Password   │  │  Kerberos   │  │  Certificate│          │
│  │  /MD5       │  │   GSSAPI    │  │   mTLS      │          │
│  └─────────────┘  └─────────────┘  └─────────────┘          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │  JWT/OIDC   │  │  WebAuthn   │  │    PAM      │          │
│  └─────────────┘  └─────────────┘  └─────────────┘          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │   RADIUS    │  │    Peer     │  │    Ident    │          │
│  └─────────────┘  └─────────────┘  └─────────────┘          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │OAuth Valid. │  │   Proxy     │  │   Factor    │          │
│  │             │  │  Assertion  │  │   Chain     │          │
│  └─────────────┘  └─────────────┘  └─────────────┘          │
│  ┌─────────────┐  ┌─────────────┐                            │
│  │Token AuthKey│  │Workload Id. │                            │
│  └─────────────┘  └─────────────┘                            │
└─────────────────────────────────────────────────────────────┘
```

## Specification

### Plugin Registry Data Structures

```cpp
// From auth_plugin_manager.cpp:59-111
struct BuiltinPluginTemplate {
    const char* plugin_id;
    std::array<const char*, 4> methods;
    std::size_t method_count;
};

const std::array<BuiltinPluginTemplate, 17> kBuiltinPhase1Plugins = {{
    {"scratchbird.auth.trust_reject",
     {"scratchbird.auth.trust", "scratchbird.auth.reject", nullptr, nullptr},
     2},
    {"scratchbird.auth.password_compat",
     {"scratchbird.auth.password_compat", "scratchbird.auth.md5_legacy", nullptr, nullptr},
     2},
    {"scratchbird.auth.scram",
     {"scratchbird.auth.scram_sha_256", "scratchbird.auth.scram_sha_512", nullptr, nullptr},
     2},
    {"scratchbird.auth.token_authkey",
     {"scratchbird.auth.authkey_token", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.peer",
     {"scratchbird.auth.peer_uid", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.certificate_mtls",
     {"scratchbird.auth.certificate_x509", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.jwt_oidc",
     {"scratchbird.auth.jwt_bearer", "scratchbird.auth.oidc_id_token", nullptr, nullptr},
     2},
    {"scratchbird.auth.webauthn",
     {"scratchbird.auth.webauthn_assertion", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.factor_chain",
     {"scratchbird.auth.factor_chain_2fa", "scratchbird.auth.factor_chain_3fa", nullptr, nullptr},
     2},
    {"scratchbird.auth.workload_identity",
     {"scratchbird.auth.workload_oidc", "scratchbird.auth.workload_spiffe", nullptr, nullptr},
     2},
    {"scratchbird.auth.oauth_validator",
     {"scratchbird.auth.oauth_bearer_validated", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.proxy_assertion",
     {"scratchbird.auth.proxy_principal_assertion", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.ldap",
     {"scratchbird.auth.ldap_bind", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.kerberos",
     {"scratchbird.auth.kerberos_gssapi", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.ident",
     {"scratchbird.auth.ident_rfc1413", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.radius",
     {"scratchbird.auth.radius_pap", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.pam",
     {"scratchbird.auth.pam_conversation", nullptr, nullptr, nullptr},
     1},
}};
```

### Plugin 1: Trust/Reject

**Plugin ID**: `scratchbird.auth.trust_reject`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.trust` | Allow without authentication | ⚠️ Low - use only for local/secure networks |
| `scratchbird.auth.reject` | Always reject connection | 🔒 High - explicit denial |

**Use Cases:**
- `trust`: Local development, Unix socket connections from trusted users
- `reject`: Block specific networks or users explicitly

**Configuration:**
```
# pg_hba.conf
local   all   all                trust
host    all   all   10.0.0.0/8   reject
```

### Plugin 2: Password Compatibility

**Plugin ID**: `scratchbird.auth.password_compat`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.password_compat` | Plaintext password | ⚠️ Low - use only with SSL |
| `scratchbird.auth.md5_legacy` | MD5 hashed password | ⚠️ Medium - legacy compatibility |

**Security Considerations:**
- Plaintext: Must be used over encrypted connection (SSL/TLS)
- MD5: Legacy support only, not recommended for new deployments
- Both methods vulnerable to replay attacks without channel binding

### Plugin 3: SCRAM (Recommended)

**Plugin ID**: `scratchbird.auth.scram`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.scram_sha_256` | SCRAM-SHA-256 (RFC 7677) | 🔒 High |
| `scratchbird.auth.scram_sha_512` | SCRAM-SHA-512 | 🔒 High |

**Properties:**
- Salted challenge-response (prevents rainbow table attacks)
- Mutual authentication (client verifies server)
- Channel binding support (prevents MITM)
- No password equivalent on wire

**Implementation Details:**
- Iteration count: 4096+ (configurable)
- Nonce length: 24 bytes server + client
- Salt length: 16 bytes

### Plugin 4: LDAP

**Plugin ID**: `scratchbird.auth.ldap`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.ldap_bind` | LDAP simple bind | 🔒 Medium-High |

**Configuration Options:**
```
ldapserver=ldap.example.com
ldapport=636
ldapscheme=ldaps
ldapbasedn="dc=example,dc=com"
ldapbinddn="cn=auth,dc=example,dc=com"
ldapbindpasswd="secret"
ldapsearchattribute=uid
```

**Security Considerations:**
- Use LDAPS (SSL) or StartTLS
- Bind DN should have limited privileges
- Consider LDAP connection pooling for performance

### Plugin 5: Kerberos (GSSAPI)

**Plugin ID**: `scratchbird.auth.kerberos`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.kerberos_gssapi` | GSSAPI Kerberos | 🔒 High |

**Requirements:**
- Kerberos principal for server
- Keytab file configured
- Client must have valid TGT

**Configuration:**
```
krb_server_keyfile='/etc/scratchbird/krb5.keytab'
krb_realm=EXAMPLE.COM
```

### Plugin 6: Certificate (mTLS)

**Plugin ID**: `scratchbird.auth.certificate_mtls`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.certificate_x509` | X.509 certificate auth | 🔒 High |

**Properties:**
- Requires TLS with client certificates
- Certificate CN or SAN matched to username
- Optional certificate revocation checking

**Certificate Mapping:**
```
cert_user_mapping='CN'
cert_verify_depth=2
cert_revocation_check=true
```

### Plugin 7: JWT/OIDC

**Plugin ID**: `scratchbird.auth.jwt_oidc`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.jwt_bearer` | JWT Bearer token | 🔒 High |
| `scratchbird.auth.oidc_id_token` | OpenID Connect ID token | 🔒 High |

**Token Validation:**
- Signature verification (RS256, ES256, EdDSA)
- Expiration check (`exp` claim)
- Issuer validation (`iss` claim)
- Audience validation (`aud` claim)

**Configuration:**
```
jwks_url=https://auth.example.com/.well-known/jwks.json
jwt_issuer=https://auth.example.com
jwt_audience=scratchbird
```

### Plugin 8: WebAuthn

**Plugin ID**: `scratchbird.auth.webauthn`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.webauthn_assertion` | FIDO2/WebAuthn | 🔒 Very High |

**Features:**
- Hardware security key support
- Platform authenticators (Touch ID, Windows Hello)
- Resident key support
- Attestation verification

### Plugin 9: RADIUS

**Plugin ID**: `scratchbird.auth.radius`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.radius_pap` | RADIUS PAP | 🔒 Medium |

**Configuration:**
```
radius_server=radius.example.com
radius_port=1812
radius_secret=shared_secret
radius_timeout=5
```

### Plugin 10: PAM

**Plugin ID**: `scratchbird.auth.pam`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.pam_conversation` | PAM conversation | 🔒 Medium-High |

**Features:**
- System authentication integration
- 2FA via PAM modules (Google Authenticator, YubiKey)
- Account/password policy enforcement

**Configuration:**
```
pam_service=scratchbird
```

### Plugin 11: Peer (Local)

**Plugin ID**: `scratchbird.auth.peer`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.peer_uid` | Unix peer UID | 🔒 High (local only) |

**Use Case:**
- Unix domain socket authentication
- Maps OS user to database user

### Plugin 12: Ident (RFC 1413)

**Plugin ID**: `scratchbird.auth.ident`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.ident_rfc1413` | Ident protocol | ⚠️ Low |

**Use Case:**
- Legacy compatibility
- Trusted local networks only
- Not recommended for production

### Plugin 13: OAuth Validator

**Plugin ID**: `scratchbird.auth.oauth_validator`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.oauth_bearer_validated` | OAuth 2.0 token validation | 🔒 High |

**Features:**
- Token introspection (RFC 7662)
- UserInfo endpoint validation
- Scope checking

### Plugin 14: Proxy Assertion

**Plugin ID**: `scratchbird.auth.proxy_assertion`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.proxy_principal_assertion` | Proxy auth assertion | 🔒 High |

**Use Case:**
- Connection pooling proxies (PgBouncer-style)
- Trusted proxy authentication

### Plugin 15: Token AuthKey

**Plugin ID**: `scratchbird.auth.token_authkey`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.authkey_token` | Pre-shared API key | 🔒 Medium-High |

**Features:**
- Stateless authentication
- Configurable key rotation
- Rate limiting per key

### Plugin 16: Workload Identity

**Plugin ID**: `scratchbird.auth.workload_identity`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.workload_oidc` | Workload OIDC tokens | 🔒 High |
| `scratchbird.auth.workload_spiffe` | SPIFFE/SPIRE identities | 🔒 High |

**Use Cases:**
- Kubernetes workload identity
- Cloud IAM (AWS IRSA, GCP Workload Identity)
- Service mesh mTLS

### Plugin 17: Factor Chain (MFA)

**Plugin ID**: `scratchbird.auth.factor_chain`

| Method | Description | Security Level |
|--------|-------------|----------------|
| `scratchbird.auth.factor_chain_2fa` | Two-factor chain | 🔒 Very High |
| `scratchbird.auth.factor_chain_3fa` | Three-factor chain | 🔒 Very High |

**Features:**
- Chains multiple authentication factors
- Configurable factor order
- Backup factor support

## Plugin Dispatch Algorithm

```
Input:  HBA rule, connection info, client response
Output: Authentication result

1. RESOLVE method to plugin
   plugin = plugin_registry_.find(method)
   if not found:
       return PLUGIN_NOT_FOUND

2. CHECK plugin availability
   if not plugin.isAvailable():
       if legacy_fallback_allowed:
           return tryLegacyAuth(method)
       return PLUGIN_UNAVAILABLE

3. INITIALIZE plugin context
   ctx = plugin.createContext(connection_info)

4. EXECUTE authentication
   result = plugin.authenticate(ctx, client_response)

5. RETURN result
   - SUCCESS: Authentication complete
   - IN_PROGRESS: Save state, return challenge
   - FAILURE: Return error with reason
```

## Security Comparison Matrix

| Plugin | Replay Protection | Mutual Auth | Channel Binding | Password Safe |
|--------|-------------------|-------------|-----------------|---------------|
| Trust | ❌ | ❌ | ❌ | N/A |
| Reject | N/A | N/A | N/A | N/A |
| Password | ❌ | ❌ | ❌ | ⚠️ (plaintext) |
| MD5 | ❌ | ❌ | ❌ | ⚠️ (hash) |
| SCRAM | ✅ | ✅ | ✅ | ✅ |
| LDAP | ✅ (with SSL) | ❌ | ❌ | ✅ |
| Kerberos | ✅ | ✅ | ✅ | ✅ |
| Certificate | ✅ | ✅ | ✅ | N/A |
| JWT | ✅ | ❌ | ❌ | N/A |
| WebAuthn | ✅ | ✅ | ✅ | N/A |
| RADIUS | ⚠️ (with secret) | ❌ | ❌ | ✅ |
| PAM | Depends on module | Depends | Depends | Depends |
| Peer | ✅ | ✅ | N/A | N/A |
| Ident | ❌ | ❌ | ❌ | N/A |
| OAuth | ✅ | ❌ | ❌ | N/A |
| Proxy | ✅ | ❌ | ❌ | N/A |
| Token | ✅ | ❌ | ❌ | N/A |
| Workload | ✅ | ✅ (mTLS) | ✅ | N/A |

## Configuration Priority

```
Authentication Method Selection (HBA order):
1. First matching rule wins
2. Plugin takes precedence over legacy
3. If plugin unavailable:
   a. Try legacy fallback (if enabled)
   b. Return error
```

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `PLUGIN_NOT_FOUND` | Method not registered | Check HBA configuration |
| `PLUGIN_UNAVAILABLE` | Plugin disabled/errored | Check plugin logs, enable |
| `CONFIG_ERROR` | Missing required options | Verify plugin configuration |
| `AUTH_FAILURE` | Credentials invalid | Retry with correct credentials |

## Related Specifications

- `authentication_flow.md` - Overall authentication flow
- `hba_rules.md` - HBA rule matching
- `password_management.md` - Password policies

## Appendix

### Plugin Development Checklist

When adding a new authentication plugin:

1. **Security Review**
   - [ ] Threat model created
   - [ ] Cryptographic review completed
   - [ ] Penetration testing performed

2. **Implementation**
   - [ ] Plugin implements auth_plugin_abi_v1
   - [ ] Error handling is comprehensive
   - [ ] Memory safety verified

3. **Testing**
   - [ ] Unit tests for all paths
   - [ ] Integration tests with real services
   - [ ] Fuzz testing for inputs

4. **Documentation**
   - [ ] Configuration options documented
   - [ ] Security considerations listed
   - [ ] Migration guide provided

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
