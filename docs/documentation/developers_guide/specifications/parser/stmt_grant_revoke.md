# Specification: GRANT and REVOKE Statements

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | parser |
| **Spec Version** | 1.0.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchors: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2356-2386`
- Source anchors: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:18155-18270`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_dcl.cpp`

## Synopsis

The GRANT and REVOKE statements manage database access privileges, controlling what operations users and roles can perform on database objects.

## Specification

### EBNF Grammar

```ebnf
grant_stmt ::=
    "GRANT" privilege_list
    "ON" [ privilege_object_type ] object_name
    "TO" grantee_list
    [ "WITH" "GRANT" "OPTION" ]

revoke_stmt ::=
    "REVOKE" [ "GRANT" "OPTION" "FOR" ]
    privilege_list
    "ON" [ privilege_object_type ] object_name
    "FROM" grantee_list
    [ "CASCADE" | "RESTRICT" ]

privilege_list ::=
    "ALL" [ "PRIVILEGES" ]
  | privilege ("," privilege )*

privilege ::=
    "SELECT" | "INSERT" | "UPDATE" | "DELETE" | "TRUNCATE" |
    "REFERENCES" | "TRIGGER" | "EXECUTE" | "USAGE" | "COPY" |
    "CREATE" | "CONNECT" | "TEMPORARY" | "TEMP" |
    "CREATE_JOB" | "VIEW_JOB_HISTORY" | "EXECUTE_EXTERNAL_JOB"

privilege_object_type ::=
    "TABLE" | "VIEW" | "SEQUENCE" | "FUNCTION" | "PROCEDURE" |
    "JOB" | "SCHEMA" | "DATABASE"

object_name ::=
    schema_path
  | "ALL" ( "TABLES" | "SEQUENCES" | "FUNCTIONS" ) "IN" "SCHEMA" schema_path

grantee_list ::= grantee ("," grantee )*

grantee ::=
    identifier
  | "PUBLIC"
  | "CURRENT_USER"
  | "SESSION_USER"
```

### AST Node Structures

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2356
class GrantStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::GrantStmt; }
    
    std::vector<PrivilegeType> privileges;
    PrivilegeObjectType object_type = PrivilegeObjectType::TABLE;
    std::vector<SchemaPath> objects;
    std::vector<StringPool::StringId> grantees;
    bool with_grant_option = false;
    bool is_public = false;
};

// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2374
class RevokeStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::RevokeStmt; }
    
    std::vector<PrivilegeType> privileges;
    PrivilegeObjectType object_type = PrivilegeObjectType::TABLE;
    std::vector<SchemaPath> objects;
    std::vector<StringPool::StringId> grantees;
    bool grant_option_for = false;
    bool cascade = false;
    bool is_public = false;
};

// Privilege types
enum class PrivilegeType : uint8_t {
    SELECT, INSERT, UPDATE, DELETE, TRUNCATE,
    REFERENCES, TRIGGER, EXECUTE, USAGE, COPY,
    CREATE_JOB, VIEW_JOB_HISTORY, EXECUTE_EXTERNAL_JOB,
    CREATE, CONNECT, TEMPORARY, ALL
};

// Object types
enum class PrivilegeObjectType : uint8_t {
    TABLE, VIEW, SEQUENCE, FUNCTION, PROCEDURE, JOB,
    SCHEMA, DATABASE,
    ALL_TABLES_IN_SCHEMA, ALL_SEQUENCES_IN_SCHEMA, ALL_FUNCTIONS_IN_SCHEMA
};
```

## Examples

```sql
-- Grant SELECT on table
GRANT SELECT ON employees TO user1;

-- Grant multiple privileges
GRANT SELECT, INSERT, UPDATE ON orders TO sales_role;

-- Grant ALL privileges
GRANT ALL PRIVILEGES ON customers TO admin_role;

-- Grant with grant option
GRANT SELECT ON products TO manager WITH GRANT OPTION;

-- Grant to PUBLIC
GRANT SELECT ON public_view TO PUBLIC;

-- Grant on all tables in schema
GRANT SELECT ON ALL TABLES IN SCHEMA sales TO analyst_role;

-- Grant EXECUTE on function
GRANT EXECUTE ON FUNCTION calculate_tax(DECIMAL) TO app_user;

-- Grant USAGE on sequence
GRANT USAGE ON SEQUENCE order_id_seq TO app_user;

-- Revoke privileges
REVOKE UPDATE ON orders FROM user1;

-- Revoke with cascade
REVOKE ALL PRIVILEGES ON customers FROM admin_role CASCADE;

-- Revoke grant option
REVOKE GRANT OPTION FOR SELECT ON products FROM manager;
```

## Related Specifications

- [stmt_rls.md](./stmt_rls.md) - Row-level security
- [stmt_create_user.md](./stmt_create_user.md) - User creation

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
