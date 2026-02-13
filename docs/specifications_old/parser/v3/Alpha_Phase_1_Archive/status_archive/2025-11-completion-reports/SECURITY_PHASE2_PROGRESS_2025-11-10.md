# Security System Phase 2 - SQL Parser Integration Progress

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: November 10, 2025
**Status**: IN PROGRESS - Foundation Complete
**Estimated Remaining**: 50-70 hours

---

## Executive Summary

Security System Phase 2 implementation has begun with the foundational work completed. The lexer and AST layer are now ready to support security SQL statements. Parser implementation, bytecode generation, executor integration, and permission hooks remain to be completed.

---

## What's Complete ✅

### 1. Lexer Keywords (100% Complete)

**Files Modified**:
- `include/scratchbird/parser/token.h` - Added 16 new token types
- `src/parser/lexer.cpp` - Added 16 keyword mappings

**Keywords Added**:
```cpp
KW_USER          // CREATE USER, ALTER USER, DROP USER
KW_ROLE          // CREATE ROLE, DROP ROLE, SET ROLE, GRANT ROLE
KW_GROUP         // CREATE GROUP, DROP GROUP
KW_GRANT         // GRANT privilege/role
KW_REVOKE        // REVOKE privilege/role
KW_PRIVILEGES    // GRANT SELECT, INSERT, ... PRIVILEGES ON
KW_PASSWORD      // CREATE USER ... WITH PASSWORD
KW_SUPERUSER     // CREATE USER ... SUPERUSER
KW_NOSUPERUSER   // CREATE USER ... NOSUPERUSER
KW_SESSION       // SET SESSION AUTHORIZATION
KW_AUTHORIZATION // SET SESSION AUTHORIZATION
KW_RESET         // RESET ROLE, RESET SESSION AUTHORIZATION
KW_PUBLIC        // GRANT ... TO PUBLIC
KW_USAGE         // GRANT USAGE ON SCHEMA
KW_CONNECT       // GRANT CONNECT ON DATABASE
KW_REFERENCES    // GRANT REFERENCES ON TABLE
```

### 2. AST Node Structures (100% Complete)

**Files Modified**:
- `include/scratchbird/parser/ast.h` - Added 13 new AST kinds, 13 statement classes, 26 visitor methods

**AST Kinds Added** (lines 65-78):
```cpp
CREATE_USER       // CREATE USER username [WITH PASSWORD 'xxx'] [SUPERUSER]
ALTER_USER        // ALTER USER username [WITH PASSWORD 'xxx'] [SUPERUSER]
DROP_USER         // DROP USER username [IF EXISTS] [CASCADE | RESTRICT]
CREATE_ROLE       // CREATE ROLE rolename
DROP_ROLE         // DROP ROLE rolename [IF EXISTS] [CASCADE | RESTRICT]
CREATE_GROUP      // CREATE GROUP groupname
DROP_GROUP        // DROP GROUP groupname [IF EXISTS] [CASCADE | RESTRICT]
GRANT_PRIVILEGE   // GRANT privilege ON object TO grantee [WITH GRANT OPTION]
REVOKE_PRIVILEGE  // REVOKE privilege ON object FROM grantee [CASCADE | RESTRICT]
GRANT_ROLE        // GRANT role TO user/role
REVOKE_ROLE       // REVOKE role FROM user/role [CASCADE | RESTRICT]
SET_ROLE          // SET ROLE rolename / RESET ROLE
SET_SESSION_AUTH  // SET SESSION AUTHORIZATION username / RESET SESSION AUTHORIZATION
```

**Statement Classes Added** (lines 2819-3264):

1. **CreateUserStmt** (lines 2821-2850)
   - Properties: username, password, has_password, is_superuser
   - SQL: `CREATE USER alice WITH PASSWORD 'secret' SUPERUSER`

2. **AlterUserStmt** (lines 2852-2885)
   - Properties: username, password, change_password, is_superuser, change_superuser
   - SQL: `ALTER USER alice WITH PASSWORD 'newsecret' NOSUPERUSER`

3. **DropUserStmt** (lines 2887-2918)
   - Properties: username, if_exists, drop_behavior (RESTRICT/CASCADE)
   - SQL: `DROP USER alice IF EXISTS CASCADE`

