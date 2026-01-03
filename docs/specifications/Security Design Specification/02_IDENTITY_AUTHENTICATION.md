# ScratchBird Identity and Authentication Specification

**Document ID**: SBSEC-02  
**Version**: 1.0  
**Status**: Draft for Review  
**Date**: January 2026  
**Scope**: All deployment modes, Security Levels 1-6  

---

## 1. Introduction

### 1.1 Purpose

This document defines the identity model and authentication framework for the ScratchBird database engine. It specifies how users are identified, how credentials are validated, and how external authentication systems integrate with ScratchBird.

### 1.2 Scope

This specification covers:
- Identity model (Users, AuthKeys, Sessions)
- Internal authentication (password-based)
- External authentication (LDAP, Kerberos, OIDC)
- UDR authentication providers
- Credential storage and handling
- Password policies
- Account lifecycle

### 1.3 Related Documents

| Document ID | Title |
|-------------|-------|
| SBSEC-01 | Security Architecture |
| SBSEC-03 | Authorization Model |
| SBSEC-05 | IPC Security |

### 1.4 Security Level Applicability

| Feature | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|---------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Authentication required | | ● | ● | ● | ● | ● | ● |
| Password policies | | ○ | ○ | ● | ● | ● | ● |
| External auth (UDR) | | ○ | ○ | ○ | ● | ● | ● |
| Multi-factor auth | | | | ○ | ○ | ● | ● |
| Certificate auth | | | | | ○ | ● | ● |
| Centralized identity (cluster) | | | | | | | ● |

● = Required | ○ = Optional | (blank) = Not applicable

---

## 2. Identity Model

### 2.1 Overview

ScratchBird uses a three-tier identity model:

