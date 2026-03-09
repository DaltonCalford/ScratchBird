# Error Codes Reference

[Error Model README](../README.md) | [Error and Diagnostics README](../../README.md)

## Synopsis

Complete reference of ScratchBird error codes, SQLSTATE codes, and diagnostic messages.

## SQLSTATE Code Classes

| Class | Category |
|-------|----------|
| `00` | Successful completion |
| `01` | Warning |
| `02` | No data |
| `08` | Connection exception |
| `09` | Triggered action exception |
| `0A` | Feature not supported |
| `0B` | Invalid transaction initiation |
| `0F` | Locator exception |
| `0L` | Invalid grantor |
| `0P` | Invalid role specification |
| `21` | Cardinality violation |
| `22` | Data exception |
| `23` | Integrity constraint violation |
| `24` | Invalid cursor state |
| `25` | Invalid transaction state |
| `26` | Invalid SQL statement name |
| `27` | Triggered data change violation |
| `28` | Invalid authorization specification |
| `2B` | Dependent privilege descriptors exist |
| `2D` | Invalid transaction termination |
| `2F` | SQL routine exception |
| `34` | Invalid cursor name |
| `38` | External routine exception |
| `39` | External routine invocation exception |
| `3B` | Savepoint exception |
| `3D` | Invalid catalog name |
| `3F` | Invalid schema name |
| `40` | Transaction rollback |
| `42` | Syntax error or access rule violation |
| `44` | WITH CHECK OPTION violation |
| `53` | Insufficient resources |
| `54` | Program limit exceeded |
| `55` | Object not in prerequisite state |
| `57` | Operator intervention |
| `58` | System error |
| `F0` | Configuration file error |
| `HV` | Foreign data wrapper error |
| `P0` | PL/pgSQL error |
| `P1` | SBLR execution error |
| `XX` | Internal error |

## Common Error Codes

### Connection Errors (Class 08)

| SQLSTATE | Error | Description |
|----------|-------|-------------|
| `08000` | connection_exception | General connection failure |
| `08003` | connection_does_not_exist | Connection not established |
| `08004` | connection_rejected | Server rejected connection |
| `08006` | connection_failure | Connection lost |
| `08001` | sqlclient_unable_to_establish_sqlconnection | Client connect failed |
| `08004` | sqlserver_rejected_establishment_of_sqlconnection | Server reject |

### Data Errors (Class 22)

| SQLSTATE | Error | Description |
|----------|-------|-------------|
| `22000` | data_exception | General data error |
| `22001` | string_data_right_truncation | Value too long |
| `22002` | null_value_no_indicator_parameter | NULL not allowed |
| `22003` | numeric_value_out_of_range | Number overflow |
| `22004` | null_value_not_allowed | NULL violation |
| `22007` | invalid_datetime_format | Bad date format |
| `22008` | datetime_field_overflow | Date out of range |
| `22012` | division_by_zero | Division by zero |
| `22018` | invalid_character_value_for_cast | Bad cast |
| `22019` | invalid_escape_character | Bad escape |
| `22021` | character_not_in_repertoire | Encoding error |
| `22023` | invalid_parameter_value | Bad parameter |
| `22024` | unterminated_c_string | Unclosed string |
| `22025` | invalid_escape_sequence | Bad escape sequence |

### Integrity Errors (Class 23)

| SQLSTATE | Error | Description |
|----------|-------|-------------|
| `23000` | integrity_constraint_violation | Constraint failed |
| `23502` | not_null_violation | NOT NULL violation |
| `23503` | foreign_key_violation | Foreign key failed |
| `23505` | unique_violation | Duplicate key |
| `23514` | check_violation | CHECK constraint failed |

### Syntax Errors (Class 42)

| SQLSTATE | Error | Description |
|----------|-------|-------------|
| `42000` | syntax_error_or_access_rule_violation | Syntax/access |
| `42501` | insufficient_privilege | Permission denied |
| `42601` | syntax_error | Syntax error |
| `42602` | invalid_name | Invalid name |
| `42703` | undefined_column | Column not found |
| `42883` | undefined_function | Function not found |
| `42P01` | undefined_table | Table not found |
| `42P02` | undefined_parameter | Parameter not found |

### Transaction Errors (Class 25, 40)

| SQLSTATE | Error | Description |
|----------|-------|-------------|
| `25000` | invalid_transaction_state | Bad transaction state |
| `25001` | active_sql_transaction | Transaction active |
| `25P01` | no_active_sql_transaction | No transaction |
| `25P02` | in_failed_sql_transaction | Failed transaction |
| `40000` | transaction_rollback | Transaction rolled back |
| `40001` | serialization_failure | Serialization failed |
| `40002` | integrity_constraint_violation | Constraint in xact |
| `40003` | statement_completion_unknown | Statement unknown |

### SB-Specific Errors (Classes P0, P1, XX)

| SQLSTATE | Error | Description |
|----------|-------|-------------|
| `P0001` | plpgsql_raise | RAISE statement |
| `P0002` | plpgsql_no_data_found | No data found |
| `P0003` | plpgsql_too_many_rows | Too many rows |
| `P1000` | sblr_execution_error | SBLR error |
| `P1001` | sblr_type_mismatch | Type error |
| `P1002` | sblr_undefined_variable | Variable not found |
| `XX000` | internal_error | Internal error |
| `XX001` | data_corrupted | Data corruption |
| `XX002` | index_corrupted | Index corruption |

## Error Message Format

```
ERROR: <message>
DETAIL: <additional info>
HINT: <suggestion>
CONTEXT: <context>
SQLSTATE: <code>
```

Example:
```
ERROR: duplicate key value violates unique constraint "users_email_key"
DETAIL: Key (email)=(john@example.com) already exists.
SQLSTATE: 23505
```

## Handling Errors

### Application Error Handling

```python
# Python example
try:
    cursor.execute("INSERT INTO users (email) VALUES ('dup@example.com')")
except psycopg2.IntegrityError as e:
    if e.pgcode == '23505':
        print("Duplicate email")
    elif e.pgcode == '23502':
        print("Required field missing")
```

### SQL Error Handling

```sql
-- PL/pgSQL exception handling
BEGIN
    INSERT INTO users (email) VALUES ('dup@example.com');
EXCEPTION 
    WHEN unique_violation THEN
        RAISE NOTICE 'Email already exists';
    WHEN not_null_violation THEN
        RAISE NOTICE 'Required field missing';
    WHEN OTHERS THEN
        RAISE EXCEPTION 'Unexpected error: %', SQLERRM;
END;
```

## Error Logging

```sql
-- View recent errors
SELECT 
    timestamp,
    sqlstate,
    message,
    detail,
    query
FROM pg_log
WHERE timestamp > NOW() - INTERVAL '1 hour'
ORDER BY timestamp DESC;
```

## See Also

- [Parser Errors](../parser_errors/README.md)
- [Runtime and Execution Errors](../runtime_and_execution_errors/README.md)
- [Security and Auth Errors](../security_and_auth_errors/README.md)
