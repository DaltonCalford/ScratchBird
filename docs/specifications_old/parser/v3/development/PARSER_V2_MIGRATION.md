# Parser V2 Guide

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


## Overview

ScratchBird uses a single SQL parser implementation: **Parser V2**. Parser V1 has been removed.

Parser V2 performs full semantic analysis at compile time and is the only supported path into SBLR.

## Parser Selection

Parser selection is no longer configurable. All compilation uses Parser V2. Any legacy parser-version
settings are ignored and coerced to V2.

## Key Characteristics

- Full semantic validation at compile time (tables, columns, types).
- Consistent error reporting with source locations.
- Inputs compile to SBLR bytecode for execution by the engine.

## Supported Features

Parser V2 supports:
- SELECT with constants, arithmetic, CASE, CAST
- CREATE TABLE, CREATE INDEX, CREATE VIEW
- INSERT, UPDATE, DELETE
- COMMIT, ROLLBACK
- Basic DDL statements

Parser V2 enforces:
- Table existence for DML/DDL that references tables
- Column existence and type checking

## Testing

Parser V2 coverage lives in:
- `tests/unit/test_parser_v2_ddl.cpp`
- `tests/unit/test_parser_dml_v2.cpp`
- `tests/unit/test_query_compiler_v2.cpp`

## Bytecode and Execution

Parser V2 → Semantic Analyzer V2 → Bytecode Generator V2 → SBLR Executor

The executor runs SBLR regardless of which parser produced it (ScratchBird native or emulated).

## Performance

Parser V2 has a slightly higher compile-time cost due to semantic analysis, but enables:
- Better error messages
- Query plan caching
- Type-driven optimizations

## Troubleshooting

### "Table not found"

Parser V2 validates table existence at compile time. Ensure:
1. The table exists in the database
2. You have permission to access the table
3. The schema path is correct

## Files and Components

- `include/scratchbird/parser/parser_v2.h` - Parser V2 header
- `src/parser/parser_v2.cpp` - Parser V2 implementation
- `include/scratchbird/sblr/query_compiler_v2.h` - V2 compilation pipeline
- `src/sblr/query_compiler_v2.cpp` - V2 compiler implementation