4. **CreateRoleStmt** (lines 2920-2937)
   - Properties: rolename
   - SQL: `CREATE ROLE admin`

5. **DropRoleStmt** (lines 2939-2970)
   - Properties: rolename, if_exists, drop_behavior
   - SQL: `DROP ROLE admin IF EXISTS CASCADE`

6. **CreateGroupStmt** (lines 2972-2989)
   - Properties: groupname
   - SQL: `CREATE GROUP developers`

7. **DropGroupStmt** (lines 2991-3022)
   - Properties: groupname, if_exists, drop_behavior
   - SQL: `DROP GROUP developers IF EXISTS CASCADE`

8. **GrantPrivilegeStmt** (lines 3024-3097)
   - Properties: privileges (bitmask), object_type, object_name, grantee_type, grantee_name, with_grant_option
   - Enums: PrivilegeType (13 values), ObjectType (8 values), GranteeType (4 values)
   - SQL: `GRANT SELECT, INSERT ON TABLE employees TO alice WITH GRANT OPTION`

9. **RevokePrivilegeStmt** (lines 3099-3146)
   - Properties: privileges, object_type, object_name, grantee_type, grantee_name, revoke_behavior
   - SQL: `REVOKE SELECT ON TABLE employees FROM alice CASCADE`

10. **GrantRoleStmt** (lines 3148-3179)
    - Properties: rolename, grantee_type (USER/ROLE), grantee_name
    - SQL: `GRANT admin TO alice`

11. **RevokeRoleStmt** (lines 3181-3218)
    - Properties: rolename, grantee_type, grantee_name, revoke_behavior
    - SQL: `REVOKE admin FROM alice CASCADE`

12. **SetRoleStmt** (lines 3220-3241)
    - Properties: rolename, is_reset
    - SQL: `SET ROLE admin` or `RESET ROLE`

13. **SetSessionAuthStmt** (lines 3243-3264)
    - Properties: username, is_reset
    - SQL: `SET SESSION AUTHORIZATION alice` or `RESET SESSION AUTHORIZATION`

**Visitor Methods Added**:
- ASTVisitor pure virtual methods (lines 3318-3331): 13 methods
- ASTPrinter concrete methods (lines 3413-3426): 13 methods

---

## What Remains ❌

### 3. Parser Implementation (Estimated: 20-25 hours)

**Files to Modify**:
- `src/parser/parser.cpp`
- `include/scratchbird/parser/parser.h`

**Functions to Implement**:

```cpp
// In Parser class
std::unique_ptr<Statement> parseCreateUser();
std::unique_ptr<Statement> parseAlterUser();
std::unique_ptr<Statement> parseDropUser();
std::unique_ptr<Statement> parseCreateRole();
std::unique_ptr<Statement> parseDropRole();
std::unique_ptr<Statement> parseCreateGroup();
std::unique_ptr<Statement> parseDropGroup();
std::unique_ptr<Statement> parseGrant();        // Handles GRANT privilege and GRANT role
std::unique_ptr<Statement> parseRevoke();       // Handles REVOKE privilege and REVOKE role
std::unique_ptr<Statement> parseSetRole();
std::unique_ptr<Statement> parseSetSessionAuth();

// Helper functions
uint32_t parsePrivilegeList();  // Parses SELECT, INSERT, UPDATE, ...
GrantPrivilegeStmt::ObjectType parseObjectType();  // Parses TABLE, VIEW, SEQUENCE, ...
GrantPrivilegeStmt::GranteeType parseGranteeType(); // Parses USER, ROLE, GROUP, PUBLIC
```

