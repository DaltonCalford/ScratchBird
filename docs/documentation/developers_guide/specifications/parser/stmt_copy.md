# Specification: COPY Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:3822`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:11565`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_utility.cpp`

## Synopsis

The COPY statement transfers data between database tables and external files. Supports various formats (CSV, TEXT, BINARY) and options for custom delimiters, encoding, and error handling.

## Specification

### EBNF Grammar

```ebnf
copy_stmt ::=
    "COPY" copy_target
    [ "(" column_list ")" ]
    copy_direction copy_source
    [ "WITH" "(" copy_option ("," copy_option )* ")" ]
    [ "WHERE" expression ]

copy_target ::=
    schema_path
  | "(" select_stmt ")"

copy_direction ::= "FROM" | "TO"

copy_source ::=
    "STDIN"
  | "STDOUT"
  | [ "PROGRAM" ] string_literal

copy_option ::=
    "FORMAT" ( "TEXT" | "CSV" | "BINARY" )
  | "FREEZE" boolean
  | "DELIMITER" string
  | "NULL" string
  | "HEADER" boolean
  | "QUOTE" string
  | "ESCAPE" string
  | "FORCE_QUOTE" "(" column_list ")" | "*"
  | "FORCE_NOT_NULL" "(" column_list ")"
  | "FORCE_NULL" "(" column_list ")"
  | "ENCODING" string
  | "BATCH_SIZE" integer
  | "MAX_ERRORS" integer
  | "ON_ERROR" ( "ABORT" | "SKIP" )
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:3822
class CopyStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CopyStmt; }
    
    // COPY (SELECT ...) TO ...
    SelectStmt* query = nullptr;

    // Target table
    SchemaPath table_path;

    // Column list (optional)
    std::vector<StringPool::StringId> columns;

    enum class Direction { FROM, TO };
    Direction direction = Direction::FROM;

    // Target source/destination
    bool target_is_stdin = false;
    bool target_is_stdout = false;
    bool target_is_program = false;
    StringPool::StringId target = StringPool::INVALID_ID;

    CopyOptions options;
};

struct CopyOptions {
    enum class Format : uint8_t { TEXT = 0, CSV = 1, BINARY = 2 };
    enum class OnError : uint8_t { ABORT = 0, SKIP = 1 };

    bool format_set = false;
    Format format = Format::TEXT;

    bool delimiter_set = false;
    StringPool::StringId delimiter = StringPool::INVALID_ID;

    bool null_set = false;
    StringPool::StringId null_string = StringPool::INVALID_ID;

    bool header_set = false;
    bool header = false;

    bool quote_set = false;
    StringPool::StringId quote = StringPool::INVALID_ID;

    bool escape_set = false;
    StringPool::StringId escape = StringPool::INVALID_ID;

    bool encoding_set = false;
    StringPool::StringId encoding = StringPool::INVALID_ID;

    bool batch_size_set = false;
    int64_t batch_size = 0;

    bool max_errors_set = false;
    int64_t max_errors = 0;

    bool on_error_set = false;
    OnError on_error = OnError::ABORT;
};
```

## Examples

```sql
-- Copy table to CSV file
COPY users TO '/tmp/users.csv' WITH (FORMAT CSV, HEADER);

-- Copy from CSV file
COPY users FROM '/tmp/users.csv' WITH (FORMAT CSV, HEADER);

-- Copy with custom delimiter
COPY orders TO '/tmp/orders.txt' WITH (FORMAT TEXT, DELIMITER '|');

-- Copy specific columns
COPY users (id, name, email) TO '/tmp/users_subset.csv' WITH (FORMAT CSV);

-- Copy from program
COPY logs FROM PROGRAM 'gzip -dc /var/log/app.log.gz' WITH (FORMAT CSV);

-- Copy query results
COPY (SELECT * FROM orders WHERE status = 'completed') TO '/tmp/completed_orders.csv';

-- Copy with encoding
COPY products FROM '/tmp/products.csv' WITH (FORMAT CSV, ENCODING 'UTF8');

-- Copy with error handling
COPY large_table FROM '/tmp/data.csv' WITH (FORMAT CSV, BATCH_SIZE 10000, MAX_ERRORS 100, ON_ERROR SKIP);

-- Copy to STDOUT
COPY users TO STDOUT WITH (FORMAT CSV, HEADER);
```

## Related Specifications

- [stmt_select.md](./stmt_select.md) - SELECT statement (for COPY query)
- [stmt_create_table.md](./stmt_create_table.md) - Table creation

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
