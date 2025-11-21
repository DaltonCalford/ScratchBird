# Security System Phase 2: Executor Layer IN PROGRESS
## Date: November 10, 2025

## Executive Summary

**STATUS: EXECUTOR DISPATCH COMPLETE, IMPLEMENTATIONS IN PROGRESS**

The executor dispatch layer for Security System Phase 2 is now set up. All 13 security opcodes are routed to their respective executor functions. The function declarations are in place, but the implementations still need to be completed.

## What Was Completed

### 1. Executor Dispatch Handlers ✅
- **File**: `src/sblr/executor.cpp` (lines 938-1003)
- **Added 13 dispatch handlers** in EXTENDED_OPCODE case:
  - All security opcodes properly routed to execute functions
  - Follows existing pattern: `executeXxx(); result = ExecutionResult();`

**Code Added**:
```cpp
// ===== Security Statements (ALPHA Phase 1 - Security System Phase 2) =====
else if (ext_op == static_cast<uint8_t>(Opcode::EXT_CREATE_USER))
{
    executeCreateUser();
    result = ExecutionResult();
}
// ... 12 more handlers
```

### 2. Executor Function Declarations ✅
- **File**: `include/scratchbird/sblr/executor.h` (lines 513-526)
- **Added 13 function declarations**

**Code Added**:
```cpp
// Security Statements (ALPHA Phase 1 - Security System Phase 2)
void executeCreateUser();        // Execute CREATE USER
void executeAlterUser();         // Execute ALTER USER
void executeDropUser();          // Execute DROP USER
void executeCreateRole();        // Execute CREATE ROLE
void executeDropRole();          // Execute DROP ROLE
void executeCreateGroup();       // Execute CREATE GROUP
void executeDropGroup();         // Execute DROP GROUP
void executeGrantPrivilege();    // Execute GRANT privilege
void executeRevokePrivilege();   // Execute REVOKE privilege
void executeGrantRole();         // Execute GRANT role
void executeRevokeRole();        // Execute REVOKE role
void executeSetRole();           // Execute SET ROLE / RESET ROLE
void executeSetSessionAuth();    // Execute SET/RESET SESSION AUTHORIZATION
```

## Implementation Guide for Executor Functions

### General Pattern

Each executor function follows this pattern:

```cpp
void Executor::executeXxx()
{
    // 1. Decode bytecode parameters
    // 2. Get current session/user for permission checks
    // 3. Look up UUIDs from names (if needed)
    // 4. Call catalog manager function
    // 5. Handle errors
}
```

### Required Helpers

Before implementing executors, you'll need these helper functions:

```cpp
// Get current connection context
core::ConnectionContext* getConnectionContext()
{
    // This should be stored in the Executor class
    // and passed in during execution
    return connection_ctx_;
}

// Get current session UUID
core::UuidV7Bytes getCurrentSessionUuid()
{
    auto* conn_ctx = getConnectionContext();
    if (!conn_ctx) {
        error("No active connection context");
    }
    return conn_ctx->session_uuid;
}

// Check if current user is superuser
bool isSuperuser()
{
    auto* conn_ctx = getConnectionContext();
    if (!conn_ctx) {
        error("No active connection context");
    }
    return conn_ctx->is_superuser;
}

// Hash password using bcrypt or similar
std::string hashPassword(const std::string& password)
{
    // TODO: Implement proper password hashing
    // For now, placeholder:
    return "hashed_" + password;
}
```

### 1. executeCreateUser Implementation

```cpp
void Executor::executeCreateUser()
{
    // Decode bytecode
    std::string username = readString();
    uint8_t flags = readByte();
    bool has_password = flags & 0x01;
    bool is_superuser = flags & 0x02;
    std::string password;
    if (has_password)
    {
        password = readString();
    }

    // Security check: Only superusers can create users
    if (!isSuperuser())
    {
        error("Permission denied: only superusers can create users");
    }

    // Hash password if provided
    std::string password_hash;
    if (has_password)
    {
        password_hash = hashPassword(password);
    }

    // Call catalog manager
    core::UuidV7Bytes user_uuid;
    core::ErrorContext err_ctx;
    auto status = db_->catalogManager().createUser(
        username, password_hash, is_superuser, user_uuid, &err_ctx);

    if (!status.ok())
    {
        error("CREATE USER failed: " + status.message());
    }
}
```

