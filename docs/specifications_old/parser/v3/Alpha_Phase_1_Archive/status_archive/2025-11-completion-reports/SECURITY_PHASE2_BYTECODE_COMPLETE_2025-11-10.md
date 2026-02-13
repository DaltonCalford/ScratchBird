# Security System Phase 2: Bytecode Layer COMPLETE

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

## Date: November 10, 2025

## Executive Summary

**STATUS: BYTECODE LAYER 100% COMPLETE AND COMPILES SUCCESSFULLY**

The bytecode generation layer for Security System Phase 2 is now fully implemented and compiling. All 13 security SQL statements can be compiled to bytecode using the new extended opcodes. The parser and SBLR libraries both compile successfully.

## What Was Completed

### 1. Bytecode Opcodes ✅
- **File**: `include/scratchbird/sblr/opcodes.h` (lines 461-486)
- **Added 13 Extended Opcodes** (0xCA-0xD6 range):
  - `EXT_CREATE_USER` (0xCA)
  - `EXT_ALTER_USER` (0xCB)
  - `EXT_DROP_USER` (0xCC)
  - `EXT_CREATE_ROLE` (0xCD)
  - `EXT_DROP_ROLE` (0xCE)
  - `EXT_CREATE_GROUP` (0xCF)
  - `EXT_DROP_GROUP` (0xD0)
  - `EXT_GRANT_PRIVILEGE` (0xD1)
  - `EXT_REVOKE_PRIVILEGE` (0xD2)
  - `EXT_GRANT_ROLE` (0xD3)
  - `EXT_REVOKE_ROLE` (0xD4)
  - `EXT_SET_ROLE` (0xD5)
  - `EXT_SET_SESSION_AUTH` (0xD6)

**Opcode Design**:
- All security opcodes use extended encoding: `EXTENDED_OPCODE (0xFF)` + `EXT_xxx` byte
- This keeps main opcode space (0x00-0xFF) available for core operations
- Extended opcodes can use up to 256 values (0x00-0xFF)

###2. Bytecode Generation Implementation ✅
- **File**: `src/sblr/bytecode_generator.cpp` (lines 2121-2403, ~280 lines)
- **Implemented 13 BytecodeGenerator Visitor Methods**

#### CREATE USER Bytecode Format
```
Byte 0-1: EXTENDED_OPCODE (0xFF) + EXT_CREATE_USER (0xCA)
Byte 2-5: username (StringPool ID, uint32_t)
Byte 6:   flags (bit 0 = has_password, bit 1 = is_superuser)
Byte 7-10: password (StringPool ID, uint32_t) - only if has_password flag set
```

#### ALTER USER Bytecode Format
```
Byte 0-1: EXTENDED_OPCODE (0xFF) + EXT_ALTER_USER (0xCB)
Byte 2-5: username (StringPool ID, uint32_t)
Byte 6:   flags (bit 0 = change_password, bit 1 = change_superuser, bit 2 = is_superuser)
Byte 7-10: password (StringPool ID, uint32_t) - only if change_password flag set
```

#### DROP USER Bytecode Format
```
Byte 0-1: EXTENDED_OPCODE (0xFF) + EXT_DROP_USER (0xCC)
Byte 2-5: username (StringPool ID, uint32_t)
Byte 6:   flags (bit 0 = if_exists, bit 1 = cascade)
```

#### CREATE ROLE Bytecode Format
```
Byte 0-1: EXTENDED_OPCODE (0xFF) + EXT_CREATE_ROLE (0xCD)
Byte 2-5: rolename (StringPool ID, uint32_t)
```

#### DROP ROLE Bytecode Format
```
Byte 0-1: EXTENDED_OPCODE (0xFF) + EXT_DROP_ROLE (0xCE)
Byte 2-5: rolename (StringPool ID, uint32_t)
Byte 6:   flags (bit 0 = if_exists, bit 1 = cascade)
```

#### CREATE GROUP Bytecode Format
```
Byte 0-1: EXTENDED_OPCODE (0xFF) + EXT_CREATE_GROUP (0xCF)
Byte 2-5: groupname (StringPool ID, uint32_t)
```

#### DROP GROUP Bytecode Format
```
Byte 0-1: EXTENDED_OPCODE (0xFF) + EXT_DROP_GROUP (0xD0)
Byte 2-5: groupname (StringPool ID, uint32_t)
Byte 6:   flags (bit 0 = if_exists, bit 1 = cascade)
```

