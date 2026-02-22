# Implementation Plan: EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md

**Spec Path:** `docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`

**Category:** executor

## Scope Summary
- Complete per‑opcode lock/GC/constraint rules for all opcode families.

## Dependencies
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`
- `docs/specifications/parser/v3/EXECUTOR_V3_SBLR.md`
- `docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md`

## Implementation Steps (Detailed)
- Enumerate every SBLR opcode family and map to lock/GC/constraint requirements
- Define lock targets and modes for DDL, DCL, PSQL, utility, and maintenance opcodes
- Define GC/visibility rules for catalog operations and system maintenance
- Define constraint enforcement order for all write opcodes (including trigger-induced writes)
- Define error code mappings for all opcode families
- Define deadlock and timeout handling per opcode category
- Cross‑link each opcode to the authoritative spec where it is defined

## Manual Gap Analysis (Missing/Unclear Details)
- Matrix currently covers only a subset of DML/COPY; missing DDL/DCL/PSQL/utility opcodes
- No explicit lock modes for catalog mutations and DDL
- No GC/visibility rules for catalog or maintenance operations
- Error codes are incomplete for non‑DML opcodes
- No mapping to the complete opcode registry

## Verification
- Conformance tests verifying lock ordering and constraint enforcement per opcode.
- Deadlock and timeout tests per opcode family.
