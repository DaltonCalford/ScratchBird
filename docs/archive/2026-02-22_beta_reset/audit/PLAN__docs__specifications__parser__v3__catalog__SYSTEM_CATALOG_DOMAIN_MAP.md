# Implementation Plan: SYSTEM_CATALOG_DOMAIN_MAP.md

**Spec Path:** `docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_DOMAIN_MAP.md`

**Category:** catalog

## Scope Summary
- Implement all normative requirements in this spec.
- Align with SBLR V3, executor contracts, and canonical storage encodings.

## Dependencies
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SEMANTICS.md`
- `docs/specifications/parser/v3/SBLR_V3_VALIDATION_RULES.md`
- `docs/specifications/parser/v3/EXECUTOR_V3_SBLR.md`
- `docs/specifications/parser/v3/storage/PAGE_TYPES_AND_LAYOUTS.md`

## Implementation Steps (Detailed)
- Validate coverage: ensure every catalog table/column has an SBDB$ domain mapping
- Define each SBDB$ domain in a canonical domain registry (name, base type, constraints)
- Resolve array domain definitions (e.g., `ARRAY<SBDB$NAME>[512]`) into explicit domain specs
- Cross-check catalog DDL (`SYSTEM_CATALOG_DDL_SBDB.md`) against this map for drift
- Specify column-level constraints (NOT NULL, DEFAULT, CHECK, FK) using domain names
- Define catalog bootstrap ordering and domain enforcement at CREATE DATABASE
- Document migration/compat policy if catalog schema evolves

## Manual Gap Analysis (Missing/Unclear Details)
- Domain definitions for many SBDB$ names are not present in this file
- Array-based domains are listed but no explicit array domain schema exists here
- Column constraints and defaults are not enumerated; only domain names are listed
- No validation that every catalog table is covered by this mapping
- No linkage to physical catalog storage layouts or page-level metadata

## Verification
- Unit tests for normative semantics and edge cases.
- Conformance tests for dialect and storage compatibility.
- Cross-doc traceability review against authoritative inventory.
