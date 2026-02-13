# SBLR Type Opcode Remediation Plan (Alpha)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Status: In Progress (tests pending)
Last Updated: 2026-02-02

## Purpose
Provide a tracked plan to ensure **full SBLR/executor type coverage** so all
native and emulated-engine datatypes can be encoded, decoded, and executed.

## Inputs
- `docs/findings/SBLR_TYPE_OPCODE_GAPS.md`
- `include/scratchbird/core/types.h`
- `include/scratchbird/sblr/opcodes.h`
- `src/sblr/executor.cpp`

## Scope
Alpha scope: add SBLR type markers and typed literals for all native DataTypes,
plus any **additional** DataTypes required for Firebird/PostgreSQL/MySQL parity.

## Emulated-Engine Type Requirements
These types are required by emulated engines and must be supported by SBLR:

| Type | Required By | DataType Enum | SBLR Type Marker | Literal Opcode |
| --- | --- | --- | --- | --- |
| JSONB | PostgreSQL | Exists | Missing | Missing |
| XML | PostgreSQL | Exists | Missing | Missing |
| INET/CIDR/MACADDR/MACADDR8 | PostgreSQL | Exists | Missing | Missing |
| INTERVAL | PostgreSQL | Exists | Missing | Missing |
| MONEY | PostgreSQL | Exists | Missing | Missing |
| COMPOSITE | PostgreSQL | Exists | Missing | Missing |
| MULTI* geometry / GEOMETRYCOLLECTION | MySQL/PostgreSQL | Exists | Missing | Missing |
| TIME WITH TIME ZONE | Firebird/PostgreSQL | Partial (flag only) | Missing | Missing |
| TIMESTAMP WITH TIME ZONE | Firebird/PostgreSQL | Partial (flag only) | Missing | Missing |
| DECFLOAT(16/34) | Firebird | **Missing** | Missing | Missing |

## Work Plan

### 1) Opcode Registry + Spec Alignment
- Define base vs extended type markers for all missing DataTypes.
- Assign new literal opcodes for typed constants.
- Update:
  - `include/scratchbird/sblr/opcodes.h`
  - `/docs/specifications/parser/v3/sblr/SBLR_OPCODE_REGISTRY.md`
  - `/docs/specifications/parser/v3/sblr/Appendix_A_SBLR_BYTECODE.md`

### 2) Type System Extensions
- Add missing DataTypes for emulated-engine parity (e.g., `DECFLOAT16/DECFLOAT34`).
- Define timezone-aware temporal encodings (TIME/TIMESTAMP WITH TZ) in TypeInfo
  and SBLR type payloads.
- Extend `TypedValue`/serialization to cover new types and their canonical
  encodings.

### 3) Parser + SBLR Emitter
- Emit new type markers in CREATE/ALTER/CAST paths.
- Emit typed literal opcodes where applicable (date/time/uuid/json/etc.).

### 4) Executor Decode + Runtime Support
- Extend `convertDataType`/`convertExtendedDataType`.
- Add literal opcode parsing and value constructors.
- Update casting, comparison, and expression evaluation for new types.

### 5) Tests + Conformance
- Add SBLR bytecode round-trip tests for all new type markers.
- Add literal parsing tests for every new literal opcode.
- Add minimal DDL/DML coverage for each newly supported type.

## Implementation Checklist
- [x] Assign opcodes for missing types in `opcodes.h`
- [x] Add literal opcodes for typed constants
- [x] Update SBLR registry + Appendix A to match header
- [x] Add DECFLOAT + timezone-aware temporal encodings
- [x] Extend TypedValue + serialization for new types
- [x] Update parser/emitter to output new markers + literals
- [x] Update executor decode + literal handling
- [ ] Add SBLR unit tests + DDL/DML coverage

## Acceptance Criteria
- All DataTypes in `core/types.h` have SBLR type markers.
- All emulated-engine-required types have a full encode/decode path.
- Executor can parse typed literals for every supported type.
- SBLR bytecode round-trip tests pass for the full type matrix.
