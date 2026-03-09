# Specification: EXPLAIN Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2252`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:15755`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_utility.cpp`

## Synopsis

The EXPLAIN statement displays the execution plan for a query without executing it. EXPLAIN ANALYZE executes the query and shows actual runtime statistics.

## Specification

### EBNF Grammar

```ebnf
explain_stmt ::=
    "EXPLAIN" [ explain_option ... ]
    ( select_stmt | insert_stmt | update_stmt | delete_stmt )

explain_option ::=
    "ANALYZE" [ boolean ]
  | "VERBOSE" [ boolean ]
  | "COSTS" [ boolean ]
  | "BUFFERS" [ boolean ]
  | "WAL" [ boolean ]
  | "TIMING" [ boolean ]
  | "FORMAT" ( "TEXT" | "XML" | "JSON" | "YAML" )

boolean ::= "TRUE" | "FALSE" | "ON" | "OFF" | "1" | "0"
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2252
class ExplainStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ExplainStmt; }
    
    Statement* query = nullptr;  // The statement to explain
    bool analyze = false;        // EXPLAIN ANALYZE (actually execute)
    bool verbose = false;        // VERBOSE output
    bool costs = true;           // Show cost estimates (default true)
    bool buffers = false;        // Show buffer usage
    bool wal = false;            // Show WAL/write-ahead instrumentation
    bool timing = true;          // Show timing (with ANALYZE)
    bool format_json = false;    // JSON output format
    bool format_xml = false;     // XML output format
    bool format_yaml = false;    // YAML output format
};
```

## Examples

```sql
-- Basic explain
EXPLAIN SELECT * FROM users WHERE id = 1;

-- Explain with analyze (executes query)
EXPLAIN ANALYZE SELECT * FROM orders JOIN users ON orders.user_id = users.id;

-- Verbose output
EXPLAIN (VERBOSE) SELECT * FROM products;

-- JSON format
EXPLAIN (FORMAT JSON) SELECT * FROM orders;

-- Multiple options
EXPLAIN (ANALYZE, BUFFERS, TIMING, FORMAT JSON)
SELECT * FROM large_table WHERE status = 'pending';

-- Explain DML
EXPLAIN UPDATE accounts SET balance = balance - 100 WHERE id = 123;
EXPLAIN DELETE FROM logs WHERE created_at < CURRENT_DATE - INTERVAL '30 days';
```

## Related Specifications

- [stmt_analyze.md](./stmt_analyze.md) - Statistics collection
- [stmt_select.md](./stmt_select.md) - SELECT statement

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
