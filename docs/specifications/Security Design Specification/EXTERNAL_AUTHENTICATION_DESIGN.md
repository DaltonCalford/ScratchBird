# External Authentication Infrastructure Design

**Status**: Infrastructure Complete (Alpha)
**Implementation**: Beta (requires network connectivity)
**Date**: November 11, 2025

---

## Overview

This document describes the external authentication infrastructure for ScratchBird. The infrastructure (interfaces, types, and factory) is implemented in Alpha, but actual external provider implementations (LDAP, Active Directory, etc.) are deferred to Beta when network/non-local connectivity is available.

---

## Architecture

### Component Hierarchy

```
┌─────────────────────────────────────────────┐
│ Connection Context                          │
│ (tracks authenticated user)                 │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│ AuthProviderFactory                         │
│ (creates appropriate provider)              │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│ AuthProvider (interface)                    │
└───┬──────────┬──────────┬──────────┬────────┘
    │          │          │          │
    │          │          │          │
┌───▼────┐ ┌──▼────┐ ┌───▼────┐ ┌──▼────────┐
│ Local  │ │ LDAP  │ │   AD   │ │  OAuth2   │
│  ✅    │ │  📋   │ │   📋   │ │    📋     │
│(Alpha) │ │(Beta) │ │ (Beta) │ │  (Beta)   │
└────────┘ └───────┘ └────────┘ └───────────┘

✅ = Fully implemented
📋 = Infrastructure only (stub implementation)
```

---

## Provider Types

### Alpha (Implemented)

#### LocalAuthProvider
- **Status**: Fully implemented
- **Purpose**: Local password-based authentication
- **Features**:
  - BCrypt password hashing (Phase 3.0)
  - User lookup from catalog
  - Group membership resolution
  - Always available (no network required)

### Beta (Infrastructure Only)

#### LDAPAuthProvider
- **Status**: Stub implementation
- **Purpose**: LDAP/LDAPS authentication
- **Planned Features**:
  - LDAP bind authentication
  - User/group synchronization
  - TLS/SSL support
  - Configurable base DNs and filters

#### ActiveDirectoryAuthProvider
- **Status**: Stub implementation
- **Purpose**: Active Directory authentication
- **Planned Features**:
  - AD domain authentication
  - Kerberos support
  - Group membership from AD
  - Service account binding

#### Future Providers (Beta+)
- OAuth2Provider
- SAMLProvider
- KerberosProvider
- ExternalScriptProvider

---

## Interface Design

### Core Interface: AuthProvider

```cpp
class AuthProvider {
public:
    // Authenticate user with username/password
    virtual AuthResult authenticate(
        const std::string& username,
        const std::string& password,
        AuthUserInfo& user_info_out,
        std::string& error_msg_out) = 0;

    // Check if user exists
    virtual bool userExists(
        const std::string& username,
        AuthUserInfo& user_info_out) = 0;

    // Get user's external groups
    virtual bool getUserGroups(
        const std::string& username,
        std::vector<std::string>& groups_out) = 0;

    // Test provider connection
    virtual bool testConnection(std::string& error_msg_out) = 0;

    // Provider metadata
    virtual AuthProviderType getType() const = 0;
    virtual std::string getName() const = 0;
};
```

### Authentication Result Types

```cpp
enum class AuthResult {
    SUCCESS,                // Authentication successful
    INVALID_CREDENTIALS,    // Wrong username/password
    USER_DISABLED,          // Account disabled
    USER_LOCKED,            // Account locked
    USER_EXPIRED,           // Account expired
    NETWORK_ERROR,          // Network issue (Beta)
    PROVIDER_ERROR,         // Provider-specific error
    NOT_IMPLEMENTED         // Feature not available
};
```

### User Information Structure

```cpp
struct AuthUserInfo {
    std::string username;
    std::string display_name;
    std::string email;
    std::vector<std::string> external_groups;
    std::string external_id;
    bool is_disabled;
    bool is_locked;
};
```

---

## Authentication Flow

### Alpha (Local Authentication)

