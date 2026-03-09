# Specification: Authentication Flow

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/authentication |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/security/auth_manager.cpp:1066`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/auth_manager.h:454`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/security/scram_auth.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/scram_auth.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/security/mfa_auth.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/security/auth_plugin_manager.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_security_issues.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_security_auth_provider_rule_chain_contract.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_security_mfa_policy_enrollment_recovery_contract.cpp`

## Synopsis

This specification defines the authentication flow for ScratchBird, including Host-Based Authentication (HBA) rule matching, SCRAM-SHA-256/512 challenge-response authentication, multi-factor authentication (MFA) chains, and the authentication plugin dispatch system.

## Scope

### In Scope

- HBA (Host-Based Authentication) rule parsing and matching
- SCRAM-SHA-256 and SCRAM-SHA-512 authentication flows
- Multi-factor authentication (MFA) with TOTP/HOTP/backup codes
- Authentication plugin registry and dispatch
- Rate limiting for authentication attempts
- Audit logging for authentication events

### Out of Scope

- Authorization and permission checking (see `authorization_model.md`)
- Row-Level Security (see `rls_policy_enforcement.md`)
- Column-Level Security (see `cls_column_masking.md`)
- Password policy enforcement (handled in `password_policy.cpp`)

## Background

ScratchBird implements a PostgreSQL-compatible authentication system with multiple authentication methods coordinated through the `AuthManager`. Authentication follows these phases:

1. **Connection Matching**: HBA rules determine which auth method applies
2. **Method Selection**: Auth method is selected based on connection properties
3. **Challenge-Response**: Multi-step authentication exchange
4. **MFA Verification**: Optional second factor verification
5. **Session Establishment**: Successful authentication creates session context

## Specification

### Data Structures

```cpp
// From /home/dcalford/CliWork/ScratchBird/include/scratchbird/security/auth_manager.h:73-114
struct HBARule {
    HBAConnectionType connection_type = HBAConnectionType::HOST;
    std::string database;           // "all", "sameuser", "samerole", "@file"
    std::vector<std::string> databases;
    bool database_is_regex = false;
    std::regex database_regex;
    
    std::string user;               // "all", "+role", "@file"
    std::vector<std::string> users;
    bool user_is_regex = false;
    std::regex user_regex;
    
    IPMatchType ip_match_type = IPMatchType::ANY;
    std::string address;            // IP, "all", "samehost", "samenet"
    uint8_t prefix_length = 0;      // CIDR prefix
    
    AuthType auth_type = AuthType::REJECT;
    std::map<std::string, std::string> auth_options;
    int line_number = 0;
};
```

```cpp
// From /home/dcalford/CliWork/ScratchBird/include/scratchbird/security/scram_auth.h:75-82
enum class ScramPhase : uint8_t {
    INITIAL = 0,
    CLIENT_FIRST_RECEIVED = 1,
    SERVER_FIRST_SENT = 2,
    CLIENT_FINAL_RECEIVED = 3,
    COMPLETE = 4,
    FAILED = 5
};
```

```cpp
// From /home/dcalford/CliWork/ScratchBird/include/scratchbird/security/auth_method.h:45-65
enum class AuthState : uint8_t {
    INITIAL = 0,
    IN_PROGRESS = 1,
    SUCCESS = 2,
    FAILURE = 3
};

enum class AuthType : uint8_t {
    TRUST = 0,
    REJECT = 1,
    PASSWORD = 2,
    MD5 = 3,
    SCRAM_SHA_256 = 4,
    SCRAM_SHA_512 = 5,
    CERTIFICATE = 6,
    LDAP = 7,
    KERBEROS = 8,
    PEER = 9,
    IDENT = 10,
    RADIUS = 11,
    PAM = 12,
    TOKEN = 13
};
```

### Interface Contracts

#### Function: `AuthManager::startAuthentication()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/security/auth_manager.cpp:1193
AuthResult AuthManager::startAuthentication(AuthContext& ctx);
```

**Preconditions:**
- `AuthContext` must have connection info set via `setConnectionInfo()`
- `AuthContext` must have username set via `setUsername()`
- AuthManager must be initialized via `initialize()`

**Postconditions:**
- On SUCCESS: User is authenticated, session can be established
- On IN_PROGRESS: Authentication state saved, client response required
- On FAILURE: Failure reason logged, rate limiter updated

**Error Handling:**
- Rate limit exceeded → `AuthFailReason::RATE_LIMITED`
- No matching HBA rule → `AuthFailReason::NOT_ALLOWED`
- Plugin unavailable → `AuthFailReason::INTERNAL_ERROR`

**Algorithm:**
1. Check rate limiting (`RateLimiter::allow()`)
2. Find matching HBA rule (`HBAConfig::findMatchingRule()`)
3. Resolve authentication method (plugin or legacy)
4. Apply client pinning checks (env vars)
5. Dispatch to authentication method
6. Log result and update rate limiter

#### Function: `ScramSHA256AuthMethod::continueAuth()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/security/scram_auth.cpp:473
AuthResult ScramSHA256AuthMethod::continueAuth(
    AuthContext& ctx,
    const std::vector<uint8_t>& data);
