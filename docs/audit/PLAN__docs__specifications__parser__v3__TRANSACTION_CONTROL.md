# Implementation Plan: TRANSACTION_CONTROL.md

**Spec Path:** `docs/specifications/parser/v3/TRANSACTION_CONTROL.md`

**Category:** transaction

## Scope Summary
- Implement parsing, SBLR emission, and executor semantics.

## Dependencies
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_*`

## Implementation Steps (Detailed)
- Define SBLR mapping for all transaction control statements
- Define executor semantics for isolation levels and MGA modes
- Define savepoint creation/release/rollback semantics
- Define error/SQLSTATE mapping for invalid transaction state changes
- Define lock and resource management for transaction boundaries

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only and references v2 parser; no V3 SBLR mapping
- Isolation characteristics are listed without MGA mapping
- No executor semantics for commit/rollback/prepare
- No error/SQLSTATE mapping for transaction state violations

## Verification
- Parser tests for statement variants.
- Executor/runtime conformance tests.
