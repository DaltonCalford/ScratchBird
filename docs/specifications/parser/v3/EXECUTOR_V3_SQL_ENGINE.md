# V3 Executor Specification: SQL Engine
Date: 2026-02-08
Status: Authoritative (V3)

This document defines the SQL execution semantics used by the V3 parser output. It specifies planning/execution stages, operator behavior, and integration with SBLR.

## 1. Scope and Goals
- Define the SQL execution pipeline from AST to SBLR to runtime.
- Specify operator semantics for SELECT/INSERT/UPDATE/DELETE/MERGE and utility statements.
- Specify transaction, MVCC, and catalog integration.

## 2. Execution Pipeline
1. Parse SQL to AST (V3 AST specs).
2. Resolve names and types; bind catalog IDs.
3. Build logical plan and physical plan.
4. Emit SBLR V3 bytecode.
5. Execute via SBLR VM.

## 3. Query Semantics
- **SELECT**: projection, filtering, grouping, windowing, ordering, limits.
- **INSERT**: values, default handling, domain enforcement.
- **UPDATE**: row selection, set expressions, domain enforcement.
- **DELETE**: row selection, cascade handling.
- **MERGE**: matched/unmatched branch semantics.

## 4. Joins
- INNER, LEFT, RIGHT, FULL, CROSS, SEMI, ANTI.
- Join order determined by optimizer; semantic equivalence required.

## 5. Aggregation
- GROUP BY with HAVING.
- Ordered aggregates and DISTINCT aggregates.
- Aggregate functions map to SBLR3_AGG_* opcodes.

## 6. Window Functions
- Window definitions mapped to SBLR3_WIN_* opcodes.
- Partition, order, frame bounds semantics follow SQL standard.

## 7. Transaction Semantics
- MVCC snapshot isolation by default.
- Transaction opcodes defined in `SBLR_V3_OPCODE_SPEC.md`.
- Autocommit handling by session or explicit control.

## 8. Domain Enforcement in SQL
- INSERT/UPDATE must validate domain constraints before table constraints.
- Domain security and audit actions execute as part of write pipeline.

## 9. Catalog Integration
- All catalog IDs are UUID v7.
- Execution must resolve object IDs at compile time.

## 10. Utility and Session Statements
- COPY, SET, SHOW, BEGIN/COMMIT/ROLLBACK, SAVEPOINT.
- PSQL blocks and stored procedures executed via SBLR.

## 11. Required Tests
- End-to-end statement coverage for core SQL statements.
- Regression tests for joins, aggregation, windows.
- Transaction visibility and MVCC tests.