**Parser Entry Point Modification**:
```cpp
// In Parser::parseStatement(), add cases for:
case TokenType::KW_CREATE:
    // ... existing cases
    if (peek(1).type == TokenType::KW_USER) return parseCreateUser();
    if (peek(1).type == TokenType::KW_ROLE) return parseCreateRole();
    if (peek(1).type == TokenType::KW_GROUP) return parseCreateGroup();

case TokenType::KW_ALTER:
    // ... existing cases
    if (peek(1).type == TokenType::KW_USER) return parseAlterUser();

case TokenType::KW_DROP:
    // ... existing cases
    if (peek(1).type == TokenType::KW_USER) return parseDropUser();
    if (peek(1).type == TokenType::KW_ROLE) return parseDropRole();
    if (peek(1).type == TokenType::KW_GROUP) return parseDropGroup();

case TokenType::KW_GRANT:
    return parseGrant();

case TokenType::KW_REVOKE:
    return parseRevoke();

case TokenType::KW_SET:
    if (peek(1).type == TokenType::KW_ROLE) return parseSetRole();
    if (peek(1).type == TokenType::KW_SESSION) return parseSetSessionAuth();
    // ... existing cases

case TokenType::KW_RESET:
    if (peek(1).type == TokenType::KW_ROLE) return parseSetRole();  // is_reset=true
    if (peek(1).type == TokenType::KW_SESSION) return parseSetSessionAuth();  // is_reset=true
    // ... existing cases
```

**Example Implementation** (parseCreateUser):
```cpp
std::unique_ptr<Statement> Parser::parseCreateUser()
{
    SourceLocation start = current().location;
    expect(TokenType::KW_CREATE);
    expect(TokenType::KW_USER);

    // Username
    Token username_tok = expect(TokenType::IDENTIFIER);
    StringPool::StringId username = username_tok.value.string_id;

    // Optional: WITH PASSWORD 'password'
    bool has_password = false;
    StringPool::StringId password = 0;
    if (match(TokenType::KW_WITH))
    {
        expect(TokenType::KW_PASSWORD);
        Token pwd_tok = expect(TokenType::STRING_LITERAL);
        password = pwd_tok.value.string_id;
        has_password = true;
    }

    // Optional: SUPERUSER | NOSUPERUSER
    bool is_superuser = false;
    if (match(TokenType::KW_SUPERUSER))
    {
        is_superuser = true;
    }
    else if (match(TokenType::KW_NOSUPERUSER))
    {
        is_superuser = false;
    }

    SourceLocation end = previous().location;
    return std::make_unique<CreateUserStmt>(
        SourceSpan(start, end), username, password, has_password, is_superuser
    );
}
```

### 4. Bytecode Opcodes (Estimated: 2-3 hours)

**File to Modify**:
- `include/scratchbird/sblr/opcodes.h`

**Opcodes to Add**:
```cpp
// Security opcodes (add after existing opcodes)
OP_CREATE_USER,
OP_ALTER_USER,
OP_DROP_USER,
OP_CREATE_ROLE,
OP_DROP_ROLE,
OP_CREATE_GROUP,
OP_DROP_GROUP,
OP_GRANT_PRIVILEGE,
OP_REVOKE_PRIVILEGE,
OP_GRANT_ROLE,
OP_REVOKE_ROLE,
OP_SET_ROLE,
OP_SET_SESSION_AUTH,
```

### 5. Bytecode Generation (Estimated: 8-10 hours)

**File to Modify**:
- `src/sblr/bytecode_generator.cpp`
- `include/scratchbird/sblr/bytecode_generator.h`

**Functions to Implement**:
```cpp
Status BytecodeGenerator::generateCreateUser(CreateUserStmt* stmt);
Status BytecodeGenerator::generateAlterUser(AlterUserStmt* stmt);
Status BytecodeGenerator::generateDropUser(DropUserStmt* stmt);
Status BytecodeGenerator::generateCreateRole(CreateRoleStmt* stmt);
Status BytecodeGenerator::generateDropRole(DropRoleStmt* stmt);
Status BytecodeGenerator::generateCreateGroup(CreateGroupStmt* stmt);
Status BytecodeGenerator::generateDropGroup(DropGroupStmt* stmt);
Status BytecodeGenerator::generateGrantPrivilege(GrantPrivilegeStmt* stmt);
Status BytecodeGenerator::generateRevokePrivilege(RevokePrivilegeStmt* stmt);
Status BytecodeGenerator::generateGrantRole(GrantRoleStmt* stmt);
Status BytecodeGenerator::generateRevokeRole(RevokeRoleStmt* stmt);
Status BytecodeGenerator::generateSetRole(SetRoleStmt* stmt);
Status BytecodeGenerator::generateSetSessionAuth(SetSessionAuthStmt* stmt);
```

