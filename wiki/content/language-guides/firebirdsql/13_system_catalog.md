[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# FirebirdSQL - System Catalog Surface

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

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
