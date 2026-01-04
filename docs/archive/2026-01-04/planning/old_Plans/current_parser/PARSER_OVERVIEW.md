# ScratchBird Parser - Implementation Overview

## Document Purpose

This document provides a complete overview of the currently implemented SQL parser in ScratchBird. It is part of a comprehensive audit for rebuilding the parser as a context-sensitive parser with reduced keyword reservation.

## Parser Architecture

### Type: Recursive Descent Parser

The ScratchBird parser is a hand-written recursive descent parser located in:
- **Header**: `include/scratchbird/parser/parser.h`
- **Implementation**: `src/parser/parser.cpp` (~9600 lines)
- **Lexer**: `src/parser/lexer.cpp`
- **Tokens**: `include/scratchbird/parser/token.h`, `src/parser/token.cpp`
- **AST**: `include/scratchbird/parser/ast.h`, `src/parser/ast.cpp`

### Core Components

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Input     │────▶│   Lexer     │────▶│   Parser    │────▶│    AST      │
│   (SQL)     │     │  (Tokens)   │     │ (Recursive) │     │   (Tree)    │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
```

### Key Classes

| Class | File | Purpose |
|-------|------|---------|
| `Parser` | parser.h | Main parser class with recursive descent methods |
| `Lexer` | lexer.h | Tokenizer that converts SQL text to tokens |
| `StringPool` | token.h | Intern pool for identifier strings |
| `Token` | token.h | Individual token with type and value |
| `ASTArena` | ast.h | Arena allocator for AST nodes |
| `ParseResult` | parser.h | Result container with statement and errors |
| `Statement` | ast.h | Base class for all statement AST nodes |
| `Expression` | ast.h | Base class for all expression AST nodes |

## Statement Dispatch

The main entry point is `Parser::parseStatement()` which dispatches based on the first keyword:

### Primary Statement Keywords

| Keyword | Dispatch | Parser Method(s) |
|---------|----------|------------------|
| `CREATE` | Sub-dispatch | See CREATE section |
| `ALTER` | Sub-dispatch | See ALTER section |
| `DROP` | Sub-dispatch | See DROP section |
| `INSERT` | Direct | `parseInsert()` |
| `SELECT` | Direct | `parseSelect()` |
| `WITH` | Direct | `parseSelect()` (handles CTEs) |
| `UPDATE` | Direct | `parseUpdate()` |
| `DELETE` | Direct | `parseDelete()` |
| `MERGE` | Direct | `parseMerge()` |
| `TRUNCATE` | Direct | `parseTruncateTable()` |
| `START` | Direct | `parseStartTransaction()` |
| `SET` | Sub-dispatch | See SET section |
| `RESET` | Sub-dispatch | `parseSetRole()` / `parseSetSessionAuth()` |
| `COMMIT` | Direct | `parseCommit()` |
| `ROLLBACK` | Direct | `parseRollback()` |
| `SAVEPOINT` | Direct | `parseSavepoint()` |
| `RELEASE` | Direct | `parseReleaseSavepoint()` |
| `SWEEP` | Direct | `parseSweep()` |
| `ANALYZE` | Direct | `parseAnalyze()` |
| `EXPLAIN` | Direct | `parseExplain()` |
| `REFRESH` | Direct | `parseRefreshMaterializedView()` |
| `ATTACH` | Sub-dispatch | `parseAttachTablespace()` |
| `DETACH` | Sub-dispatch | `parseDetachTablespace()` |
| `GRANT` | Direct | `parseGrant()` |
| `REVOKE` | Direct | `parseRevoke()` |
| `SHOW` | Direct | `parseShowStatement()` |
| `DESCRIBE`/`DESC` | Direct | `parseDescribeStatement()` |
| `CALL` | Direct | `parseCallStatement()` |

### CREATE Sub-dispatch

After matching `CREATE`, the parser checks the next token:

| Next Token | Parser Method |
|------------|---------------|
| `TABLESPACE` | `parseCreateTablespace()` |
| `TABLE` | `parseCreateTable()` |
| `INDEX` or `UNIQUE` | `parseCreateIndex()` |
| `SEQUENCE` | `parseCreateSequence()` |
| `VIEW` or `OR` or `MATERIALIZED` | `parseCreateView()` |
| `USER` | `parseCreateUser()` |
| `ROLE` | `parseCreateRole()` |
| `GROUP` | `parseCreateGroup()` |
| `POLICY` | `parseCreatePolicy()` |
| `TYPE` | `parseCreateType()` |
| `DOMAIN` | `parseCreateDomain()` |
| `TRIGGER` | `parseCreateTrigger()` |
| `FUNCTION` | `parseCreateFunction()` |
| `PROCEDURE` | `parseCreateProcedure()` |

### ALTER Sub-dispatch

| Next Token | Parser Method |
|------------|---------------|
| `TABLESPACE` | `parseAlterTablespace()` |
| `TABLE` | `parseAlterTable()` |
| `SEQUENCE` | `parseAlterSequence()` |
| `USER` | `parseAlterUser()` |

### DROP Sub-dispatch

| Next Token | Parser Method |
|------------|---------------|
| `TABLE` | `parseDropTable()` |
| `INDEX` | `parseDropIndex()` |
| `TABLESPACE` | `parseDropTablespace()` |
| `SEQUENCE` | `parseDropSequence()` |
| `VIEW` | `parseDropView()` |
| `USER` | `parseDropUser()` |
| `ROLE` | `parseDropRole()` |
| `GROUP` | `parseDropGroup()` |
| `POLICY` | `parseDropPolicy()` |
| `TRIGGER` | `parseDropTrigger()` |

### SET Sub-dispatch

| Next Token | Parser Method |
|------------|---------------|
| `ROLE` | `parseSetRole()` |
| `SESSION` | `parseSetSessionAuth()` |
| `CONSTRAINTS` | `parseSetConstraints()` |
| `SQL` | `parseSetSqlDialect()` |
| `NAMES` | `parseSetNames()` |
| `LOCAL_TIMEOUT` | `parseSetLocalTimeout()` |
| (default) | `parseSetTransaction()` |

## Expression Parsing

Expression parsing uses operator precedence via recursive descent:

```
parseExpression()
    └── parseOr()           (lowest precedence: OR)
        └── parseAnd()      (AND)
            └── parseComparison()  (=, <>, <, >, <=, >=, LIKE, IN, BETWEEN, IS NULL, etc.)
                └── parseTerm()    (+, -, ||)
                    └── parseFactor()  (*, /, %)
                        └── parsePrimary()  (highest precedence: literals, identifiers, functions, etc.)