```
1. User provides username + password
         │
         ▼
2. LocalAuthProvider::authenticate()
         │
         ▼
3. Lookup user in catalog
         │
         ├─ Not found → INVALID_CREDENTIALS
         │
         ▼
4. Check if user is active
         │
         ├─ Inactive → USER_DISABLED
         │
         ▼
5. Verify password (BCrypt)
         │
         ├─ Invalid → INVALID_CREDENTIALS
         │
         ▼
6. Return SUCCESS + user info
```

### Beta (External Authentication)

```
1. User provides username + password
         │
         ▼
2. ExternalAuthProvider::authenticate()
         │
         ▼
3. Connect to external service (LDAP/AD)
         │
         ├─ Connection failed → NETWORK_ERROR
         │
         ▼
4. Bind/authenticate with external service
         │
         ├─ Auth failed → INVALID_CREDENTIALS
         │
         ▼
5. Retrieve user info + groups
         │
         ▼
6. Synchronize with local catalog (optional)
         │
         ▼
7. Return SUCCESS + user info
```

---

## Configuration Format

### Local Authentication (Alpha)

```json
{
  "type": "local",
  "config": {
    // No configuration needed
  }
}
```

### LDAP Authentication (Beta)

```json
{
  "type": "ldap",
  "config": {
    "server_uri": "ldaps://ldap.example.com:636",
    "bind_dn": "cn=admin,dc=example,dc=com",
    "bind_password": "secret",
    "user_base_dn": "ou=users,dc=example,dc=com",
    "group_base_dn": "ou=groups,dc=example,dc=com",
    "user_filter": "(uid={username})",
    "group_filter": "(memberUid={username})",
    "use_tls": true,
    "verify_cert": true,
    "timeout_seconds": 10
  }
}
```

### Active Directory (Beta)

```json
{
  "type": "active_directory",
  "config": {
    "domain": "EXAMPLE.COM",
    "domain_controller": "dc1.example.com",
    "bind_user": "svc_scratchbird",
    "bind_password": "secret",
    "use_kerberos": false,
    "use_ssl": true,
    "timeout_seconds": 10
  }
}
```

---

## Usage Examples

### Alpha - Local Authentication

```cpp
// Create local auth provider
auto catalog = db->catalog_manager();
auto auth = AuthProviderFactory::createDefault(catalog);

// Authenticate user
AuthUserInfo user_info;
std::string error_msg;
AuthResult result = auth->authenticate("alice", "password123", user_info, error_msg);

if (result == AuthResult::SUCCESS) {
    // Success - user_info contains user details
    std::cout << "Welcome, " << user_info.username << "!" << std::endl;
} else {
    // Failed - error_msg contains details
    std::cerr << "Authentication failed: " << error_msg << std::endl;
}
```

### Beta - LDAP Authentication (Planned)

```cpp
// Create LDAP auth provider (Beta)
LDAPAuthProvider::Config ldap_config;
ldap_config.server_uri = "ldaps://ldap.example.com:636";
ldap_config.bind_dn = "cn=admin,dc=example,dc=com";
ldap_config.bind_password = "secret";
ldap_config.user_base_dn = "ou=users,dc=example,dc=com";
ldap_config.group_base_dn = "ou=groups,dc=example,dc=com";
ldap_config.user_filter = "(uid={username})";
ldap_config.use_tls = true;

auto auth = std::make_unique<LDAPAuthProvider>(ldap_config);

// Test connection first
std::string conn_error;
if (!auth->testConnection(conn_error)) {
    std::cerr << "LDAP connection failed: " << conn_error << std::endl;
    return;
}

// Authenticate user
AuthUserInfo user_info;
std::string error_msg;
AuthResult result = auth->authenticate("alice", "password123", user_info, error_msg);

if (result == AuthResult::SUCCESS) {
    // Synchronize external groups with local catalog
    std::vector<std::string> external_groups;
    if (auth->getUserGroups("alice", external_groups)) {
        // Map external groups to local groups
        for (const auto& group_name : external_groups) {
            // Create/update group mapping
        }
    }
}
```

---

## Security Considerations

### Password Handling

1. **Alpha (Local)**:
   - Passwords hashed with BCrypt (cost 12)
   - Salts generated with OpenSSL RAND_bytes
   - Timing-safe comparison
   - Never logged or exposed

