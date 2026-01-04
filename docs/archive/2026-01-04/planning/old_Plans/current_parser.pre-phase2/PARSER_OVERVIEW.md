# ScratchBird Parser Overview

**Last Updated:** December 4, 2025

## Architecture

ScratchBird uses a hand-written recursive descent parser with a stateful lexer. This approach provides:
- Full control over error messages and recovery
- Easy extension for new syntax
- No external dependencies (flex/bison)

## Parser Files

### Source Files (`src/parser/`)

| File | Purpose |
|------|---------|
| `parser.cpp` | Main recursive descent parser implementation |
| `lexer.cpp` | SQL tokenizer/lexical analyzer |
| `token.cpp` | Token type definitions and utilities |
| `ast.cpp` | Abstract Syntax Tree node implementations |
| `semantic_analyzer.cpp` | Type checking and name resolution |
| `symbol_table.cpp` | Symbol table management for identifiers |
| `expression_to_string.cpp` | Expression serialization for EXPLAIN |

### Header Files (`include/scratchbird/parser/`)

| File | Purpose |
|------|---------|
| `parser.h` | Parser class and `parseStatement()` public interface |
| `lexer.h` | Lexer class, ErrorReporter interface, StringPool |
| `token.h` | TokenType enum, Token struct, SourceLocation |
| `ast.h` | Complete AST node definitions (65+ classes) |
| `semantic_analyzer.h` | SemanticAnalyzer class for validation |
| `symbol_table.h` | Symbol table and scope management |
| `expression_to_string.h` | Expression-to-string conversion utilities |

## Parser Pipeline

```
SQL Text → Lexer → Token Stream → Parser → AST → Semantic Analyzer → Validated AST
                                                                          ↓
                                                              Bytecode Generator
```

## Key Classes

### Lexer
- **State machine**: INITIAL, IN_IDENTIFIER, IN_NUMBER, IN_STRING, IN_COMMENT_LINE, IN_COMMENT_BLOCK
- **String pooling**: Via `StringPool::intern()` to deduplicate strings
- **Keyword detection**: `checkKeyword()` maps identifiers to keywords
- **Operator scanning**: `scanOperator()` handles multi-character operators

### Parser
- **Lookahead**: 1 token (`current_token_`)
- **Entry point**: `parseStatement()`
- **Expression precedence**: parseExpression() → parseOr() → parseAnd() → parseComparison() → parseTerm() → parseFactor() → parsePrimary()

### SemanticAnalyzer
- **Name resolution**: Validates table and column references
- **Type checking**: Ensures type compatibility in expressions
- **Scope management**: Handles nested scopes (subqueries, CTEs)

## Statistics

| Category | Count |
|----------|-------|
| Token Types | 150+ |
| AST Node Kinds | 90+ |
| Statement Types | 60+ |
| Expression Types | 30+ |
| Binary Operators | 20+ |
| Aggregate Functions | 6 |
| Window Functions | 10+ |
| Data Types | 30+ |
| JOIN Types | 5 |

## Related Documentation

- [SQL_COMMANDS.md](SQL_COMMANDS.md) - Complete SQL command reference
- [TOKEN_REFERENCE.md](TOKEN_REFERENCE.md) - Token type definitions
- [AST_NODES.md](AST_NODES.md) - AST node class hierarchy
- [DATABASE_COMPARISON.md](DATABASE_COMPARISON.md) - Feature comparison with other databases