**Bytecode Format Example** (CREATE USER):
```
OP_CREATE_USER
  - username (string ref)
  - has_password (bool)
  - password (string ref)
  - is_superuser (bool)
```

### 6. Executor Implementation (Estimated: 10-12 hours)

**File to Modify**:
- `src/sblr/executor.cpp`
- `include/scratchbird/sblr/executor.h`

**Functions to Implement**:
```cpp
Status Executor::executeCreateUser(CreateUserStmt* stmt, Session* session);
Status Executor::executeAlterUser(AlterUserStmt* stmt, Session* session);
Status Executor::executeDropUser(DropUserStmt* stmt, Session* session);
Status Executor::executeCreateRole(CreateRoleStmt* stmt, Session* session);
Status Executor::executeDropRole(DropRoleStmt* stmt, Session* session);
Status Executor::executeCreateGroup(CreateGroupStmt* stmt, Session* session);
Status Executor::executeDropGroup(DropGroupStmt* stmt, Session* session);
Status Executor::executeGrantPrivilege(GrantPrivilegeStmt* stmt, Session* session);
Status Executor::executeRevokePrivilege(RevokePrivilegeStmt* stmt, Session* session);
Status Executor::executeGrantRole(GrantRoleStmt* stmt, Session* session);
Status Executor::executeRevokeRole(RevokeRoleStmt* stmt, Session* session);
Status Executor::executeSetRole(SetRoleStmt* stmt, Session* session);
Status Executor::executeSetSessionAuth(SetSessionAuthStmt* stmt, Session* session);
```

**Example Implementation** (executeCreateUser):
```cpp
Status Executor::executeCreateUser(CreateUserStmt* stmt, Session* session)
{
    ErrorContext ctx;

    // Check permission: only superusers can create users
    if (!session->is_superuser)
    {
        SET_ERROR_CONTEXT(&ctx, Status::PERMISSION_DENIED,
            "Only superusers can create users");
        return Status::PERMISSION_DENIED;
    }

    // Get username string from pool
    std::string username = string_pool_.get(stmt->username());
    std::string password;
    if (stmt->hasPassword())
    {
        password = string_pool_.get(stmt->password());
    }

    // Call catalog manager
    ID user_id;
    Status s = db_->catalog_manager()->createUser(
        username,
        password,
        stmt->isSuperuser(),
        user_id,
        &ctx
    );

    if (s != Status::OK)
    {
        return s;
    }

    // Set result
    result_.setMessage("CREATE USER successful");
    return Status::OK;
}
```

### 7. Permission Check Hooks (Estimated: 10-15 hours)

**File to Modify**:
- `src/sblr/executor.cpp`

**DML Permission Checks** (4 functions):
```cpp
Status Executor::executeSelect(SelectStmt* stmt, Session* session)
{
    // BEFORE existing logic:

    // Check SELECT privilege on all tables in FROM clause
    for (const auto& table_ref : stmt->from_clause)
    {
        bool has_perm = false;
        Status s = db_->catalog_manager()->hasPermission(
            session->user_id,
            table_ref.table_id,
            PermissionObjectType::TABLE,
            Privilege::SELECT,
            has_perm,
            &ctx
        );

        if (s != Status::OK || !has_perm)
        {
            SET_ERROR_CONTEXT(&ctx, Status::PERMISSION_DENIED,
                "SELECT permission denied on table");
            return Status::PERMISSION_DENIED;
        }
    }

    // ... existing SELECT logic
}

Status Executor::executeInsert(InsertStmt* stmt, Session* session)
{
    // Check INSERT privilege
    bool has_perm = false;
    Status s = db_->catalog_manager()->hasPermission(
        session->user_id,
        stmt->table_id,
        PermissionObjectType::TABLE,
        Privilege::INSERT,
        has_perm,
        &ctx
    );

    if (s != Status::OK || !has_perm)
    {
        SET_ERROR_CONTEXT(&ctx, Status::PERMISSION_DENIED,
            "INSERT permission denied");
        return Status::PERMISSION_DENIED;
    }

    // ... existing INSERT logic
}

Status Executor::executeUpdate(UpdateStmt* stmt, Session* session)
{
    // Check UPDATE and SELECT (for WHERE clause)
    bool has_update = false, has_select = false;

    db_->catalog_manager()->hasPermission(
        session->user_id, stmt->table_id, PermissionObjectType::TABLE,
        Privilege::UPDATE, has_update, &ctx
    );

    db_->catalog_manager()->hasPermission(
        session->user_id, stmt->table_id, PermissionObjectType::TABLE,
        Privilege::SELECT, has_select, &ctx
    );

    if (!has_update || !has_select)
    {
        SET_ERROR_CONTEXT(&ctx, Status::PERMISSION_DENIED,
            "UPDATE/SELECT permission denied");
        return Status::PERMISSION_DENIED;
    }

    // ... existing UPDATE logic
}

Status Executor::executeDelete(DeleteStmt* stmt, Session* session)
{
    // Check DELETE and SELECT (for WHERE clause)
    bool has_delete = false, has_select = false;

    db_->catalog_manager()->hasPermission(
        session->user_id, stmt->table_id, PermissionObjectType::TABLE,
        Privilege::DELETE, has_delete, &ctx
    );

    db_->catalog_manager()->hasPermission(
        session->user_id, stmt->table_id, PermissionObjectType::TABLE,
        Privilege::SELECT, has_select, &ctx
    );

    if (!has_delete || !has_select)
    {
        SET_ERROR_CONTEXT(&ctx, Status::PERMISSION_DENIED,
            "DELETE/SELECT permission denied");
        return Status::PERMISSION_DENIED;
    }

    // ... existing DELETE logic
}
```