#### GRANT PRIVILEGE Bytecode Format
```
Byte 0-1:   EXTENDED_OPCODE (0xFF) + EXT_GRANT_PRIVILEGE (0xD1)
Byte 2-5:   privilege bitmask (uint32_t)
Byte 6:     object type (uint8_t enum: TABLE=0, VIEW=1, SEQUENCE=2, etc.)
Byte 7-10:  object name (StringPool ID, uint32_t)
Byte 11:    grantee type (uint8_t enum: USER=0, ROLE=1, GROUP=2, PUBLIC=3)
Byte 12-15: grantee name (StringPool ID, uint32_t, 0 if PUBLIC)
Byte 16:    flags (bit 0 = with_grant_option)
```

#### REVOKE PRIVILEGE Bytecode Format
```
Byte 0-1:   EXTENDED_OPCODE (0xFF) + EXT_REVOKE_PRIVILEGE (0xD2)
Byte 2-5:   privilege bitmask (uint32_t)
Byte 6:     object type (uint8_t enum)
Byte 7-10:  object name (StringPool ID, uint32_t)
Byte 11:    grantee type (uint8_t enum)
Byte 12-15: grantee name (StringPool ID, uint32_t)
Byte 16:    flags (bit 0 = cascade)
```

#### GRANT ROLE Bytecode Format
```
Byte 0-1:   EXTENDED_OPCODE (0xFF) + EXT_GRANT_ROLE (0xD3)
Byte 2-5:   rolename (StringPool ID, uint32_t)
Byte 6:     grantee type (uint8_t enum: USER=0, ROLE=1)
Byte 7-10:  grantee name (StringPool ID, uint32_t)
```

#### REVOKE ROLE Bytecode Format
```
Byte 0-1:   EXTENDED_OPCODE (0xFF) + EXT_REVOKE_ROLE (0xD4)
Byte 2-5:   rolename (StringPool ID, uint32_t)
Byte 6:     grantee type (uint8_t enum)
Byte 7-10:  grantee name (StringPool ID, uint32_t)
Byte 11:    flags (bit 0 = cascade)
```

#### SET ROLE Bytecode Format
```
Byte 0-1: EXTENDED_OPCODE (0xFF) + EXT_SET_ROLE (0xD5)
Byte 2:   flags (bit 0 = is_reset)
Byte 3-6: rolename (StringPool ID, uint32_t) - only if not reset
```

#### SET SESSION AUTHORIZATION Bytecode Format
```
Byte 0-1: EXTENDED_OPCODE (0xFF) + EXT_SET_SESSION_AUTH (0xD6)
Byte 2:   flags (bit 0 = is_reset)
Byte 3-6: username (StringPool ID, uint32_t) - only if not reset
```

## Compilation Results

### Parser Library ✅
```bash
cmake --build build --target scratchbird_parser
[100%] Built target scratchbird_parser
```
**Result**: SUCCESS - No errors

### SBLR Library ✅
```bash
cmake --build build --target scratchbird_sblr
[100%] Built target scratchbird_sblr
```
**Result**: SUCCESS - Only pre-existing warnings (unrelated to security work)

## Code Statistics

| Component | Files Modified | Lines Added | Functionality |
|-----------|----------------|-------------|---------------|
| Opcodes | 1 | 26 | 13 extended opcodes |
| BytecodeGenerator | 1 | 280 | 13 visitor implementations |
| **TOTAL** | **2** | **~306** | **Full bytecode layer** |

## Bytecode Design Decisions

### 1. Extended Opcode Encoding
**Why**: Main opcode space (0x00-0xFF) is valuable and should be reserved for frequently-used core operations. Security statements are less frequent.

**How**: All security opcodes use the EXTENDED_OPCODE prefix (0xFF) followed by a secondary opcode byte.

**Benefit**: Allows for 256 extended opcodes without consuming main opcode space.

### 2. Flag Byte Compression
**Why**: Many security statements have optional boolean flags (IF EXISTS, CASCADE, WITH GRANT OPTION, etc.)

**How**: Pack multiple boolean flags into a single uint8_t flags byte using bit positions.

**Benefit**: Compact bytecode representation, easy to extend with more flags if needed.

**Example**: DROP USER flags byte
- Bit 0: if_exists
- Bit 1: cascade
- Bits 2-7: Reserved for future flags

### 3. StringPool ID Encoding
**Why**: Usernames, role names, object names are strings that may be repeated.

**How**: Parser interns strings in StringPool and assigns each a unique uint32_t ID. Bytecode stores IDs, not raw strings.

**Benefit**:
- Compact encoding (4 bytes vs variable-length strings)
- String deduplication
- Fast lookups during execution

### 4. Enum Type Encoding
**Why**: Object types, grantee types, privilege types are small enumerations.

**How**: Cast enum values to uint8_t for serialization.

**Benefit**: Type-safe in C++ code, compact in bytecode (1 byte per enum).

### 5. Privilege Bitmask
**Why**: GRANT/REVOKE can specify multiple privileges (e.g., SELECT, INSERT, UPDATE).

**How**: Use uint32_t bitmask where each bit represents one privilege type.

