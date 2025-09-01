# Phase 17: Authentication and Security

## Objective
Implement user authentication and basic security.

## Prerequisites
- Phase 16 complete (WAL/recovery)

## Tasks

### 17.1 User Management
```sql
CREATE USER username WITH PASSWORD 'password';
ALTER USER username WITH PASSWORD 'newpass';
DROP USER username;
```

### 17.2 Authentication Methods
```cpp
enum AuthMethod {
    Password,
    Trust,
    MD5,
    SCRAM_SHA_256
};
```

### 17.3 Password Storage
- Never store plaintext
- Use bcrypt or PBKDF2
- Salt all passwords
- Support password policies

### 17.4 Session Management
```cpp
struct Session {
    uint32_t session_id;
    string username;
    time_t login_time;
    IsolationLevel isolation;
    map<string, string> variables;
};
```

### 17.5 Connection Security
- Support TLS/SSL connections
- Certificate validation
- Encrypted password transmission

## Files to Create/Modify
- `include/scratchbird/auth.h`
- `src/engine/auth_manager.cpp`
- `src/engine/password_auth.cpp`

## Validation Tests
```cpp
// Create user
execute("CREATE USER testuser WITH PASSWORD 'secret123'");

// Successful authentication
auto session = connect("testuser", "secret123");
assert(session != nullptr);

// Failed authentication
auto session2 = connect("testuser", "wrongpass");
assert(session2 == nullptr);

// Password change
execute("ALTER USER testuser WITH PASSWORD 'newsecret'");
session = connect("testuser", "secret123");
assert(session == nullptr);  // Old password fails
session = connect("testuser", "newsecret");
assert(session != nullptr);  // New password works

// Verify password hashing
auto hash = get_password_hash("testuser");
assert(!hash.contains("newsecret"));  // Not plaintext
assert(hash.starts_with("$2b$"));      // bcrypt format
```

## Exit Criteria
- Users can be created/modified/deleted
- Passwords properly hashed
- Authentication works correctly
- Sessions tracked properly