**DDL Permission Checks** (3 functions):
```cpp
Status Executor::executeCreateTable(CreateTableStmt* stmt, Session* session)
{
    // Check CREATE privilege on schema
    bool has_perm = false;
    Status s = db_->catalog_manager()->hasPermission(
        session->user_id,
        stmt->schema_id,
        PermissionObjectType::SCHEMA,
        Privilege::CREATE,
        has_perm,
        &ctx
    );

    if (s != Status::OK || !has_perm)
    {
        SET_ERROR_CONTEXT(&ctx, Status::PERMISSION_DENIED,
            "CREATE permission denied on schema");
        return Status::PERMISSION_DENIED;
    }

    // ... existing CREATE TABLE logic

    // AFTER table creation: Set owner to current user
    table_rec.owner_id = session->user_id;
}

Status Executor::executeDropTable(DropTableStmt* stmt, Session* session)
{
    // Get table info
    TableInfo table;
    db_->catalog_manager()->getTable(stmt->table_id, table, &ctx);

    // Check: must be owner or superuser
    if (table.owner_id != session->user_id && !session->is_superuser)
    {
        SET_ERROR_CONTEXT(&ctx, Status::PERMISSION_DENIED,
            "Only table owner or superuser can drop table");
        return Status::PERMISSION_DENIED;
    }

    // ... existing DROP TABLE logic
}

Status Executor::executeAlterTable(AlterTableStmt* stmt, Session* session)
{
    // Get table info
    TableInfo table;
    db_->catalog_manager()->getTable(stmt->table_id, table, &ctx);

    // Check: must be owner or superuser
    if (table.owner_id != session->user_id && !session->is_superuser)
    {
        SET_ERROR_CONTEXT(&ctx, Status::PERMISSION_DENIED,
            "Only table owner or superuser can alter table");
        return Status::PERMISSION_DENIED;
    }

    // ... existing ALTER TABLE logic
}
```

### 8. ASTPrinter Implementation (Estimated: 3-4 hours)

**File to Modify**:
- `src/parser/ast.cpp` (or wherever ASTPrinter methods are implemented)