### 2. executeAlterUser Implementation

```cpp
void Executor::executeAlterUser()
{
    // Decode bytecode
    std::string username = readString();
    uint8_t flags = readByte();
    bool change_password = flags & 0x01;
    bool change_superuser = flags & 0x02;
    bool is_superuser = flags & 0x04;
    std::string password;
    if (change_password)
    {
        password = readString();
    }

    // Security check: Only superusers can alter users
    if (!isSuperuser())
    {
        error("Permission denied: only superusers can alter users");
    }

    // Look up user UUID by name
    core::UuidV7Bytes user_uuid;
    core::ErrorContext err_ctx;
    auto get_status = db_->catalogManager().getUserByName(
        username, user_uuid, &err_ctx);

    if (!get_status.ok())
    {
        error("User '" + username + "' not found");
    }

    // Prepare updates
    std::string password_hash;
    if (change_password)
    {
        password_hash = hashPassword(password);
    }

    // Call catalog manager update
    auto status = db_->catalogManager().updateUser(
        user_uuid,
        change_password ? &password_hash : nullptr,
        change_superuser ? &is_superuser : nullptr,
        &err_ctx);

    if (!status.ok())
    {
        error("ALTER USER failed: " + status.message());
    }
}
```

### 3. executeDropUser Implementation

```cpp
void Executor::executeDropUser()
{
    // Decode bytecode
    std::string username = readString();
    uint8_t flags = readByte();
    bool if_exists = flags & 0x01;
    bool cascade = flags & 0x02;

    // Security check: Only superusers can drop users
    if (!isSuperuser())
    {
        error("Permission denied: only superusers can drop users");
    }

    // Look up user UUID
    core::UuidV7Bytes user_uuid;
    core::ErrorContext err_ctx;
    auto get_status = db_->catalogManager().getUserByName(
        username, user_uuid, &err_ctx);

    if (!get_status.ok())
    {
        if (if_exists)
        {
            // Silently succeed if IF EXISTS specified
            return;
        }
        error("User '" + username + "' not found");
    }

    // Call catalog manager delete
    auto status = db_->catalogManager().deleteUser(
        user_uuid, cascade, &err_ctx);

    if (!status.ok())
    {
        error("DROP USER failed: " + status.message());
    }
}
```

### 4. executeCreateRole Implementation

```cpp
void Executor::executeCreateRole()
{
    // Decode bytecode
    std::string rolename = readString();

    // Security check: Only superusers can create roles
    if (!isSuperuser())
    {
        error("Permission denied: only superusers can create roles");
    }

    // Call catalog manager
    core::UuidV7Bytes role_uuid;
    core::ErrorContext err_ctx;
    auto status = db_->catalogManager().createRole(
        rolename, role_uuid, &err_ctx);

    if (!status.ok())
    {
        error("CREATE ROLE failed: " + status.message());
    }
}
```

### 5. executeDropRole Implementation

```cpp
void Executor::executeDropRole()
{
    // Decode bytecode
    std::string rolename = readString();
    uint8_t flags = readByte();
    bool if_exists = flags & 0x01;
    bool cascade = flags & 0x02;

    // Security check: Only superusers can drop roles
    if (!isSuperuser())
    {
        error("Permission denied: only superusers can drop roles");
    }

    // Look up role UUID
    core::UuidV7Bytes role_uuid;
    core::ErrorContext err_ctx;
    auto get_status = db_->catalogManager().getRoleByName(
        rolename, role_uuid, &err_ctx);

    if (!get_status.ok())
    {
        if (if_exists)
        {
            return; // Silently succeed
        }
        error("Role '" + rolename + "' not found");
    }

    // Call catalog manager
    auto status = db_->catalogManager().deleteRole(
        role_uuid, cascade, &err_ctx);

    if (!status.ok())
    {
        error("DROP ROLE failed: " + status.message());
    }
}
```

### 6. executeCreateGroup Implementation