**Benefit**:
- Compact representation (4 bytes for up to 32 privilege types)
- Fast set operations (OR for grant, AND NOT for revoke)
- Matches Phase 1 catalog manager API exactly

**Privilege Bits**:
```
Bit 0:  SELECT
Bit 1:  INSERT
Bit 2:  UPDATE
Bit 3:  DELETE
Bit 4:  EXECUTE
Bit 5:  USAGE
Bit 6:  CREATE
Bit 7:  CONNECT
Bit 8:  TEMPORARY
Bit 9:  TRIGGER
Bit 10: REFERENCES
Bits 11-31: Reserved
```

## Implementation Challenges & Solutions

### Challenge 1: AST Method Name Inconsistencies
**Problem**: Initial bytecode implementation used incorrect method names:
- Used `roleName()` when AST uses `rolename()`
- Used `cascade()` when AST uses `dropBehavior()` returning enum
- Used `hasPassword()` when AlterUserStmt uses `changePassword()`

**Solution**: Carefully read AST node definitions to verify exact method names and return types.

**Code Example**:
```cpp
// WRONG:
if (node->cascade())

// RIGHT:
if (node->dropBehavior() == parser::DropUserStmt::DropBehavior::CASCADE)
```

### Challenge 2: ALTER USER Complexity
**Problem**: ALTER USER can change password, superuser status, or both. Needs to track what changed and the new values.

**Solution**: Use 3 flag bits:
- Bit 0: change_password
- Bit 1: change_superuser
- Bit 2: is_superuser (new value)

Only serialize password if change_password is true.

**Bytecode Size**: 7-11 bytes depending on what's being changed.

### Challenge 3: GRANT/REVOKE Disambiguation
**Problem**: GRANT and REVOKE can be used for privileges OR roles, but bytecode needs separate opcodes.

**Solution**: Parser already disambiguates into separate AST nodes (GrantPrivilegeStmt vs GrantRoleStmt). BytecodeGenerator just emits appropriate opcode for each node type.

## Testing Strategy

### Unit Tests (Recommended)
1. **Opcode Encoding Tests**: Verify each statement emits correct opcode sequence
2. **Flag Bit Tests**: Verify all flag combinations encode/decode correctly
3. **StringPool ID Tests**: Verify StringPool IDs serialize correctly
4. **Round-Trip Tests**: Parse → BytecodeGenerate → BytecodeRead → Compare

### Integration Tests (Recommended)
1. **End-to-End**: Parse → Bytecode → Execute → Verify catalog changes
2. **Error Cases**: Invalid privileges, non-existent users, CASCADE failures
3. **Permission Tests**: Verify only authorized users can execute security statements

## Bytecode Size Analysis

| Statement | Min Bytes | Max Bytes | Average | Notes |
|-----------|-----------|-----------|---------|-------|
| CREATE USER | 7 | 11 | 9 | +4 if password provided |
| ALTER USER | 7 | 11 | 9 | +4 if changing password |
| DROP USER | 7 | 7 | 7 | Fixed size |
| CREATE ROLE | 6 | 6 | 6 | Fixed size |
| DROP ROLE | 7 | 7 | 7 | Fixed size |
| CREATE GROUP | 6 | 6 | 6 | Fixed size |
| DROP GROUP | 7 | 7 | 7 | Fixed size |
| GRANT PRIVILEGE | 17 | 17 | 17 | Fixed size |
| REVOKE PRIVILEGE | 17 | 17 | 17 | Fixed size |
| GRANT ROLE | 11 | 11 | 11 | Fixed size |
| REVOKE ROLE | 12 | 12 | 12 | Fixed size |
| SET ROLE | 3 | 7 | 5 | +4 if not reset |
| SET SESSION AUTH | 3 | 7 | 5 | +4 if not reset |

**Total Average**: ~9.5 bytes per security statement (very compact!)

## Integration with Phase 1

The bytecode layer is designed to integrate seamlessly with Phase 1 catalog manager:

| Bytecode | Phase 1 Function | Mapping |
|----------|------------------|---------|
| EXT_CREATE_USER | catalog_manager.createUser() | username, password_hash, is_superuser |
| EXT_ALTER_USER | catalog_manager.updateUser() | user_uuid, new password/superuser |
| EXT_DROP_USER | catalog_manager.deleteUser() | user_uuid, cascade flag |
| EXT_CREATE_ROLE | catalog_manager.createRole() | rolename |
| EXT_DROP_ROLE | catalog_manager.deleteRole() | role_uuid, cascade flag |
| EXT_CREATE_GROUP | catalog_manager.createGroup() | groupname |
| EXT_DROP_GROUP | catalog_manager.deleteGroup() | group_uuid, cascade flag |
| EXT_GRANT_PRIVILEGE | catalog_manager.grantPermission() | object_uuid, grantee_uuid, privileges |
| EXT_REVOKE_PRIVILEGE | catalog_manager.revokePermission() | object_uuid, grantee_uuid, privileges |
| EXT_GRANT_ROLE | catalog_manager.grantRoleToUser() | role_uuid, user_uuid |
| EXT_REVOKE_ROLE | catalog_manager.revokeRoleFromUser() | role_uuid, user_uuid |
| EXT_SET_ROLE | catalog_manager.createSession() | Sets active_role_uuid in session |
| EXT_SET_SESSION_AUTH | catalog_manager.createSession() | Changes session user |