**Functions to Implement**:
```cpp
void ASTPrinter::visit(CreateUserStmt *node) { /* Print CREATE USER ... */ }
void ASTPrinter::visit(AlterUserStmt *node) { /* Print ALTER USER ... */ }
void ASTPrinter::visit(DropUserStmt *node) { /* Print DROP USER ... */ }
void ASTPrinter::visit(CreateRoleStmt *node) { /* Print CREATE ROLE ... */ }
void ASTPrinter::visit(DropRoleStmt *node) { /* Print DROP ROLE ... */ }
void ASTPrinter::visit(CreateGroupStmt *node) { /* Print CREATE GROUP ... */ }
void ASTPrinter::visit(DropGroupStmt *node) { /* Print DROP GROUP ... */ }
void ASTPrinter::visit(GrantPrivilegeStmt *node) { /* Print GRANT ... */ }
void ASTPrinter::visit(RevokePrivilegeStmt *node) { /* Print REVOKE ... */ }
void ASTPrinter::visit(GrantRoleStmt *node) { /* Print GRANT role ... */ }
void ASTPrinter::visit(RevokeRoleStmt *node) { /* Print REVOKE role ... */ }
void ASTPrinter::visit(SetRoleStmt *node) { /* Print SET/RESET ROLE ... */ }
void ASTPrinter::visit(SetSessionAuthStmt *node) { /* Print SET/RESET SESSION AUTHORIZATION ... */ }
```

### 9. Testing (Estimated: 5-8 hours)

**Test Cases to Implement**:
1. Parser tests for all security statements
2. Bytecode generation tests
3. Executor tests for CREATE/ALTER/DROP USER/ROLE/GROUP
4. GRANT/REVOKE privilege tests
5. GRANT/REVOKE role tests
6. Permission check tests (DML)
7. Permission check tests (DDL)
8. SET ROLE / RESET ROLE tests
9. SET SESSION AUTHORIZATION tests
10. Error handling tests (permission denied, etc.)

---

## Implementation Order

### Recommended Sequence:

1. **Parser Implementation** (20-25 hours)
   - Start with simpler statements (CREATE ROLE, CREATE GROUP)
   - Move to CREATE/ALTER/DROP USER (password handling)
   - Implement GRANT/REVOKE (most complex due to privilege parsing)

2. **Bytecode Opcodes & Generation** (10-13 hours)
   - Add opcodes
   - Implement bytecode generation for all statements
   - Test bytecode output

3. **Executor Implementation** (10-12 hours)
   - Implement security statement executors
   - Wire up to catalog manager functions (already implemented in Phase 1)

4. **Permission Check Hooks** (10-15 hours)
   - Add DML permission checks
   - Add DDL permission checks
   - Test enforcement

5. **ASTPrinter & Testing** (8-12 hours)
   - Implement ASTPrinter methods
   - Write comprehensive tests
   - Fix any bugs found

**Total Estimated Time**: 58-77 hours

---

## Files Summary

### Modified So Far:
1. `include/scratchbird/parser/token.h` - 16 new keywords
2. `src/parser/lexer.cpp` - 16 keyword mappings
3. `include/scratchbird/parser/ast.h` - 13 AST kinds, 13 statement classes, 26 visitor methods

### To Be Modified:
4. `include/scratchbird/parser/parser.h` - Parser method declarations
5. `src/parser/parser.cpp` - Parser implementation (~800-1000 lines)
6. `include/scratchbird/sblr/opcodes.h` - 13 new opcodes
7. `src/sblr/bytecode_generator.cpp` - Bytecode generation (~400-500 lines)
8. `include/scratchbird/sblr/executor.h` - Executor method declarations
9. `src/sblr/executor.cpp` - Executor implementation + permission hooks (~800-1000 lines)
10. `src/parser/ast.cpp` - ASTPrinter implementation (~200-300 lines)
11. Test files - Comprehensive test suite

**Total New/Modified Code**: ~2,200-2,800 lines

---

## Dependencies

### Phase 1 Complete ✅:
- Catalog Manager security functions (34 functions)
- Session management
- Permission checking with transitive closure
- Bootstrap (SYSTEM user, PUBLIC role, DB_OWNER role)

### External Dependencies:
- Password hashing library (bcrypt or argon2) - TO BE ADDED

---

## Next Steps

1. **Immediate**: Implement parser functions in `src/parser/parser.cpp`
2. **Short-term**: Add bytecode opcodes and generation
3. **Medium-term**: Implement executors and permission hooks
4. **Final**: Testing and documentation

---

## Notes

- Security API from Phase 1 is fully functional and tested
- All AST structures are type-safe with proper enums
- Privilege bitmasks match catalog manager definitions exactly
- CASCADE/RESTRICT behavior matches PostgreSQL semantics
- Session management ready for connection handler integration

---

**Document Version**: 1.0
**Author**: Claude (Anthropic)
**Next Update**: After parser implementation complete
