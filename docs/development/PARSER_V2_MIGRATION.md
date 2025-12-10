# Parser V2 Migration Guide

## Overview

ScratchBird supports two SQL parser implementations:

- **Parser V1**: Original parser with deferred semantic validation
- **Parser V2**: New parser with full semantic analysis at compile time

This document describes how to select between parsers and the migration path.

## Parser Version Selection

### SQL Command

Use the `SET PARSER VERSION` command to switch parser versions at runtime:

```sql
-- Switch to Parser V2
SET PARSER VERSION V2;

-- Switch back to Parser V1
SET PARSER VERSION V1;

-- Numeric forms also work
SET PARSER VERSION 2;
SET PARSER VERSION 1;
```

The command returns confirmation of the new parser version.

### Server Session API

For programmatic control:

```cpp
#include "scratchbird/server/server_session.h"

// Set parser version
session.setParserVersion(ParserVersion::V2);

// Check current version
ParserVersion current = session.parserVersion();
```

### ConnectionContext API

The parser version is also accessible via the connection context:

```cpp
#include "scratchbird/core/connection_context.h"

// Set parser version (1 = V1, 2 = V2)
ctx.set_parser_version(2);

// Get current version
uint8_t version = ctx.parser_version();
```

## Key Differences

### Semantic Validation Timing

| Aspect | Parser V1 | Parser V2 |
|--------|-----------|-----------|
| Table validation | At execution time | At compile time |
| Column validation | At execution time | At compile time |
| Type checking | Partial | Full |
| Error messages | Runtime errors | Compile-time errors with source locations |

### Supported Features

Both parsers support:
- SELECT with constants, arithmetic, CASE, CAST
- CREATE TABLE (new tables)
- COMMIT, ROLLBACK
- CREATE VIEW (with constant queries)
- Basic DDL statements

Parser V2 provides stricter validation:
- INSERT/UPDATE/DELETE require table to exist
- CREATE INDEX requires table to exist
- ALTER TABLE requires table to exist
- DROP (without IF EXISTS) checks object existence

### V1 Limitations

The following are documented V1 limitations that V2 addresses:
- No CONCAT function (use `||` operator)
- Limited IF NOT EXISTS support
- No unary NOT in some expression contexts
- Limited TEMPORARY table support

## Migration Strategy

### Phase 1: Parallel Operation (Current)

Both parsers are available. Default is V1 for backward compatibility.

1. Test your SQL against V2 using `SET PARSER VERSION V2`
2. Fix any semantic errors (usually missing tables/views)
3. Switch individual sessions to V2 as validated

### Phase 2: V2 Default (Future)

In a future release, V2 will become the default:
- Applications depending on V1 behavior should explicitly set V1
- New applications should use V2 exclusively

### Phase 3: V1 Deprecation (Future)

V1 will be deprecated and eventually removed:
- Migration window will be announced
- V1 will log deprecation warnings
- Final release will remove V1 code

## Testing Parser Parity

The test suite includes parity tests in `tests/unit/test_parser_v2_parity.cpp`:

```bash
# Run parity tests
./tests/scratchbird_tests --gtest_filter="ParserParity*"
```

This verifies:
- Features that work in both parsers
- Expected V2 semantic strictness
- V1 known limitations

## Bytecode Compatibility

Both parsers generate compatible bytecode. The executor works identically regardless of which parser was used to compile the SQL.

## Performance

Parser V2 has a slightly higher compile-time cost due to semantic analysis, but provides:
- Better error messages
- Potential for query plan caching
- Type information for optimization

The benchmark tests in `tests/benchmark/test_parser_v2_benchmark.cpp` measure relative performance.

## Troubleshooting

### "Table not found" with V2

V2 validates table existence at compile time. Ensure:
1. The table exists in the database
2. You have permission to access the table
3. The schema path is correct

### Different behavior between V1 and V2

Check the parity tests for documented differences. V2 is intentionally stricter to catch errors earlier.

### Switching versions mid-transaction

Parser version can be changed mid-transaction. The change affects subsequent queries only.

## Files and Components

- `include/scratchbird/parser/parser_v2.h` - Parser V2 header
- `src/parser/parser_v2.cpp` - Parser V2 implementation
- `include/scratchbird/sblr/query_compiler_v2.h` - V2 compilation pipeline
- `src/sblr/query_compiler_v2.cpp` - V2 compiler implementation
- `tests/unit/test_parser_v2_ddl.cpp` - V2 DDL tests
- `tests/unit/test_parser_v2_parity.cpp` - Parity tests
- `tests/benchmark/test_parser_v2_benchmark.cpp` - Performance benchmarks
