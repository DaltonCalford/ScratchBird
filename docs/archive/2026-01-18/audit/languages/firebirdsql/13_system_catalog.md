# FirebirdSQL - System Catalog Surface

Spec refs:
- `ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md`
- `ScratchBird/docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md`

## Catalog namespaces (expected)
- `RDB$*` system tables
- `MON$*` monitoring tables
- `SEC$*` security tables

## Implementation status
Status: Partial.

Notes:
- Emulated Firebird catalogs are implemented as views over ScratchBird
  metadata; coverage is still under parity audit.
- `FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md` lists remaining RDB$/MON$/SEC$
  gaps.
