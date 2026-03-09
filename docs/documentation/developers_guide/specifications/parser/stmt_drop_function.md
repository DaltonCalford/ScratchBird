# Specification: DROP FUNCTION/PROCEDURE/TRIGGER Statements

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | parser |
| **Spec Version** | 1.0.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Team |

## Synopsis

The DROP FUNCTION, DROP PROCEDURE, and DROP TRIGGER statements remove stored routines and triggers from the database.

## Specification

### EBNF Grammar

```ebnf
drop_function_stmt ::=
    "DROP" "FUNCTION" [ "IF" "EXISTS" ]
    schema_path ("," schema_path )*
    [ "CASCADE" | "RESTRICT" ]

drop_procedure_stmt ::=
    "DROP" "PROCEDURE" [ "IF" "EXISTS" ]
    schema_path ("," schema_path )*
    [ "CASCADE" | "RESTRICT" ]

drop_trigger_stmt ::=
    "DROP" "TRIGGER" [ "IF" "EXISTS" ]
    schema_path ("," schema_path )*
    [ "CASCADE" | "RESTRICT" ]
```

### AST Node Structures

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2064
class DropFunctionStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropFunctionStmt; }
    
    bool if_exists = false;
    std::vector<SchemaPath> functions;
};

// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2076
class DropProcedureStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropProcedureStmt; }
    
    bool if_exists = false;
    std::vector<SchemaPath> procedures;
};

// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2088
class DropTriggerStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropTriggerStmt; }
    
    bool if_exists = false;
    std::vector<SchemaPath> triggers;
};
```

## Examples

```sql
-- Drop function
DROP FUNCTION calculate_tax;

-- Drop if exists
DROP FUNCTION IF EXISTS old_function;

-- Drop procedure
DROP PROCEDURE transfer_funds;

-- Drop trigger
DROP TRIGGER trg_users_audit;

-- Drop multiple
DROP FUNCTION func1, func2, func3;

-- Drop with cascade
DROP FUNCTION complex_calc CASCADE;
```

## Related Specifications

- [stmt_create_function.md](./stmt_create_function.md) - Function/procedure creation
- [stmt_create_trigger.md](./stmt_create_trigger.md) - Trigger creation

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