## Remaining Work (Next Steps)

### 1. Executor Implementation (20-30 hours)
- **File**: `src/sblr/executor.cpp`
- **Add 13 executor handlers** for security opcodes
- Each handler:
  1. Decode bytecode parameters
  2. Look up UUIDs from names (username → user_uuid)
  3. Call appropriate catalog_manager function
  4. Handle errors (user not found, permission denied, etc.)
  5. Return execution result

**Example Executor Pseudocode**:
```cpp
case Opcode::EXT_CREATE_USER:
{
    // Decode bytecode
    StringPool::StringId username_id = readInt32(bytecode);
    uint8_t flags = readByte(bytecode);
    bool has_password = flags & 0x01;
    bool is_superuser = flags & 0x02;
    StringPool::StringId password_id = has_password ? readInt32(bytecode) : 0;

    // Get strings from pool
    std::string username = string_pool.get(username_id);
    std::string password = has_password ? string_pool.get(password_id) : "";

    // Hash password
    std::string password_hash = has_password ? hashPassword(password) : "";

    // Call catalog manager
    UuidV7Bytes user_uuid;
    Status status = catalog_manager->createUser(
        username, password_hash, is_superuser, user_uuid, error_ctx);

    // Handle errors
    if (!status.ok()) {
        return ExecutionError(status.message());
    }

    return ExecutionSuccess();
}
```

### 2. Permission Check Hooks (10-15 hours)
- **Files**: All DML/DDL executors
- **Before executeSelect()**: Check SELECT permission on table
- **Before executeInsert()**: Check INSERT permission on table
- **Before executeUpdate()**: Check UPDATE permission on table
- **Before executeDelete()**: Check DELETE permission on table
- **Before executeCreateTable()**: Check CREATE permission on schema
- **Before executeDropTable()**: Check DROP permission on table
- **Before executeAlterTable()**: Check ALTER permission on table

**Permission Check Pattern**:
```cpp
Status executeSelect(SelectStmt* stmt) {
    // Get current session
    Session* session = getCurrentSession();

    // Look up table UUID
    UuidV7Bytes table_uuid = lookupTable(stmt->tableName());

    // Check SELECT permission
    bool has_permission = catalog_manager->hasPermission(
        session->user_uuid,
        table_uuid,
        PermissionType::SELECT,
        error_ctx);

    if (!has_permission) {
        return Status::PermissionDenied(
            "User does not have SELECT permission on table");
    }

    // Proceed with execution
    ...
}
```

### 3. Superuser Checks (2-3 hours)
- Only superusers can execute security DDL (CREATE/ALTER/DROP USER/ROLE/GROUP)
- Add superuser check at start of each security executor:
```cpp
if (!getCurrentSession()->is_superuser) {
    return Status::PermissionDenied("Only superusers can create users");
}
```

### 4. CASCADE Implementation (5-8 hours)
- When DROP USER/ROLE/GROUP with CASCADE:
  - Delete all dependent objects (roles, permissions, group memberships)
  - Transfer ownership of owned objects to SYSTEM user or drop them
- When REVOKE with CASCADE:
  - Recursively revoke permissions granted by this grantee
  - Revoke roles from users who got them transitively

## Files Modified Summary

### Headers (1 file)
1. `include/scratchbird/sblr/opcodes.h` - Added 13 extended opcodes

### Implementations (1 file)
1. `src/sblr/bytecode_generator.cpp` - Implemented 13 bytecode generators

## Conclusion

The bytecode generation layer for Security System Phase 2 is **100% complete** and **compiles successfully**. All 13 security SQL statements are now compiled to compact, well-designed bytecode ready for execution.

**Compilation verified**: Both `scratchbird_parser` and `scratchbird_sblr` libraries build without errors.

**Bytecode design**: Compact, extensible, and directly maps to Phase 1 catalog manager API.

**Next recommended step**: Implement executor handlers for security opcodes (Tasks 9-10 in todo list).

---

**Bytecode Layer Complete**: November 10, 2025
**Estimated Next Layer Time**: 20-30 hours (executor implementation + permission hooks)
**Total Phase 2 Progress**: ~35% complete (parser + bytecode done, executor/hooks remain)
