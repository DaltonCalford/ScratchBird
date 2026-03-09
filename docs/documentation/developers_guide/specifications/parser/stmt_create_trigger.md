# Specification: CREATE TRIGGER Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1027`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:4900`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_psql.cpp`

## Synopsis

The CREATE TRIGGER statement defines a trigger that automatically executes when specified events occur on a table or database. Supports row-level and statement-level triggers with BEFORE, AFTER, and INSTEAD OF timing.

## Specification

### EBNF Grammar

```ebnf
create_trigger_stmt ::=
    "CREATE" [ "OR" ( "REPLACE" | "ALTER" ) ]
    "TRIGGER" identifier
    trigger_timing trigger_event [ "OR" trigger_event ]*
    [ "ACTIVE" | "INACTIVE" ]
    [ "DATABASE" ]
    "ON" schema_path
    trigger_granularity
    [ "POSITION" integer ]
    [ "SQL" "SECURITY" ( "INVOKER" | "DEFINER" ) ]
    trigger_body

trigger_timing ::= "BEFORE" | "AFTER" | "INSTEAD" "OF"

trigger_event ::=
    "INSERT" | "UPDATE" | "DELETE" |
    "CONNECT" | "DISCONNECT" |
    "TRANSACTION" "START" | "TRANSACTION" "COMMIT" | "TRANSACTION" "ROLLBACK"

trigger_granularity ::=
    "FOR" "EACH" "ROW" | "FOR" "EACH" "STATEMENT"

trigger_body ::=
    "AS" string_literal
  | "BEGIN" [ "ATOMIC" ] statement_list "END"
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1027
class CreateTriggerStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateTriggerStmt; }
    
    bool or_replace = false;
    bool active = true;
    bool is_database_trigger = false;
    bool has_sql_security = false;
    RoutineSqlSecurity sql_security = RoutineSqlSecurity::INVOKER;

    StringPool::StringId trigger_name = StringPool::INVALID_ID;
    SchemaPath table_path;

    TriggerTiming timing = TriggerTiming::BEFORE;
    uint8_t event_mask = 1u << static_cast<uint8_t>(TriggerEvent::INSERT);
    TriggerGranularity granularity = TriggerGranularity::FOR_EACH_ROW;

    StringPool::StringId body = StringPool::INVALID_ID;
};

enum class TriggerTiming : uint8_t {
    BEFORE = 0, AFTER = 1, INSTEAD_OF = 2
};

enum class TriggerEvent : uint8_t {
    INSERT = 0, UPDATE = 1, DELETE = 2,
    CONNECT = 3, DISCONNECT = 4,
    TRANSACTION_START = 5, TRANSACTION_COMMIT = 6, TRANSACTION_ROLLBACK = 7
};

enum class TriggerGranularity : uint8_t {
    FOR_EACH_ROW = 0, FOR_EACH_STATEMENT = 1
};
```

## Examples

```sql
-- Basic BEFORE INSERT trigger
CREATE TRIGGER trg_users_created_at
BEFORE INSERT ON users
FOR EACH ROW
AS
BEGIN
    NEW.created_at = CURRENT_TIMESTAMP;
END;

-- AFTER UPDATE trigger with audit logging
CREATE TRIGGER trg_orders_audit
AFTER UPDATE ON orders
FOR EACH ROW
AS
BEGIN
    INSERT INTO audit_log (table_name, record_id, action, changed_at)
    VALUES ('orders', OLD.id, 'UPDATE', CURRENT_TIMESTAMP);
END;

-- Multiple events
CREATE TRIGGER trg_products_changes
BEFORE INSERT OR UPDATE OR DELETE ON products
FOR EACH ROW
AS
BEGIN
    IF (INSERTING) THEN
        NEW.modified_at = CURRENT_TIMESTAMP;
    ELSIF (UPDATING) THEN
        NEW.modified_at = CURRENT_TIMESTAMP;
    END IF;
END;

-- Database trigger
CREATE TRIGGER trg_db_connect
ON CONNECT
AS
BEGIN
    INSERT INTO connection_log (username, connect_time)
    VALUES (CURRENT_USER, CURRENT_TIMESTAMP);
END;
```

## Related Specifications

- [stmt_create_function.md](./stmt_create_function.md) - Function creation
- [stmt_create_table.md](./stmt_create_table.md) - Table creation
- [stmt_drop_trigger.md](./stmt_drop_trigger.md) - Trigger removal

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