```
┌─────────────────────────────────────────────────────────────────┐
│                      IDENTITY HIERARCHY                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                     USER (Persistent)                    │    │
│  │                                                          │    │
│  │  • UUID v7 (immutable, never reused)                    │    │
│  │  • Username (mutable, for display/login)                │    │
│  │  • Credential store reference                           │    │
│  │  • Account status                                       │    │
│  │  • Role and group memberships                           │    │
│  │  • Metadata (created, modified, last login)             │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              │ authenticates, creates            │
│                              ▼                                   │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                   AUTHKEY (Per-Authentication)           │    │
│  │                                                          │    │
│  │  • UUID v7                                              │    │
│  │  • Bound to User UUID                                   │    │
│  │  • Authentication source (internal, LDAP, etc.)         │    │
│  │  • Issue timestamp                                      │    │
│  │  • Expiry timestamp                                     │    │
│  │  • Usage limits (optional)                              │    │
│  │  • Allowed roles subset (optional)                      │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              │ bound to                          │
│                              ▼                                   │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    SESSION (Ephemeral)                   │    │
│  │                                                          │    │
│  │  • UUID v7                                              │    │
│  │  • Bound to AuthKey UUID                                │    │
│  │  • Current transaction                                  │    │
│  │  • Security Context                                     │    │
│  │  • Dialect/namespace binding                            │    │
│  │  • Connection metadata                                  │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 User

A User represents a persistent identity within ScratchBird.

#### 2.2.1 User Record Structure

```sql
CREATE TABLE sys.sec_users (
    user_uuid           UUID PRIMARY KEY,       -- UUID v7, immutable
    username            VARCHAR(128) NOT NULL,  -- Display name, mutable
    username_normalized VARCHAR(128) NOT NULL,  -- Lowercase for comparison
    
    -- Credential reference (internal auth)
    credential_id       UUID,                   -- FK to credential store
    
    -- Account status
    status              ENUM('ACTIVE', 'BLOCKED', 'SUSPENDED', 
                             'PENDING_ACTIVATION', 'PENDING_PASSWORD_RESET'),
    status_reason       VARCHAR(256),
    status_changed_at   TIMESTAMP,
    status_changed_by   UUID,                   -- User who changed status
    
    -- Flags
    is_superuser        BOOLEAN DEFAULT FALSE,
    is_system           BOOLEAN DEFAULT FALSE,  -- System accounts (non-login)
    requires_mfa        BOOLEAN DEFAULT FALSE,
    
    -- External identity mapping
    external_id         VARCHAR(256),           -- DN, UPN, or external identifier
    auth_source         VARCHAR(64),            -- 'internal', 'ldap', 'kerberos', etc.
    
    -- Defaults
    default_schema_uuid UUID,
    default_role_uuid   UUID,
    
    -- Metadata
    created_at          TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    created_by          UUID,
    modified_at         TIMESTAMP,
    modified_by         UUID,
    last_login_at       TIMESTAMP,
    last_login_address  VARCHAR(256),
    failed_login_count  INTEGER DEFAULT 0,
    locked_until        TIMESTAMP,
    
    -- Constraints
    UNIQUE (username_normalized),
    UNIQUE (external_id, auth_source)
);
```

#### 2.2.2 User UUID Properties

The User UUID:
- Is generated as UUID v7 (time-ordered, random)
- Is immutable for the lifetime of the user
- Is never reused, even after user deletion
- Is the primary key for all authorization decisions
- May be referenced in audit logs indefinitely after user deletion

#### 2.2.3 Username Handling

Usernames are for human readability and login convenience:
- Usernames may be changed without affecting permissions
- All permission grants reference User UUID, not username
- Username comparison is case-insensitive (normalized to lowercase)
- Usernames must match pattern: `[a-zA-Z][a-zA-Z0-9_@.-]{0,127}`

#### 2.2.4 Account Status

| Status | Description | Can Login | Can Complete Active Txn |
|--------|-------------|:---------:|:-----------------------:|
| ACTIVE | Normal operational state | ✓ | ✓ |
| BLOCKED | Administratively disabled | | ✓ (with timeout) |
| SUSPENDED | Temporarily disabled (policy) | | ✓ (with timeout) |
| PENDING_ACTIVATION | New account awaiting activation | | |
| PENDING_PASSWORD_RESET | Must reset password before use | Limited | |

### 2.3 AuthKey

An AuthKey represents a successful authentication event and its validity window.

#### 2.3.1 AuthKey Record Structure

```sql
CREATE TABLE sys.sec_authkeys (
    authkey_uuid        UUID PRIMARY KEY,       -- UUID v7
    user_uuid           UUID NOT NULL,          -- FK to users
    
    -- Authentication source
    auth_source         VARCHAR(64) NOT NULL,   -- Provider that issued this
    auth_method         VARCHAR(64),            -- Specific method (password, cert, etc.)
    
    -- Validity
    issued_at           TIMESTAMP NOT NULL,
    expires_at          TIMESTAMP,              -- NULL = session-only
    not_before          TIMESTAMP,              -- Optional: validity window start
    
    -- Usage limits
    max_uses            INTEGER,                -- NULL = unlimited
    current_uses        INTEGER DEFAULT 0,
    
    -- Restrictions
    allowed_roles       UUID[],                 -- NULL = all user's roles
    allowed_addresses   INET[],                 -- NULL = any address
    
    -- Binding
    client_fingerprint  BYTEA,                  -- TLS cert fingerprint if mTLS
    
    -- Status
    status              ENUM('ACTIVE', 'EXPIRED', 'REVOKED', 'EXHAUSTED'),
    revoked_at          TIMESTAMP,
    revoked_by          UUID,
    revoke_reason       VARCHAR(256),
    
    -- Metadata
    issued_address      VARCHAR(256),
    user_agent          VARCHAR(256),
    
    FOREIGN KEY (user_uuid) REFERENCES sys.sec_users(user_uuid)
);
```

#### 2.3.2 AuthKey Lifecycle

```
┌─────────────────────────────────────────────────────────────────┐
│                    AUTHKEY LIFECYCLE                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. ISSUANCE                                                     │
│     • User provides credentials                                  │
│     • Authentication provider validates                          │
│     • AuthKey created with:                                      │
│       - New UUID v7                                              │
│       - Bound to User UUID                                       │
│       - Expiry based on policy or provider                       │
│       - Optional restrictions (roles, addresses)                 │
│                                                                  │
│  2. ACTIVE                                                       │
│     • Session may be created using AuthKey                       │
│     • Each use increments current_uses                           │
│     • Validity checked on each session creation                  │
│                                                                  │
│  3. TERMINATION (one of):                                        │
│     • EXPIRED: expires_at reached                                │
│     • EXHAUSTED: current_uses >= max_uses                        │
│     • REVOKED: Administratively or security revocation           │
│                                                                  │
│  4. POST-TERMINATION                                             │
│     • AuthKey record retained for audit                          │
│     • No new sessions may use this AuthKey                       │
│     • Existing sessions continue until their termination         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 2.3.3 AuthKey Validation

Before creating a session from an AuthKey:

```python
def validate_authkey(authkey, request_context):
    # Status check
    if authkey.status != 'ACTIVE':
        return AUTHKEY_INVALID
    
    # Time validity
    now = current_timestamp()
    if authkey.not_before and now < authkey.not_before:
        return AUTHKEY_NOT_YET_VALID
    if authkey.expires_at and now > authkey.expires_at:
        authkey.status = 'EXPIRED'
        return AUTHKEY_EXPIRED
    
    # Usage limits
    if authkey.max_uses and authkey.current_uses >= authkey.max_uses:
        authkey.status = 'EXHAUSTED'
        return AUTHKEY_EXHAUSTED
    
    # Address restriction
    if authkey.allowed_addresses:
        if request_context.address not in authkey.allowed_addresses:
            return AUTHKEY_ADDRESS_DENIED
    
    # Client binding (mTLS)
    if authkey.client_fingerprint:
        if request_context.tls_fingerprint != authkey.client_fingerprint:
            return AUTHKEY_BINDING_MISMATCH
    
    # User status check
    user = get_user(authkey.user_uuid)
    if user.status != 'ACTIVE':
        return USER_NOT_ACTIVE
    
    # Success - increment usage
    authkey.current_uses += 1
    return AUTHKEY_VALID
```

### 2.4 Session

Sessions are defined in SBSEC-01 Section 5. This section covers authentication-specific aspects.

