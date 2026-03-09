# Specification: CREATE FUNCTION/PROCEDURE Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:969`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:4710`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_psql.cpp`

## Synopsis

The CREATE FUNCTION and CREATE PROCEDURE statements define stored routines. Functions return values and can be used in expressions; procedures perform actions and are called via CALL/EXECUTE PROCEDURE.

## Specification

### EBNF Grammar

```ebnf
create_function_stmt ::=
    "CREATE" [ "OR" ( "REPLACE" | "ALTER" ) ]
    "FUNCTION" schema_path
    "(" [ routine_param ("," routine_param )* ] ")"
    "RETURNS" type_name
    [ routine_attributes ]
    routine_body

create_procedure_stmt ::=
    "CREATE" [ "OR" ( "REPLACE" | "ALTER" ) ]
    "PROCEDURE" schema_path
    "(" [ routine_param ("," routine_param )* ] ")"
    [ routine_attributes ]
    routine_body

routine_param ::=
    [ "IN" | "OUT" | "INOUT" ]
    identifier type_name
    [ "=" | "DEFAULT" expression ]

routine_attributes ::=
    [ "DETERMINISTIC" ]
    [ "SQL" "SECURITY" ( "INVOKER" | "DEFINER" ) ]

routine_body ::=
    "AS" string_literal
  | "BEGIN" [ "ATOMIC" ] statement_list "END"
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:969
class CreateFunctionStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateFunctionStmt; }
    
    bool or_replace = false;
    bool deterministic = false;
    RoutineSqlSecurity sql_security = RoutineSqlSecurity::INVOKER;

    SchemaPath function_path;
    std::vector<RoutineParam> params;
    TypeName return_type;
    StringPool::StringId body = StringPool::INVALID_ID;
};

// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:989
class CreateProcedureStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateProcedureStmt; }
    
    bool or_replace = false;
    RoutineSqlSecurity sql_security = RoutineSqlSecurity::INVOKER;

    SchemaPath procedure_path;
    std::vector<RoutineParam> params;
    StringPool::StringId body = StringPool::INVALID_ID;
};

struct RoutineParam {
    RoutineParamMode mode = RoutineParamMode::IN;
    StringPool::StringId name = StringPool::INVALID_ID;
    TypeName type;
    Expression* default_value = nullptr;
    bool has_default = false;
};

enum class RoutineParamMode : uint8_t {
    IN = 0, OUT = 1, INOUT = 2
};

enum class RoutineSqlSecurity : uint8_t {
    INVOKER = 0, DEFINER = 1
};
```

## Examples

```sql
-- Simple function
CREATE FUNCTION calculate_tax(amount DECIMAL(10,2))
RETURNS DECIMAL(10,2)
AS
BEGIN
    RETURN amount * 0.08;
END;

-- Function with multiple parameters
CREATE FUNCTION format_currency(amount DECIMAL(10,2), currency_code VARCHAR(3))
RETURNS VARCHAR(50)
DETERMINISTIC
AS
BEGIN
    RETURN CONCAT(currency_code, ' ', FORMAT(amount, 2));
END;

-- Procedure
CREATE PROCEDURE transfer_funds(
    IN from_account INT,
    IN to_account INT,
    IN amount DECIMAL(10,2)
)
AS
BEGIN
    UPDATE accounts SET balance = balance - amount WHERE id = from_account;
    UPDATE accounts SET balance = balance + amount WHERE id = to_account;
END;

-- Replace existing function
CREATE OR REPLACE FUNCTION get_user_name(user_id INT)
RETURNS VARCHAR(100)
SQL SECURITY INVOKER
AS
BEGIN
    SELECT name INTO :result FROM users WHERE id = user_id;
    RETURN :result;
END;
```

## Related Specifications

- [stmt_create_trigger.md](./stmt_create_trigger.md) - Trigger creation
- [stmt_create_package.md](./stmt_create_package.md) - Package creation
- [stmt_drop_function.md](./stmt_drop_function.md) - Function removal

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