```cpp
void Executor::executeCreateGroup()
{
    // Decode bytecode
    std::string groupname = readString();

    // Security check: Only superusers can create groups
    if (!isSuperuser())
    {
        error("Permission denied: only superusers can create groups");
    }

    // Call catalog manager
    core::UuidV7Bytes group_uuid;
    core::ErrorContext err_ctx;
    auto status = db_->catalogManager().createGroup(
        groupname, group_uuid, &err_ctx);

    if (!status.ok())
    {
        error("CREATE GROUP failed: " + status.message());
    }
}
```

### 7. executeDropGroup Implementation

```cpp
void Executor::executeDropGroup()
{
    // Decode bytecode
    std::string groupname = readString();
    uint8_t flags = readByte();
    bool if_exists = flags & 0x01;
    bool cascade = flags & 0x02;

    // Security check: Only superusers can drop groups
    if (!isSuperuser())
    {
        error("Permission denied: only superusers can drop groups");
    }

    // Look up group UUID
    core::UuidV7Bytes group_uuid;
    core::ErrorContext err_ctx;
    auto get_status = db_->catalogManager().getGroupByName(
        groupname, group_uuid, &err_ctx);

    if (!get_status.ok())
    {
        if (if_exists)
        {
            return; // Silently succeed
        }
        error("Group '" + groupname + "' not found");
    }

    // Call catalog manager
    auto status = db_->catalogManager().deleteGroup(
        group_uuid, cascade, &err_ctx);

    if (!status.ok())
    {
        error("DROP GROUP failed: " + status.message());
    }
}
```

### 8. executeGrantPrivilege Implementation

```cpp
void Executor::executeGrantPrivilege()
{
    // Decode bytecode
    uint32_t privileges = readInt32();
    uint8_t object_type_byte = readByte();
    std::string object_name = readString();
    uint8_t grantee_type_byte = readByte();
    std::string grantee_name = readString();
    uint8_t flags = readByte();
    bool with_grant_option = flags & 0x01;

    // Security check: Only superusers can grant privileges
    if (!isSuperuser())
    {
        error("Permission denied: only superusers can grant privileges");
    }

    // Look up object UUID (table, view, sequence, etc.)
    core::UuidV7Bytes object_uuid;
    core::ErrorContext err_ctx;

    // Based on object_type, look up in appropriate catalog
    // For now, assume TABLE (object_type == 0)
    auto get_obj_status = db_->catalogManager().getTableByName(
        object_name, object_uuid, &err_ctx);

    if (!get_obj_status.ok())
    {
        error("Object '" + object_name + "' not found");
    }

    // Look up grantee UUID
    core::UuidV7Bytes grantee_uuid;
    if (grantee_type_byte == 0) // USER
    {
        auto get_grantee = db_->catalogManager().getUserByName(
            grantee_name, grantee_uuid, &err_ctx);
        if (!get_grantee.ok())
        {
            error("User '" + grantee_name + "' not found");
        }
    }
    else if (grantee_type_byte == 1) // ROLE
    {
        auto get_grantee = db_->catalogManager().getRoleByName(
            grantee_name, grantee_uuid, &err_ctx);
        if (!get_grantee.ok())
        {
            error("Role '" + grantee_name + "' not found");
        }
    }
    else if (grantee_type_byte == 2) // GROUP
    {
        auto get_grantee = db_->catalogManager().getGroupByName(
            grantee_name, grantee_uuid, &err_ctx);
        if (!get_grantee.ok())
        {
            error("Group '" + grantee_name + "' not found");
        }
    }
    else if (grantee_type_byte == 3) // PUBLIC
    {
        // PUBLIC has a special UUID (all zeros or special marker)
        std::memset(&grantee_uuid, 0, sizeof(grantee_uuid));
    }

    // Call catalog manager
    auto status = db_->catalogManager().grantPermission(
        object_uuid, grantee_uuid, privileges, with_grant_option, &err_ctx);

    if (!status.ok())
    {
        error("GRANT PRIVILEGE failed: " + status.message());
    }
}
```

### 9. executeRevokePrivilege Implementation