2. **Beta (External)**:
   - Passwords transmitted securely (TLS/SSL)
   - Never stored locally
   - Bind operations use service account
   - Connection pooling with authentication

### Connection Security (Beta)

1. **TLS/SSL**:
   - Required for production use
   - Certificate verification mandatory
   - Mutual TLS support planned

2. **Credential Storage**:
   - Service account credentials encrypted
   - Rotation policy enforcement
   - Key management integration

3. **Network Isolation**:
   - Authentication traffic on separate VLAN
   - Firewall rules for provider access
   - Rate limiting and DDoS protection

---

## Implementation Roadmap

### ✅ Alpha (Current)
- [x] AuthProvider interface
- [x] AuthResult enum and types
- [x] LocalAuthProvider (full implementation)
- [x] AuthProviderFactory
- [x] Stub implementations for external providers
- [x] Documentation

### 📋 Beta (Future)
- [ ] Network connectivity infrastructure
- [ ] LDAP client library integration
- [ ] LDAPAuthProvider full implementation
- [ ] Active Directory integration
- [ ] Group synchronization logic
- [ ] Connection pooling
- [ ] Health monitoring and failover
- [ ] Configuration management
- [ ] Audit logging for auth events

### 🔮 Post-Beta
- [ ] OAuth2Provider
- [ ] SAMLProvider
- [ ] Kerberos integration
- [ ] Multi-factor authentication (MFA)
- [ ] External script authentication
- [ ] Certificate-based authentication

---

## Testing Strategy

### Alpha Tests (Unit)
- [x] LocalAuthProvider authentication
- [x] BCrypt password verification
- [x] User existence checking
- [x] Group membership resolution
- [x] Factory provider creation

### Beta Tests (Integration)
- [ ] LDAP connection and binding
- [ ] User authentication via LDAP
- [ ] Group synchronization
- [ ] Connection failure handling
- [ ] Timeout handling
- [ ] Certificate verification

### Beta Tests (Performance)
- [ ] Connection pool efficiency
- [ ] Authentication latency
- [ ] Concurrent authentication load
- [ ] Group sync performance
- [ ] Failover timing

---

## Migration Path

### Alpha → Beta Migration

1. **Configuration Update**:
   ```sql
   -- Alpha (local auth)
   -- No configuration needed, uses catalog

   -- Beta (external auth)
   ALTER DATABASE SET auth_provider = 'ldap';
   ALTER DATABASE SET auth_config = '{...}';
   ```

2. **User Migration**:
   ```sql
   -- Map external users to local users
   CREATE EXTERNAL USER MAPPING
     SERVER ldap_server
     FOR alice
     OPTIONS (external_id 'uid=alice,ou=users,dc=example,dc=com');
   ```

3. **Group Synchronization**:
   ```sql
   -- Create group mapping
   CREATE GROUP MAPPING
     LOCAL GROUP developers
     EXTERNAL GROUP 'cn=developers,ou=groups,dc=example,dc=com';
   ```

---

## Performance Considerations

### Alpha (Local Auth)
- **Latency**: ~250ms per authentication (BCrypt cost 12)
- **Throughput**: ~4 authentications/second/core
- **Caching**: Not needed (catalog already cached)

### Beta (External Auth - Projected)
- **Latency**: 50-200ms (network + provider processing)
- **Throughput**: 20-100 authentications/second (with pooling)
- **Caching**:
  - Authentication results: 5-15 minutes TTL
  - Group memberships: 30-60 minutes TTL
  - Connection pooling: 10-50 connections

---

## Conclusion

The external authentication infrastructure is **complete** for Alpha, providing:

1. ✅ Clean interface for all authentication providers
2. ✅ Fully functional local authentication (BCrypt)
3. ✅ Stub implementations for external providers
4. ✅ Factory pattern for provider creation
5. ✅ Comprehensive documentation

**Beta implementation** deferred until network connectivity is available.

---

**Status**: Infrastructure Ready for Beta
**Next Steps**: Proceed with Phase 3.2 (Query Plan Security Integration)
