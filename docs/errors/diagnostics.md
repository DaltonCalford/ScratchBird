[Errors and Diagnostics Index](index.md)

## Diagnostics and Warnings

This document lists diagnostics strings emitted by parsers and performance systems, with locations for reference.

### Performance Diagnostics

Implementation References:
- `src/engine/performance_config.cpp`

Representative messages (threshold-driven):
- "CRITICAL: CPU usage is extremely high (<percent>%)"
- "WARNING: CPU usage is high (<percent>%)"
- "CRITICAL: Memory usage is extremely high (<percent>%)"
- "WARNING: Memory usage is high (<percent>%)"
- "CRITICAL: Buffer hit ratio is very low (<ratio>%)"
- "WARNING: Buffer hit ratio is low (<ratio>%)"
- "CRITICAL: Average query time is very high (<ms> ms)"
- "WARNING: Average query time is high (<ms> ms)"
- "INFO: All performance metrics are within normal ranges"

### DDL Parser Diagnostics

Implementation References:
- `src/engine/parser_ddl.cpp`

Canonical warnings:
- "ALTER TABLE operations parentheses unbalanced; recovered"
- "ALTER COLUMN TYPE missing type"
- "Column '<name>': VIRTUAL modifier noted for GENERATED AS; treated as computed"
- "IDENTITY options missing parentheses"
- "FK on update without action"
- "FK on delete without action"
- "CREATE TABLE columns/constraints malformed; recovered"

### DML Parser Diagnostics

Implementation References:
- `src/engine/parser_dml.cpp`

Canonical warnings:
- "UPSERT MATCHING column not in INSERT column list"

### Trigger Diagnostics

Implementation References:
- `src/engine/trigger_engine.cpp`

Behavior:
- Emits: "[TRIGGER] RAISE: <message> (SQLSTATE: <code>)" to stderr before throwing.


## Related
- [Error Codes and Results](error-codes.md)
- [Errors and Diagnostics Index](index.md)
- [ScratchBird Analysis Documentation](../analysis/index.md)