```

### Operator Precedence (Low to High)

1. `OR`
2. `AND`
3. `NOT` (unary)
4. Comparison: `=`, `<>`, `<`, `>`, `<=`, `>=`
5. `LIKE`, `ILIKE`, `SIMILAR TO`
6. `IN`, `NOT IN`
7. `BETWEEN`, `NOT BETWEEN`
8. `IS NULL`, `IS NOT NULL`, `IS DISTINCT FROM`
9. `+`, `-`, `||` (concatenation)
10. `*`, `/`, `%`
11. Unary `-`, `+`
12. Array subscript `[]`, JSON operators `->`, `->>`
13. Primary: literals, identifiers, function calls, `CAST`, `CASE`, subqueries

## Token Management

The parser maintains two token references:
- `previous_token_`: The most recently consumed token
- `current_token_`: The current lookahead token

### Key Token Methods

| Method | Purpose |
|--------|---------|
| `advance()` | Move to next token |
| `match(TokenType)` | Check and consume if matches |
| `consume(TokenType, msg)` | Require and consume, error if not |
| `check(TokenType)` | Check without consuming |
| `previous()` | Get last consumed token |
| `current()` | Get current lookahead token |
| `isAtEnd()` | Check for EOF |

## Error Handling

### Error Recovery

The parser uses `synchronize()` for error recovery, which advances to the next statement boundary (semicolon or statement keyword).

### Error Reporting

Errors are collected in `errors_` vector and returned via `ParseResult::errors()`.

## Memory Management

All AST nodes are allocated via `ASTArena` which provides:
- Fast bump-pointer allocation
- Automatic cleanup when arena is destroyed
- No individual node deallocation needed

## Contextual Keywords

Some keywords are only reserved in specific contexts. The parser uses helper lambdas like `matchContextual()` to handle these:

### Current Contextual Keywords (in SHOW statements)
- `PATH`
- `TREE`
- `DEPTH`
- `SEARCH`
- `OF`
- `RESOLVED`
- `OBJECTS`
- `DETAIL`
- `HOME`
- `ROOT`
- `UP`

These are lexed as `IDENTIFIER` and matched by string comparison in context.

## File Organization

### Related Documents in This Audit

| Document | Content |
|----------|---------|
| `SET.md` | All SET command variants |
| `SHOW.md` | All SHOW command variants |
| `CREATE.md` | All CREATE command variants |
| `ALTER.md` | All ALTER command variants |
| `DROP.md` | All DROP command variants |
| `DML.md` | INSERT, UPDATE, DELETE, MERGE |
| `SELECT.md` | SELECT with all clauses |
| `GRANT_REVOKE.md` | Security privilege commands |
| `TRANSACTION.md` | Transaction control commands |
| `OTHER_COMMANDS.md` | DESCRIBE, CALL, TRUNCATE, ANALYZE, EXPLAIN, etc. |
| `TOKEN_REFERENCE.md` | Complete token type listing |
| `AST_NODES.md` | Complete AST node listing |

## Parser Statistics

| Metric | Value |
|--------|-------|
| Total Lines (parser.cpp) | ~9,600 |
| Statement Parse Methods | ~50 |
| Expression Parse Methods | ~6 |
| Helper Parse Methods | ~25 |
| Token Types | ~200+ |
| Reserved Keywords | ~180+ |
| AST Statement Types | ~40+ |
| AST Expression Types | ~20+ |

## Known Limitations

1. **No COMMENT statement** - Comments on objects not yet implemented
2. **No ALTER INDEX** - Index modification not supported
3. **No ALTER VIEW** - View modification not supported (use OR REPLACE)
4. **Limited ALTER TABLE** - Some operations not yet implemented

## Recommendations for Context-Sensitive Rewrite

1. **Reduce Reserved Keywords**: Most keywords should only be reserved where contextually required
2. **State Machine Approach**: Each command type should have its own parsing state
3. **Keyword Categories**:
   - Truly reserved (SELECT, FROM, WHERE, etc.)
   - Command-specific (PATH in SHOW, LOCATION in TABLESPACE)
   - Never reserved (common column names like NAME, TYPE, STATUS)
4. **Lookahead Optimization**: Use lookahead to disambiguate constructs
5. **Modular Design**: One module per command family for maintainability