```cpp
void Executor::executeRevokePrivilege()
{
    // Decode bytecode
    uint32_t privileges = readInt32();
    uint8_t object_type_byte = readByte();
    std::string object_name = readString();
    uint8_t grantee_type_byte = readByte();
    std::string grantee_name = readString();
    uint8_t flags = readByte();
    bool cascade = flags & 0x01;

    // Security check: Only superusers can revoke privileges
    if (!isSuperuser())
    {
        error("Permission denied: only superusers can revoke privileges");
    }

    // Look up object and grantee UUIDs (similar to GRANT)
    core::UuidV7Bytes object_uuid, grantee_uuid;
    core::ErrorContext err_ctx;

    // [Similar UUID lookups as in executeGrantPrivilege]
    // ...

    // Call catalog manager
    auto status = db_->catalogManager().revokePermission(
        object_uuid, grantee_uuid, privileges, cascade, &err_ctx);

    if (!status.ok())
    {
        error("REVOKE PRIVILEGE failed: " + status.message());
    }
}
```

### 10. executeGrantRole Implementation

```cpp
void Executor::executeGrantRole()
{
    // Decode bytecode
    std::string rolename = readString();
    uint8_t grantee_type_byte = readByte();
    std::string grantee_name = readString();

    // Security check: Only superusers can grant roles
    if (!isSuperuser())
    {
        error("Permission denied: only superusers can grant roles");
    }

    // Look up role and grantee UUIDs
    core::UuidV7Bytes role_uuid, grantee_uuid;
    core::ErrorContext err_ctx;

    auto get_role = db_->catalogManager().getRoleByName(
        rolename, role_uuid, &err_ctx);
    if (!get_role.ok())
    {
        error("Role '" + rolename + "' not found");
    }

    if (grantee_type_byte == 0) // USER
    {
        auto get_grantee = db_->catalogManager().getUserByName(
            grantee_name, grantee_uuid, &err_ctx);
        if (!get_grantee.ok())
        {
            error("User '" + grantee_name + "' not found");
        }

        // Call catalog manager
        auto status = db_->catalogManager().grantRoleToUser(
            role_uuid, grantee_uuid, &err_ctx);

        if (!status.ok())
        {
            error("GRANT ROLE failed: " + status.message());
        }
    }
    else // ROLE
    {
        auto get_grantee = db_->catalogManager().getRoleByName(
            grantee_name, grantee_uuid, &err_ctx);
        if (!get_grantee.ok())
        {
            error("Role '" + grantee_name + "' not found");
        }

        // Call catalog manager for role-to-role grant
        auto status = db_->catalogManager().grantRoleToRole(
            role_uuid, grantee_uuid, &err_ctx);

        if (!status.ok())
        {
            error("GRANT ROLE failed: " + status.message());
        }
    }
}
```

### 11. executeRevokeRole Implementation

```cpp
void Executor::executeRevokeRole()
{
    // Similar to executeGrantRole but calls revokeRoleFromUser/Role
    // and handles CASCADE flag
    // [Implementation similar to GRANT ROLE]
}
```

### 12. executeSetRole Implementation

```cpp
void Executor::executeSetRole()
{
    // Decode bytecode
    uint8_t flags = readByte();
    bool is_reset = flags & 0x01;

    core::ErrorContext err_ctx;
    auto session_uuid = getCurrentSessionUuid();

    if (is_reset)
    {
        // RESET ROLE: Clear active role
        auto status = db_->catalogManager().updateSession(
            session_uuid, nullptr, &err_ctx); // nullptr = clear role

        if (!status.ok())
        {
            error("RESET ROLE failed: " + status.message());
        }
    }
    else
    {
        // SET ROLE: Set active role
        std::string rolename = readString();

        // Look up role UUID
        core::UuidV7Bytes role_uuid;
        auto get_role = db_->catalogManager().getRoleByName(
            rolename, role_uuid, &err_ctx);

        if (!get_role.ok())
        {
            error("Role '" + rolename + "' not found");
        }

        // Update session
        auto status = db_->catalogManager().updateSession(
            session_uuid, &role_uuid, &err_ctx);

        if (!status.ok())
        {
            error("SET ROLE failed: " + status.message());
        }
    }
}
```

### 13. executeSetSessionAuth Implementation

