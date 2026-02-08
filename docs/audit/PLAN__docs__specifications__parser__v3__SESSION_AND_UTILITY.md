# Implementation Plan: SESSION_AND_UTILITY.md

**Spec Path:** `docs/specifications/parser/v3/SESSION_AND_UTILITY.md`

**Category:** session

## Scope Summary
- Implement parsing, SBLR emission, and executor semantics.

## Dependencies
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_*`

## Implementation Steps (Detailed)
- Define authoritative grammar for SET/SHOW/RESET/EXPLAIN/ANALYZE/CONNECT
- Define AST schemas and SBLR emission for each session/utility command
- Define executor semantics for session variable changes and visibility
- Define SHOW output schemas and catalog queries
- Define error/SQLSTATE mapping for unsupported/invalid commands
- Define security/permission checks for admin-level commands

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only and references v2 parser; no V3 SBLR mapping
- No executor semantics for session state changes or SHOW outputs
- No permission model for SET/SHOW/CONNECT/DISCONNECT
- No formal output schemas for SHOW/EXPLAIN/ANALYZE

## Verification
- Parser tests for statement variants.
- Executor/runtime conformance tests.
