# Implementation Plan: PSQL_RUNTIME_V3.md

**Spec Path:** `docs/specifications/parser/v3/PSQL_RUNTIME_V3.md`

**Category:** psql

## Scope Summary
- Implement PSQL runtime semantics in the SBLR executor.

## Dependencies
- `docs/specifications/parser/v3/EXECUTOR_V3_SBLR.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`
- `docs/specifications/parser/v3/PSQL_STATEMENTS.md`

## Implementation Steps (Detailed)
- Define PSQL runtime VM frame layout (locals, params, temporaries, handler frames)
- Define opcode mapping for PSQL control flow and variable ops
- Define cursor implementation (binding, plan caching, fetch protocol)
- Define sub‑savepoint creation and rollback mechanics for PSQL statements
- Define exception object structure (SQLSTATE, message, stack context)
- Define rules for dynamic SQL execution and parameter binding
- Define deterministic evaluation order for expressions inside PSQL
- Define integration with transaction manager and lock ordering
- Define error/SQLSTATE mapping for runtime PSQL violations

## Manual Gap Analysis (Missing/Unclear Details)
- No explicit VM frame layout or opcode mapping for PSQL operations
- Cursor implementation details (plan caching, row buffer format) are missing
- No explicit savepoint naming/scoping and rollback algorithm
- Exception object structure and propagation payloads are not defined
- No dynamic SQL parameter binding rules

## Verification
- Unit tests for scoping, exceptions, and cursor lifecycle.
- Integration tests for dynamic SQL and savepoint behavior.
