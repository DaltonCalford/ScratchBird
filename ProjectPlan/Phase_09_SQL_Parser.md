# Phase 9: SQL Parser

## Objective
Implement SQL parser for DDL and DML statements.

## Prerequisites
- Phase 8 complete (catalog system)

## Technical Specifications
- **Complete BNF/EBNF Grammar**: See `/references/technical_specifications/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- **SQL Dialect**: See `/references/technical_specifications/SCRATCHBIRD_SQL_DIALECT_COMPLETE.md`
- **Parser Implementation**: See `/references/technical_specifications/POSTGRESQL_PARSER_IMPLEMENTATION.md`

## Tasks

### 9.1 Lexer Implementation
Token types:
- Keywords: SELECT, INSERT, UPDATE, DELETE, CREATE, DROP
- Identifiers: table names, column names
- Literals: strings, numbers, NULL
- Operators: =, <, >, <=, >=, <>
- Delimiters: (, ), ,, ;

### 9.2 Parser Grammar
Support statements:
```sql
CREATE TABLE name (col1 type, col2 type, ...)
DROP TABLE name
INSERT INTO name VALUES (val1, val2, ...)
SELECT col1, col2 FROM name WHERE condition
UPDATE name SET col=val WHERE condition
DELETE FROM name WHERE condition
```

### 9.3 AST Generation
```cpp
struct ASTNode {
    NodeType type;
    vector<unique_ptr<ASTNode>> children;
    variant<string, int64_t, double> value;
};
```

### 9.4 Data Types
- INTEGER
- TEXT
- REAL
- BOOLEAN
- DATE

## Files to Create/Modify
- `include/scratchbird/parser/lexer.h`
- `include/scratchbird/parser/parser.h`
- `src/parser/lexer.cpp`
- `src/parser/parser.cpp`

## Validation Tests
```cpp
// Parse CREATE TABLE
auto ast = parse("CREATE TABLE users (id INTEGER, name TEXT)");
assert(ast->type == NodeType::CreateTable);
assert(ast->children.size() == 2);  // Two columns

// Parse SELECT
auto ast2 = parse("SELECT * FROM users WHERE id = 1");
assert(ast2->type == NodeType::Select);

// Parse errors
auto result = parse("INVALID SQL");
assert(result.error_code != 0);
```

## Exit Criteria
- Basic SQL statements parsed correctly
- Syntax errors detected and reported
- AST suitable for execution