# Implementation Plan: SELECT_AND_QUERY.md

**Spec Path:** `docs/specifications/parser/v3/SELECT_AND_QUERY.md`

**Category:** query

## Scope Summary
- Implement parsing, SBLR emission, and executor semantics.

## Dependencies
- `docs/specifications/parser/v3/JOINS.md`
- `docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md`

## Implementation Steps (Detailed)
- Define authoritative SELECT grammar and AST schema
- Define CTE and set operation semantics and SBLR emission
- Define executor semantics for DISTINCT/ORDER/LIMIT/FETCH
- Define FOR UPDATE/SHARE locking semantics and OF table_list handling
- Define error/SQLSTATE mapping for invalid query constructs

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only and references v2 parser; no V3 grammar or SBLR mapping
- No executor semantics for ORDER/LIMIT/FETCH or set operations
- FOR UPDATE/SHARE OF table_list is parsed but ignored
- No lock ordering rules for SELECT with row locks

## Verification
- Parser tests for statement variants.
- Executor/runtime conformance tests.