```

**Preconditions:**
- SCRAM state exists for this context
- Data contains valid SCRAM message

**Postconditions:**
- Phase transitions according to state machine
- On COMPLETE: Client proof verified, server signature generated

**Thread Safety:**
- Thread-safe via `states_mutex_`
- Each AuthContext has isolated state

### Algorithms

#### Algorithm: SCRAM-SHA-256 Authentication

```
Input:  Client-first-message (gs2-header,client-first-message-bare)
Output: Authentication success/failure

1. PARSE client-first message
   - Extract GS2 header (channel binding flag)
   - Extract username (n=)
   - Extract client nonce (r=)
   - Store client-first-message-bare

2. VALIDATE channel binding requirements
   - If channel_binding_required && gs2_flag == 'n' → FAIL

3. LOOKUP user credentials
   - Query CredentialStore for username
   - If not found, generate fake credentials (anti-enumeration)
   - Extract salt, iterations, stored_key, server_key

4. GENERATE server nonce
   - server_nonce = client_nonce + server_random(24 bytes)

5. BUILD server-first message
   - r=<server_nonce>,s=<base64(salt)>,i=<iterations>

6. WAIT for client-final message
   - c=<base64(channel_binding)>,r=<nonce>,p=<base64(proof)>

7. VERIFY client proof
   - AuthMessage = client-first-bare + "," + server-first + "," + client-final-without-proof
   - ClientSignature = HMAC(StoredKey, AuthMessage)
   - ClientKey = ClientProof XOR ClientSignature
   - ComputedStoredKey = H(ClientKey)
   - VERIFY constantTimeCompare(ComputedStoredKey, StoredKey)

8. GENERATE server signature
   - ServerSignature = HMAC(ServerKey, AuthMessage)

9. RETURN server-final with verifier
   - v=<base64(ServerSignature)>
```

**Complexity:**
- Time: O(1) - Fixed number of cryptographic operations
- Space: O(1) - Fixed-size state per authentication

### State Machines

```
┌───────────┐     start()      ┌──────────────────┐
│   IDLE    │ ───────────────► │  IN_PROGRESS     │
└───────────┘                  └──────────────────┘
                                      │
                                      │ client-first
                                      ▼
                              ┌──────────────────┐
                              │ AWAIT_CLIENT_    │
                              │ FIRST            │
                              └──────────────────┘
                                      │
                                      │ receive
                                      ▼
                              ┌──────────────────┐
                              │ SERVER_FIRST_    │
                              │ SENT             │
                              └──────────────────┘
                                      │
                                      │ client-final
                                      ▼
                              ┌──────────────────┐
                              │ AWAIT_CLIENT_    │
                              │ FINAL            │
                              └──────────────────┘
                                      │
                                      │ verify
                                      ▼
                          ┌─────────────────────┐
              proof valid │                     │ proof invalid
                 ┌───────►│      COMPLETE       │◄────────┐
                 │        │                     │         │
                 │        └─────────────────────┘         │
                 │                   │                    │
                 │                   │ success            │
                 │                   ▼                    │
                 │           ┌──────────────────┐        │
                 │           │      SUCCESS     │        │
                 │           └──────────────────┘        │
                 │                                         │
                 └─────────────────────────────────────────┘
```

| Current State | Event | Action | Next State |
|---------------|-------|--------|------------|
| IDLE | start() | Initialize state | IN_PROGRESS |
| IN_PROGRESS | client-first | Parse, lookup credentials, send server-first | SERVER_FIRST_SENT |
| SERVER_FIRST_SENT | client-final | Verify proof, generate signature | COMPLETE (success) or FAILED |
| COMPLETE | - | Return success | SUCCESS |
| FAILED | - | Return failure | (terminal) |

### MFA State Machine

```
┌──────────────────┐
│   PRIMARY_AUTH   │
└──────────────────┘
        │
        │ primary success
        ▼