```cpp
void Executor::executeSetSessionAuth()
{
    // Decode bytecode
    uint8_t flags = readByte();
    bool is_reset = flags & 0x01;

    // Security check: Only superusers can change session authorization
    if (!isSuperuser())
    {
        error("Permission denied: only superusers can change session authorization");
    }

    core::ErrorContext err_ctx;
    auto session_uuid = getCurrentSessionUuid();

    if (is_reset)
    {
        // RESET SESSION AUTHORIZATION: Restore to connection user
        // This requires storing original user_uuid in connection context
        auto status = db_->catalogManager().resetSessionUser(
            session_uuid, &err_ctx);

        if (!status.ok())
        {
            error("RESET SESSION AUTHORIZATION failed: " + status.message());
        }
    }
    else
    {
        // SET SESSION AUTHORIZATION: Change session user
        std::string username = readString();

        // Look up user UUID
        core::UuidV7Bytes user_uuid;
        auto get_user = db_->catalogManager().getUserByName(
            username, user_uuid, &err_ctx);

        if (!get_user.ok())
        {
            error("User '" + username + "' not found");
        }

        // Update session
        auto status = db_->catalogManager().setSessionUser(
            session_uuid, user_uuid, &err_ctx);

        if (!status.ok())
        {
            error("SET SESSION AUTHORIZATION failed: " + status.message());
        }
    }
}
```

## Compilation Status

### Header Declarations ✅
```bash
# All function declarations added to executor.h
Lines 513-526: 13 security executor function declarations
```

### Dispatch Handlers ✅
```bash
# All opcode dispatches added to executor.cpp
Lines 938-1003: 13 security opcode handlers in EXTENDED_OPCODE case
```

### Function Implementations ❌
```bash
# Implementations still need to be added after line 12380
# Should be added before closing namespace braces (line 12382-12383)
```

## Next Steps

### Immediate Tasks

1. **Add Helper Functions** (executor.cpp):
   - `getConnectionContext()`
   - `getCurrentSessionUuid()`
   - `isSuperuser()`
   - `hashPassword()`

2. **Implement 13 Executor Functions** (executor.cpp, ~400-500 lines):
   - Add after line 12380 (after executeJumpIfFalse)
   - Before closing namespaces (line 12382-12383)
   - Use implementation templates provided above

3. **Add ConnectionContext to Executor**:
   - Modify Executor constructor to accept ConnectionContext
   - Store as member variable: `core::ConnectionContext* connection_ctx_`
   - Use for session/user tracking

4. **Compile and Test**:
   - Fix any compilation errors
   - Test basic security statements
   - Verify catalog manager integration

### Future Work

1. **Permission Check Hooks** (10-15 hours):
   - Add to executeSelect, executeInsert, executeUpdate, executeDelete
   - Add to executeCreateTable, executeDropTable, executeAlterTable
   - Check permissions before operation

2. **Enhanced Error Handling**:
   - Better error messages
   - Rollback on failure
   - Audit logging

3. **CASCADE Implementation**:
   - Implement recursive deletion for DROP CASCADE
   - Implement transitive revoke for REVOKE CASCADE

## Files Modified Summary

### Headers (1 file)
1. `include/scratchbird/sblr/executor.h` - Added 13 function declarations

### Implementations (1 file)
1. `src/sblr/executor.cpp` - Added 13 dispatch handlers (implementations pending)

## Estimated Work Remaining

| Task | Estimated Time |
|------|----------------|
| Helper functions | 1-2 hours |
| 13 executor implementations | 8-12 hours |
| Connection context integration | 2-3 hours |
| Testing & debugging | 5-8 hours |
| Permission check hooks | 10-15 hours |
| CASCADE logic | 5-8 hours |
| **TOTAL** | **31-48 hours** |

## Conclusion

The executor dispatch layer is **complete** and all function declarations are in place. The implementations follow a clear pattern and have detailed templates provided. With the catalog manager functions already implemented in Phase 1, the executor layer primarily involves decoding bytecode, looking up UUIDs, and calling the appropriate catalog manager functions.

---

**Executor Dispatch Complete**: November 10, 2025
**Function Implementations**: In Progress (templates provided)
**Total Phase 2 Progress**: ~40% complete (parser + bytecode + dispatch done)
