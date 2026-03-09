# Specification: Row-Level Security (RLS) Statements

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

- Source anchors: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1114-1154`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:5128`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_dcl.cpp`

## Synopsis

Row-Level Security (RLS) policies control which rows users can access or modify. CREATE POLICY defines access rules, ALTER POLICY modifies them, and DROP POLICY removes them.

## Specification

### EBNF Grammar

```ebnf
create_policy_stmt ::=
    "CREATE" "POLICY" identifier
    "ON" schema_path
    [ "AS" ( "PERMISSIVE" | "RESTRICTIVE" ) ]
    "FOR" policy_type
    "TO" role_list
    [ "USING" "(" expression ")" ]
    [ "WITH" "CHECK" "(" expression ")" ]

policy_type ::= "ALL" | "SELECT" | "INSERT" | "UPDATE" | "DELETE"

role_list ::= role_name ("," role_name )* | "PUBLIC"

alter_policy_stmt ::=
    "ALTER" "POLICY" identifier
    "ON" schema_path
    [ "RENAME" "TO" new_name ]
    [ "TO" role_list ]
    [ "USING" "(" expression ")" ]
    [ "WITH" "CHECK" "(" expression ")" ]

drop_policy_stmt ::=
    "DROP" "POLICY" [ "IF" "EXISTS" ] identifier
    "ON" schema_path
    [ "CASCADE" | "RESTRICT" ]
```

### AST Node Structures

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1114
class CreatePolicyStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreatePolicyStmt; }
    
    StringPool::StringId policy_name = StringPool::INVALID_ID;
    SchemaPath table_path;
    PolicyType policy_type = PolicyType::ALL;
    bool is_permissive = true;  // true = PERMISSIVE, false = RESTRICTIVE
    std::vector<StringPool::StringId> roles;  // Empty = PUBLIC
    Expression* using_expr = nullptr;  // USING expression (for SELECT, UPDATE, DELETE)
    Expression* with_check_expr = nullptr;  // WITH CHECK expression (for INSERT, UPDATE)
};

// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1131
class AlterPolicyStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterPolicyStmt; }
    
    StringPool::StringId policy_name = StringPool::INVALID_ID;
    SchemaPath table_path;
    std::vector<StringPool::StringId> roles;
    Expression* using_expr = nullptr;
    Expression* with_check_expr = nullptr;
};

// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1146
class DropPolicyStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropPolicyStmt; }
    
    StringPool::StringId policy_name = StringPool::INVALID_ID;
    SchemaPath table_path;
    bool if_exists = false;
};

enum class PolicyType : uint8_t {
    ALL = 0, SELECT = 1, INSERT = 2, UPDATE = 3, DELETE = 4
};
```

## Examples

```sql
-- Enable RLS on table
ALTER TABLE employees ENABLE ROW LEVEL SECURITY;

-- Create policy for SELECT (users see only their own records)
CREATE POLICY employee_self_access ON employees
    FOR SELECT
    TO PUBLIC
    USING (user_id = current_user_id());

-- Create policy for UPDATE (managers can update their department)
CREATE POLICY manager_update_policy ON employees
    FOR UPDATE
    TO manager_role
    USING (department_id IN (
        SELECT department_id FROM department_managers WHERE manager_id = current_user_id()
    ));

-- Create restrictive policy (restricts access beyond other policies)
CREATE POLICY hide_terminated AS RESTRICTIVE ON employees
    FOR ALL
    TO PUBLIC
    USING (status != 'terminated');

-- Policy with USING and WITH CHECK (different rules for viewing vs inserting)
CREATE POLICY order_policy ON orders
    FOR ALL
    TO sales_role
    USING (region = current_user_region())
    WITH CHECK (region = current_user_region() AND status = 'pending');

-- Alter policy
ALTER POLICY employee_self_access ON employees
    TO employee_role, contractor_role;

-- Drop policy
DROP POLICY employee_self_access ON employees;

-- Drop if exists
DROP POLICY IF EXISTS old_policy ON employees;
```

## Related Specifications

- [stmt_grant_revoke.md](./stmt_grant_revoke.md) - Privilege management
- [stmt_alter_table.md](./stmt_alter_table.md) - Table modification (enable/disable RLS)

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