┌──────────────────┐    MFA not required    ┌──────────┐
│  CHECK_MFA_REQ   │ ─────────────────────► │ SUCCESS  │
└──────────────────┘                        └──────────┘
        │ MFA required
        ▼
┌──────────────────┐
│  MFA_CHALLENGE   │
└──────────────────┘
        │
        │ send challenge
        ▼
┌──────────────────┐
│  MFA_VERIFY      │
└──────────────────┘
        │
        ├────────────┬────────────┐
        │ code valid │ code invalid, attempts < max
        ▼            │            │
┌──────────┐         │            ▼
│  SUCCESS │         │    ┌──────────────────┐
└──────────┘         │    │  MFA_RETRY       │
                     │    └──────────────────┘
                     │            │
                     │            │ retry
                     │            └────────────► MFA_VERIFY
                     │
                     ▼
            ┌──────────────────┐
            │  MFA_LOCKED      │ (too many attempts)
            └──────────────────┘
```

### Decision Trees

```
Authentication Request
│
├─ Rate limited? ──Yes──► Return RATE_LIMITED
│
├─ HBA enabled? ──Yes──► Find matching HBA rule
│                        └─ No match? ──► Return NOT_ALLOWED
│
├─ Plugin registry enabled?
│   ├─ Yes ──► Resolve method to plugin
│   │          ├─ Runtime available? ──Yes──► Use plugin runtime
│   │          └─ Legacy fallback allowed? ──No──► Return PLUGIN_UNAVAILABLE
│   └─ No ──► Use legacy auth method
│
├─ Client pinning checks
│   ├─ Required methods specified? ──► Verify method in list
│   ├─ Forbidden methods specified? ──► Verify method NOT in list
│   └─ Channel binding required? ──► Verify SCRAM with binding
│
└─ Dispatch to authentication method
   ├─ SUCCESS ──► Log success, reset rate limit
   ├─ IN_PROGRESS ──► Save state, return challenge
   └─ FAILURE ──► Log failure, increment rate limit
```

## Invariants

1. **Credential Constant-Time Comparison**: All password/credential comparisons use constant-time comparison
   - Verification: `constantTimeCompare()` in `auth_manager.cpp:1030`
   
2. **Anti-Enumeration**: When user not found, fake credentials are used to prevent user enumeration
   - Verification: `scram_auth.cpp:538-550`

3. **State Isolation**: Each authentication context has isolated state
   - Verification: States stored per-`AuthContext*` pointer

4. **Rate Limiting**: Failed attempts are tracked and locked after threshold
   - Verification: `RateLimiter` class in `auth_manager.h:185-251`

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `RATE_LIMITED` | Too many failed attempts | Wait for lockout duration |
| `INVALID_CREDENTIALS` | Password/proof mismatch | Return generic failure |
| `PROTOCOL_ERROR` | Invalid message format | Abort authentication |
| `NOT_ALLOWED` | HBA rejection or policy violation | Check configuration |
| `INTERNAL_ERROR` | System/plugin failure | Check logs, retry |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `test_security_issues.cpp` | Basic auth security tests |
| `test_catalog_security_auth_provider_rule_chain_contract.cpp` | Auth provider chains |
| `test_catalog_security_mfa_policy_enrollment_recovery_contract.cpp` | MFA enrollment/recovery |
| `test_domain_security.cpp` | Integration security tests |

## Migration Notes

The authentication system supports both legacy built-in methods and a new plugin registry:
- Plugin registry is the primary dispatch path when enabled
- Legacy fallback is available for backwards compatibility
- Environment variables provide runtime client pinning

## Related Specifications

- `auth_plugins.md` - All 17+ authentication plugins
- `hba_rules.md` - pg_hba.conf parsing and matching
- `password_management.md` - Password policies and expiration
- `authorization_model.md` - Permission checking after authentication
- `ssl_tls.md` - SSL/TLS secure transport
- `audit_logging.md` - Authentication event logging

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| HBA | Host-Based Authentication - rules for connection matching |
| SCRAM | Salted Challenge Response Authentication Mechanism (RFC 5802) |
| TOTP | Time-based One-Time Password (RFC 6238) |
| HOTP | HMAC-based One-Time Password (RFC 4226) |
| GS2 | Generic Security Service Application Program Interface |
| Channel Binding | Binding authentication to TLS channel to prevent MITM |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