#### 2.4.1 Session-AuthKey Binding

Each session is bound to exactly one AuthKey:
- Session inherits User UUID from AuthKey
- Session inherits role restrictions from AuthKey
- Session validity is independent of AuthKey validity
- AuthKey revocation does not immediately terminate sessions (policy-dependent)

#### 2.4.2 Multiple Sessions

A single AuthKey may be used to create multiple sessions (subject to max_uses):
- Each session is independent
- Sessions do not share state
- Sessions may have different active roles (within AuthKey's allowed_roles)

### 2.5 External Identity Mapping

External identities (from LDAP, Kerberos, OIDC) are mapped to internal User UUIDs.

#### 2.5.1 Mapping Table

```sql
CREATE TABLE sys.sec_identity_mappings (
    mapping_uuid        UUID PRIMARY KEY,
    
    -- External identity
    auth_source         VARCHAR(64) NOT NULL,   -- 'ldap', 'kerberos', 'oidc:issuer'
    external_id         VARCHAR(256) NOT NULL,  -- DN, UPN, subject
    external_username   VARCHAR(128),           -- Display name from external
    
    -- Internal identity
    user_uuid           UUID NOT NULL,          -- FK to users
    
    -- Mapping metadata
    created_at          TIMESTAMP NOT NULL,
    last_used_at        TIMESTAMP,
    use_count           INTEGER DEFAULT 0,
    
    -- Auto-provisioning
    auto_provisioned    BOOLEAN DEFAULT FALSE,
    
    UNIQUE (auth_source, external_id),
    FOREIGN KEY (user_uuid) REFERENCES sys.sec_users(user_uuid)
);
```

#### 2.5.2 Mapping Resolution

```
┌─────────────────────────────────────────────────────────────────┐
│                  IDENTITY MAPPING FLOW                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. External authentication succeeds                             │
│     → Provider returns external_id                               │
│                                                                  │
│  2. Lookup mapping: (auth_source, external_id)                   │
│     → If found: return user_uuid                                 │
│     → If not found: continue to step 3                           │
│                                                                  │
│  3. Check auto-provisioning policy                               │
│     → If disabled: return MAPPING_NOT_FOUND                      │
│     → If enabled: continue to step 4                             │
│                                                                  │
│  4. Auto-provision user                                          │
│     a. Create User record with new UUID                          │
│     b. Set auth_source, external_id                              │
│     c. Apply default role/group from provisioning policy         │
│     d. Create mapping record                                     │
│     e. Log provisioning event                                    │
│     → Return new user_uuid                                       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Authentication Framework

### 3.1 Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                 AUTHENTICATION ARCHITECTURE                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    Auth Dispatcher                         │  │
│  │                                                            │  │
│  │  • Receives credentials from IPC                          │  │
│  │  • Determines auth source from username or config         │  │
│  │  • Routes to appropriate provider                         │  │
│  │  • Handles provider response                              │  │
│  │  • Creates AuthKey on success                             │  │
│  └─────────────────────────────┬─────────────────────────────┘  │
│                                │                                 │
│            ┌───────────────────┼───────────────────┐            │
│            ▼                   ▼                   ▼            │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐   │
│  │    Internal     │ │   UDR Provider  │ │   UDR Provider  │   │
│  │    Provider     │ │   (LDAP)        │ │   (Kerberos)    │   │
│  │                 │ │                 │ │                 │   │
│  │  • Password     │ │  • LDAP bind    │ │  • GSSAPI       │   │
│  │    validation   │ │  • Group fetch  │ │  • Ticket       │   │
│  │  • Hash verify  │ │  • Mapping      │ │    validation   │   │
│  └─────────────────┘ └─────────────────┘ └─────────────────┘   │
│                                                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                   Provider Registry                        │  │
│  │                                                            │  │
│  │  • Registered providers with priority                     │  │
│  │  • Provider health monitoring                             │  │
│  │  • Fallback chain configuration                           │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Authentication Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                   AUTHENTICATION FLOW                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Parser                    Engine                                │
│    │                         │                                   │
│    │  AUTH_REQUEST           │                                   │
│    │  {dialect, creds}       │                                   │
│    │────────────────────────►│                                   │
│    │                         │                                   │
│    │                         │  1. Extract credentials           │
│    │                         │  2. Determine auth source         │
│    │                         │  3. Lookup/validate user          │
│    │                         │  4. Check account status          │
│    │                         │  5. Route to provider             │
│    │                         │                                   │
│    │                         │         Provider                  │
│    │                         │            │                      │
│    │                         │  Validate  │                      │
│    │                         │───────────►│                      │
│    │                         │            │                      │
│    │                         │   Result   │                      │
│    │                         │◄───────────│                      │
│    │                         │                                   │
│    │                         │  6. On success:                   │
│    │                         │     - Create AuthKey              │
│    │                         │     - Create Session              │
│    │                         │     - Map external groups         │
│    │                         │     - Log auth event              │
│    │                         │                                   │
│    │  AUTH_SUCCESS           │                                   │
│    │  {session_uuid}         │                                   │
│    │◄────────────────────────│                                   │
│    │                         │                                   │
│    │        OR               │                                   │
│    │                         │                                   │
│    │  AUTH_FAILURE           │                                   │
│    │  {error_code}           │                                   │
│    │◄────────────────────────│                                   │
│    │                         │                                   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 3.3 Auth Source Determination

The authentication source is determined by:

1. **Explicit prefix**: Username contains `@source` suffix
   - `alice@ldap` → LDAP provider
   - `bob@kerberos` → Kerberos provider
   - `charlie` or `charlie@internal` → Internal provider

2. **Domain mapping**: Username domain maps to provider
   - `alice@corp.example.com` → Mapped to LDAP via configuration

3. **Default provider**: Configured default when no match
   - Policy: `auth.default_provider = internal`

4. **Provider chain**: Try providers in priority order until success

### 3.4 Authentication Result

```c
typedef struct AuthResult {
    AuthResultCode  code;           // SUCCESS, FAILURE, LOCKED, etc.
    
    // On success
    uuid_t          user_uuid;      // Internal user identity
    uuid_t          authkey_uuid;   // Newly created AuthKey
    
    // From external providers
    char**          external_groups; // Groups to map to roles
    int             group_count;
    time_t          auth_expiry;    // Provider-specified expiry
    
    // On failure
    AuthFailureReason reason;
    char*           message;        // Human-readable (no secrets)
    int             remaining_attempts;
    time_t          locked_until;   // If account locked
    
} AuthResult;
```

---

## 4. Internal Authentication

### 4.1 Overview

Internal authentication validates credentials against ScratchBird's credential store.

### 4.2 Credential Storage

#### 4.2.1 Credential Record

```sql
CREATE TABLE sys.sec_credentials (
    credential_uuid     UUID PRIMARY KEY,
    user_uuid           UUID NOT NULL,
    
    -- Password hash
    hash_algorithm      VARCHAR(32) NOT NULL,   -- 'argon2id', 'bcrypt', etc.
    hash_params         JSONB,                  -- Algorithm parameters
    password_hash       BYTEA NOT NULL,         -- The hash value
    
    -- Password metadata
    created_at          TIMESTAMP NOT NULL,
    expires_at          TIMESTAMP,              -- NULL = never expires
    must_change         BOOLEAN DEFAULT FALSE,
    
    -- History (for reuse prevention)
    previous_hashes     BYTEA[],                -- Last N hashes
    
    FOREIGN KEY (user_uuid) REFERENCES sys.sec_users(user_uuid)
);
```

#### 4.2.2 Hash Algorithms

| Algorithm | Security Level | Parameters | Notes |
|-----------|----------------|------------|-------|
| Argon2id | 3+ (Required 5+) | memory=64MB, iterations=3, parallelism=4 | Default for new passwords |
| bcrypt | 1+ | cost=12 | Legacy support |
| scrypt | 1+ | N=2^14, r=8, p=1 | Legacy support |

#### 4.2.3 Password Hashing

```python
def hash_password(password, algorithm='argon2id'):
    if algorithm == 'argon2id':
        # Argon2id with recommended parameters
        params = {
            'memory_cost': 65536,    # 64 MB
            'time_cost': 3,          # 3 iterations
            'parallelism': 4,
            'hash_len': 32,
            'salt_len': 16
        }
        salt = secure_random_bytes(params['salt_len'])
        hash_value = argon2id(
            password.encode('utf-8'),
            salt,
            **params
        )
        return {
            'algorithm': 'argon2id',
            'params': params,
            'hash': salt + hash_value
        }
```

#### 4.2.4 Password Verification

```python
def verify_password(password, credential):
    # Constant-time comparison to prevent timing attacks
    computed_hash = compute_hash(
        password,
        credential.hash_algorithm,
        credential.hash_params,
        extract_salt(credential.password_hash)
    )
    return constant_time_compare(
        computed_hash,
        credential.password_hash
    )
```

### 4.3 Password Policies

#### 4.3.1 Policy Configuration

```sql
-- Password policy table
CREATE TABLE sys.sec_password_policies (
    policy_uuid         UUID PRIMARY KEY,
    policy_name         VARCHAR(64) NOT NULL UNIQUE,
    
    -- Complexity
    min_length          INTEGER DEFAULT 12,
    max_length          INTEGER DEFAULT 128,
    require_uppercase   BOOLEAN DEFAULT TRUE,
    require_lowercase   BOOLEAN DEFAULT TRUE,
    require_digit       BOOLEAN DEFAULT TRUE,
    require_special     BOOLEAN DEFAULT TRUE,
    disallow_username   BOOLEAN DEFAULT TRUE,
    
    -- History
    history_count       INTEGER DEFAULT 5,      -- Can't reuse last N
    
    -- Expiry
    max_age_days        INTEGER DEFAULT 90,     -- 0 = never expires
    warn_days_before    INTEGER DEFAULT 14,
    
    -- Lockout
    max_failed_attempts INTEGER DEFAULT 5,
    lockout_duration    INTERVAL DEFAULT '30 minutes',
    lockout_reset_after INTERVAL DEFAULT '15 minutes',
    
    -- Additional
    min_change_interval INTERVAL DEFAULT '1 day',  -- Prevent rapid changes
    
    is_default          BOOLEAN DEFAULT FALSE
);
```

#### 4.3.2 Password Validation

```python
def validate_password(password, username, policy):
    errors = []
    
    # Length
    if len(password) < policy.min_length:
        errors.append(f"Minimum length is {policy.min_length}")
    if len(password) > policy.max_length:
        errors.append(f"Maximum length is {policy.max_length}")
    
    # Complexity
    if policy.require_uppercase and not any(c.isupper() for c in password):
        errors.append("Must contain uppercase letter")
    if policy.require_lowercase and not any(c.islower() for c in password):
        errors.append("Must contain lowercase letter")
    if policy.require_digit and not any(c.isdigit() for c in password):
        errors.append("Must contain digit")
    if policy.require_special and not any(c in SPECIAL_CHARS for c in password):
        errors.append("Must contain special character")
    
    # Username check
    if policy.disallow_username:
        if username.lower() in password.lower():
            errors.append("Must not contain username")
    
    # Common password check (optional)
    if is_common_password(password):
        errors.append("Password is too common")
    
    return errors
```

#### 4.3.3 Account Lockout

```
┌─────────────────────────────────────────────────────────────────┐
│                   ACCOUNT LOCKOUT FLOW                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. Authentication attempt fails                                 │
│                                                                  │
│  2. Increment failed_login_count                                 │
│                                                                  │
│  3. If failed_login_count >= max_failed_attempts:                │
│     a. Set locked_until = now + lockout_duration                 │
│     b. Set status = SUSPENDED                                    │
│     c. Log security event (ACCOUNT_LOCKED)                       │
│     d. Optionally notify user/admin                              │
│                                                                  │
│  4. Return AUTH_LOCKED with remaining lockout time               │
│                                                                  │
│  ─────────────────────────────────────────────────────────────   │
│                                                                  │
│  Lockout Reset:                                                  │
│                                                                  │
│  • Automatic: After lockout_duration expires                     │
│    - locked_until checked on next attempt                        │
│    - If expired: reset failed_login_count, clear locked_until    │
│                                                                  │
│  • Administrative: SYSTEM_ADMINISTRATOR unlocks account          │
│    - Immediate reset of lockout state                            │
│    - Audit logged                                                │
│                                                                  │
│  • Gradual Reset: After lockout_reset_after of no attempts       │
│    - failed_login_count decremented                              │
│    - Prevents permanent lockout from occasional typos            │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 4.4 Password Change

```sql
-- Password change procedure
ALTER USER username SET PASSWORD 'new_password';

-- Force password change on next login
ALTER USER username SET PASSWORD EXPIRED;

-- Administrative reset
ALTER USER username SET PASSWORD 'temp_password' MUST_CHANGE;
```

Password change validation:
1. Verify current password (unless admin reset)
2. Validate new password against policy
3. Check password history (reuse prevention)
4. Hash new password
5. Store new credential
6. Add old hash to history
7. Clear must_change flag
8. Log password change event
9. Optionally invalidate existing AuthKeys

---

## 5. External Authentication

### 5.1 UDR Provider Framework

External authentication is implemented via User Defined Routines (UDRs) that implement the authentication provider interface.

#### 5.1.1 Provider Interface

```c
typedef struct AuthProvider {
    // Provider identification
    char*           provider_name;      // Unique name
    char*           provider_type;      // 'ldap', 'kerberos', 'oidc', etc.
    int             priority;           // Lower = tried first
    
    // Capabilities
    bool            supports_password;
    bool            supports_token;
    bool            supports_certificate;
    bool            supports_mfa;
    bool            provides_groups;
    
    // Entry points
    AuthResult*     (*authenticate)(AuthRequest* request);
    GroupList*      (*get_groups)(char* external_id);
    bool            (*validate_token)(char* token);
    void            (*cleanup)(void);
    
} AuthProvider;
```

#### 5.1.2 Provider Registration

```sql
-- Register an authentication provider
INSERT INTO sys.sec_auth_providers (
    provider_uuid,
    provider_name,
    provider_type,
    priority,
    library_path,
    library_checksum,
    config,
    sandbox_level,
    is_enabled
) VALUES (
    gen_uuid_v7(),
    'corporate_ldap',
    'ldap',
    100,
    '/opt/scratchbird/auth/ldap_provider.so',
    'sha256:abc123...',
    '{"server": "ldap.corp.example.com", "base_dn": "dc=corp,dc=example,dc=com"}',
    'RESTRICTED',
    TRUE
);
```

#### 5.1.3 Provider Trust Model

Authentication UDRs are security-critical and subject to strict controls:

| Control | Description |
|---------|-------------|
| Registration | Only CLUSTER_ADMIN may register providers |
| Checksum | Library checksum verified on load |
| Signature | Optional cryptographic signature verification |
| Sandbox | Providers run in configurable sandbox |
| Monitoring | Periodic checksum re-verification |
| Audit | All provider operations logged |

### 5.2 LDAP Authentication

#### 5.2.1 Configuration

```json
{
    "server": "ldap.corp.example.com",
    "port": 636,
    "use_tls": true,
    "tls_verify": "full",
    "tls_ca_cert": "/etc/ssl/certs/corp-ca.pem",
    
    "bind_dn": "cn=scratchbird,ou=services,dc=corp,dc=example,dc=com",
    "bind_password_ref": "vault:ldap/bind_password",
    
    "user_search_base": "ou=users,dc=corp,dc=example,dc=com",
    "user_search_filter": "(&(objectClass=person)(sAMAccountName={username}))",
    "user_id_attribute": "sAMAccountName",
    "user_dn_attribute": "distinguishedName",
    
    "group_search_base": "ou=groups,dc=corp,dc=example,dc=com",
    "group_search_filter": "(&(objectClass=group)(member={user_dn}))",
    "group_name_attribute": "cn",
    
    "connection_timeout": 5,
    "search_timeout": 10,
    "pool_size": 5
}
```

#### 5.2.2 LDAP Authentication Flow

```
1. Receive username and password
2. Connect to LDAP server (TLS)
3. Bind with service account
4. Search for user by username
5. If not found: return USER_NOT_FOUND
6. Attempt bind with user's DN and provided password
7. If bind fails: return INVALID_CREDENTIALS
8. Fetch user's group memberships
9. Map LDAP groups to ScratchBird roles/groups
10. Return success with external_id = user DN
```

### 5.3 Kerberos Authentication

#### 5.3.1 Configuration

```json
{
    "realm": "CORP.EXAMPLE.COM",
    "kdc_servers": ["kdc1.corp.example.com", "kdc2.corp.example.com"],
    "service_principal": "scratchbird/db.corp.example.com@CORP.EXAMPLE.COM",
    "keytab_path": "/etc/scratchbird/scratchbird.keytab",
    
    "accept_forwardable": true,
    "accept_proxiable": false,
    
    "map_principal_to_user": true,
    "principal_strip_realm": true
}
```

#### 5.3.2 Kerberos Authentication Flow

```
1. Client initiates GSSAPI authentication (wire protocol specific)
2. Receive client's service ticket
3. Validate ticket using service keytab
4. Extract client principal from ticket
5. Map principal to username (strip realm if configured)
6. Lookup or auto-provision user
7. Fetch group memberships from ticket or directory
8. Return success with external_id = principal
```

### 5.4 OIDC Authentication

#### 5.4.1 Configuration

```json
{
    "issuer": "https://auth.corp.example.com",
    "client_id": "scratchbird-db",
    "client_secret_ref": "vault:oidc/client_secret",
    
    "discovery_url": "https://auth.corp.example.com/.well-known/openid-configuration",
    
    "scopes": ["openid", "profile", "groups"],
    
    "claims_mapping": {
        "username": "preferred_username",
        "email": "email",
        "groups": "groups"
    },
    
    "token_validation": {
        "verify_signature": true,
        "verify_audience": true,
        "allowed_audiences": ["scratchbird-db"],
        "clock_skew_seconds": 60
    }
}
```

#### 5.4.2 OIDC Authentication Flow

```
1. Receive access token or ID token from client
2. Validate token signature against issuer's JWKS
3. Validate token claims (issuer, audience, expiry)
4. Extract user identity from claims
5. Extract groups from claims (if present)
6. Map to internal user
7. Return success with external_id = subject claim
```

### 5.5 External Group Mapping

External groups are mapped to ScratchBird roles and groups:

#### 5.5.1 Mapping Configuration

```sql
CREATE TABLE sys.sec_group_mappings (
    mapping_uuid        UUID PRIMARY KEY,
    
    -- External group
    auth_source         VARCHAR(64) NOT NULL,
    external_group      VARCHAR(256) NOT NULL,  -- DN, name, or pattern
    match_type          ENUM('EXACT', 'PREFIX', 'SUFFIX', 'REGEX'),
    
    -- Internal target
    target_type         ENUM('ROLE', 'GROUP'),
    target_uuid         UUID NOT NULL,          -- Role or group UUID
    
    -- Behavior
    auto_grant          BOOLEAN DEFAULT TRUE,   -- Grant on login
    auto_revoke         BOOLEAN DEFAULT FALSE,  -- Revoke when removed externally
    
    -- Priority (for conflicts)
    priority            INTEGER DEFAULT 100,
    
    UNIQUE (auth_source, external_group, target_uuid)
);
```

#### 5.5.2 Group Mapping Example

```sql
-- Map LDAP group to ScratchBird role
INSERT INTO sys.sec_group_mappings (
    mapping_uuid, auth_source, external_group, match_type,
    target_type, target_uuid, auto_grant, auto_revoke
) VALUES (
    gen_uuid_v7(),
    'ldap',
    'cn=database_admins,ou=groups,dc=corp,dc=example,dc=com',
    'EXACT',
    'ROLE',
    (SELECT role_uuid FROM sys.sec_roles WHERE role_name = 'db_admin'),
    TRUE,
    TRUE
);

-- Map all LDAP groups starting with 'app_' to ScratchBird groups
INSERT INTO sys.sec_group_mappings (
    mapping_uuid, auth_source, external_group, match_type,
    target_type, target_uuid, auto_grant, auto_revoke
) VALUES (
    gen_uuid_v7(),
    'ldap',
    'cn=app_*',
    'PREFIX',
    'GROUP',
    NULL,  -- Auto-create matching group
    TRUE,
    FALSE
);
```

---

## 6. Multi-Factor Authentication

### 6.1 Overview

Multi-factor authentication (MFA) is supported for Security Levels 3+ and required for Levels 5-6.

### 6.2 MFA Methods

| Method | Type | Security Level |
|--------|------|----------------|
| TOTP | Something you have | 3+ |
| Hardware token (FIDO2) | Something you have | 4+ |
| Push notification | Something you have | 4+ |
| SMS (discouraged) | Something you have | 3 only |

### 6.3 MFA Enrollment

```sql
-- Enable MFA for user
ALTER USER alice ENABLE MFA;

-- Enroll TOTP
SELECT sys.sec_mfa_enroll_totp('alice');
-- Returns: {"secret": "base32...", "qr_uri": "otpauth://..."}

-- Verify enrollment with code
SELECT sys.sec_mfa_verify_enrollment('alice', '123456');

-- Enroll hardware token
SELECT sys.sec_mfa_enroll_fido2('alice', credential_data);
```

### 6.4 MFA Authentication Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                   MFA AUTHENTICATION FLOW                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. Primary authentication (password) succeeds                   │
│                                                                  │
│  2. Check if user requires MFA:                                  │
│     - User flag: requires_mfa                                    │
│     - Policy flag: policy.require_mfa                            │
│     - Security level: Level 5+ always requires                   │
│                                                                  │
│  3. If MFA required:                                             │
│     a. Return AUTH_MFA_REQUIRED with challenge                   │
│     b. Client prompts user for second factor                     │
│     c. Client sends MFA_RESPONSE with factor value               │
│                                                                  │
│  4. Validate MFA response:                                       │
│     - TOTP: Verify code against user's secret                    │
│     - FIDO2: Verify assertion against registered credential      │
│     - Push: Verify approval received                             │
│                                                                  │
│  5. If MFA valid:                                                │
│     - Create AuthKey with mfa_verified = true                    │
│     - Return AUTH_SUCCESS                                        │
│                                                                  │
│  6. If MFA invalid:                                              │
│     - Increment MFA failure count                                │
│     - Return AUTH_MFA_FAILED                                     │
│     - Apply lockout policy if threshold reached                  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 7. Wire Protocol Integration

### 7.1 Overview

Each wire protocol has its own authentication mechanism. The parser extracts credentials and passes them to the Engine in a normalized format.

### 7.2 Protocol-Specific Authentication

#### 7.2.1 PostgreSQL

| Method | Supported | Notes |
|--------|:---------:|-------|
| md5 | ✓ | Legacy, requires stored MD5 hash |
| scram-sha-256 | ✓ | Preferred |
| gss | ✓ | Kerberos via GSSAPI |
| sspi | ✓ | Windows integrated auth |
| cert | ✓ | Client certificate |
| password | ✓ | Cleartext over TLS only |

#### 7.2.2 MySQL

| Method | Supported | Notes |
|--------|:---------:|-------|
| mysql_native_password | ✓ | Legacy |
| caching_sha2_password | ✓ | Default in MySQL 8+ |
| auth_gssapi_client | ✓ | Kerberos |
| client_ed25519 | ✓ | Certificate |

#### 7.2.3 FirebirdSQL

| Method | Supported | Notes |
|--------|:---------:|-------|
| Srp | ✓ | Secure Remote Password |
| Srp256 | ✓ | SRP with SHA-256 |
| Legacy_Auth | | Disabled by default (insecure) |
| Certificate | ✓ | Client certificate |

#### 7.2.4 V2 (Native)

| Method | Supported | Notes |
|--------|:---------:|-------|
| password | ✓ | Over TLS |
| scram-sha-256 | ✓ | Preferred |
| gssapi | ✓ | Kerberos |
| certificate | ✓ | mTLS |
| token | ✓ | Bearer token (OIDC) |

### 7.3 Credential Normalization

The parser normalizes credentials to a common format:

```c
typedef struct NormalizedCredentials {
    char*           username;
    CredentialType  type;           // PASSWORD, TOKEN, CERTIFICATE, GSSAPI
    
    union {
        struct {
            uint8_t*    password;
            size_t      password_len;
        } password;
        
        struct {
            uint8_t*    token;
            size_t      token_len;
            char*       token_type;  // 'bearer', 'saml', etc.
        } token;
        
        struct {
            uint8_t*    cert_der;
            size_t      cert_len;
        } certificate;
        
        struct {
            uint8_t*    gss_token;
            size_t      token_len;
            char*       service_name;
        } gssapi;
    };
    
    // Additional context
    char*           client_address;
    char*           tls_fingerprint;
    char*           protocol_dialect;
    
} NormalizedCredentials;
```

---

## 8. Credential Caching (Cluster Mode)

### 8.1 Overview

In cluster mode, data databases cache credentials from security databases to maintain availability during network partitions.

### 8.2 Cache Structure

```sql
-- On data databases (non-security databases in cluster)
CREATE TABLE sys.local_credential_cache (
    user_uuid           UUID PRIMARY KEY,
    username            VARCHAR(128),
    password_hash       BYTEA,
    hash_algorithm      VARCHAR(32),
    user_status         VARCHAR(32),
    
    -- Cache metadata
    cached_at           TIMESTAMP NOT NULL,
    expires_at          TIMESTAMP NOT NULL,
    source_db_uuid      UUID,                   -- Security DB that provided this
    source_version      BIGINT,                 -- Version for invalidation
    
    -- Validation
    last_validated_at   TIMESTAMP,
    validation_failures INTEGER DEFAULT 0
);
```

### 8.3 Cache TTL

| Security Level | Normal TTL | Degraded TTL | Maximum TTL |
|----------------|------------|--------------|-------------|
| 1-4 | 5 minutes | 10 minutes | 1 hour |
| 5 | 2 minutes | 5 minutes | 30 minutes |
| 6 | 1 minute | 2 minutes | 10 minutes |

### 8.4 Cache Invalidation

Credentials are invalidated via:
1. **Push**: Security database broadcasts invalidation
2. **TTL**: Cache entry expires
3. **Version**: Source version mismatch detected
4. **Administrative**: Manual cache clear

### 8.5 Degraded Mode Authentication

When security database is unreachable:

```
1. Check local cache for user
2. If found and not expired:
   - Validate password against cached hash
   - Create session with degraded flag
   - Log degraded authentication event
3. If not found or expired:
   - Return AUTH_SERVICE_UNAVAILABLE
   - Client should retry or fail
```

Degraded mode restrictions:
- New user creation disabled
- Password changes disabled
- Role/group changes disabled
- Administrative functions disabled
- Audit marked as "degraded mode"

---

## 9. Security Considerations

### 9.1 Credential Handling

| Requirement | Implementation |
|-------------|----------------|
| No plaintext storage | All passwords hashed with strong algorithm |
| No plaintext logging | Credentials redacted in all logs |
| Memory clearing | Credentials cleared after use (secure_memzero) |
| Transport security | TLS required for password transmission |
| Timing attacks | Constant-time comparison for hash verification |

### 9.2 Brute Force Protection

| Control | Implementation |
|---------|----------------|
| Account lockout | After N failed attempts |
| Progressive delay | Increasing delay between attempts |
| Rate limiting | Per-IP and per-username limits |
| CAPTCHA | Optional for repeated failures |
| Monitoring | Alert on unusual failure patterns |

### 9.3 Session Security

| Requirement | Implementation |
|-------------|----------------|
| Session ID entropy | UUID v7 with 62 bits randomness |
| Session binding | Optional IP/fingerprint binding |
| Session timeout | Configurable idle timeout |
| Concurrent session limit | Configurable per-user limit |

---

## 10. Audit Events

### 10.1 Authentication Events

| Event | Logged Data |
|-------|-------------|
| AUTH_ATTEMPT | username, source, client_address, protocol |
| AUTH_SUCCESS | user_uuid, authkey_uuid, auth_method |
| AUTH_FAILURE | username, reason, failed_count |
| AUTH_LOCKED | user_uuid, locked_until, trigger |
| AUTH_UNLOCKED | user_uuid, unlocked_by, method |
| MFA_CHALLENGE | user_uuid, mfa_method |
| MFA_SUCCESS | user_uuid, mfa_method |
| MFA_FAILURE | user_uuid, mfa_method, reason |

### 10.2 Account Events

| Event | Logged Data |
|-------|-------------|
| USER_CREATED | user_uuid, created_by, auth_source |
| USER_MODIFIED | user_uuid, modified_by, changes |
| USER_BLOCKED | user_uuid, blocked_by, reason |
| PASSWORD_CHANGED | user_uuid, changed_by, forced |
| PASSWORD_EXPIRED | user_uuid |
| AUTHKEY_CREATED | authkey_uuid, user_uuid, expiry |
| AUTHKEY_REVOKED | authkey_uuid, revoked_by, reason |

---

## Appendix A: Configuration Reference

### A.1 Authentication Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `auth.mode` | enum | LOCAL | NONE, LOCAL, CLUSTER |
| `auth.default_provider` | string | internal | Default auth provider |
| `auth.allow_anonymous` | boolean | false | Allow anonymous connections |
| `auth.require_tls` | boolean | false | Require TLS for auth |
| `auth.require_mfa` | boolean | false | Require MFA globally |

### A.2 Password Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `password.min_length` | integer | 12 | Minimum password length |
| `password.require_complexity` | boolean | true | Require mixed characters |
| `password.history_count` | integer | 5 | Passwords to remember |
| `password.max_age_days` | integer | 90 | Days until expiry (0=never) |
| `password.hash_algorithm` | string | argon2id | Hash algorithm |

### A.3 Lockout Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `lockout.max_attempts` | integer | 5 | Attempts before lockout |
| `lockout.duration` | interval | 30m | Lockout duration |
| `lockout.reset_after` | interval | 15m | Reset counter after |

---

*End of Document*
