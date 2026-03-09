# Specification: ANALYZE Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2276`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:15850`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_utility.cpp`

## Synopsis

The ANALYZE statement collects statistics about database contents for query optimization. Supports table-level, column-level, and index-level analysis with sampling options.

## Specification

### EBNF Grammar

```ebnf
analyze_stmt ::=
    "ANALYZE" [ "VERBOSE" ]
    [ ( "TABLE" | "INDEX" ) ]
    [ schema_path ]
    [ analyze_column_spec ]
    [ analyze_sample_spec ]

analyze_column_spec ::=
    "(" column_list ")"
  | "COLUMN" column_name

analyze_sample_spec ::=
    "SAMPLE" numeric_literal [ "%" ]
  | "USING" "SAMPLE" numeric_literal [ "%" ]
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2276
class AnalyzeStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AnalyzeStmt; }
    
    enum class AnalyzeTarget : uint8_t {
        TABLE, INDEX
    };

    AnalyzeTarget target = AnalyzeTarget::TABLE;
    SchemaPath table_path;
    SchemaPath index_path;
    StringPool::StringId column_name = StringPool::INVALID_ID;
    bool has_column = false;
    bool has_sample = false;
    double sample_rate = 0.0;
    bool verbose = false;
};
```

## Examples

```sql
-- Analyze all tables in current database
ANALYZE;

-- Analyze specific table
ANALYZE users;

-- Analyze table with verbose output
ANALYZE VERBOSE orders;

-- Analyze specific columns
ANALYZE users (name, email);

-- Analyze with sampling
ANALYZE large_table SAMPLE 10%;

-- Analyze index
ANALYZE INDEX idx_users_email;

-- Analyze specific column
ANALYZE TABLE orders COLUMN status;
```

## Related Specifications

- [stmt_explain.md](./stmt_explain.md) - Execution plans
- [stmt_create_index.md](./stmt_create_index.md) - Index creation

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
