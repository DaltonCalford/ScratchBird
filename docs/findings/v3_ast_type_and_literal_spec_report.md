# V3 AST Type and Literal Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/AST_TYPE_AND_LITERAL_SPEC.md`

## Summary
- Document is labeled **non-authoritative** but internally marked “Authoritative (V3)”; this conflicts with inventory status.
- Current V3 AST does **not** implement the `TypeSpec`/`ValueSpec` model described; literal expression classes exist and partially align with the spec’s literal shapes.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative (internal “Authoritative (V3)” label conflicts).

## Implementation Check

### TypeSpec / TypeKind Model
[ ] Spec’s `TypeSpec` structure (kind/precision/scale/element_type/catalog_id/flags/etc.) is **not present** in AST.
[ ] `TypeKind` enum in AST is a domain-type enum (`ENUM/RECORD/RANGE/BASE/SHELL`), not the spec’s expanded type list.

Key references:
- `include/scratchbird/parser/ast_v3.h:306-343` (TypeName used for types)
- `include/scratchbird/parser/ast_v3.h:491-499` (TypeKind enum)

### ValueSpec
[ ] Spec’s `ValueSpec` wrapper is **not present**; AST literals are modeled as `Expression` subclasses without an explicit typed `ValueSpec` container.

Key references:
- `include/scratchbird/parser/ast_v3.h:2661-2687` (LiteralExpr)

### Literal Nodes
[~] Most literal-specific AST classes exist (ENUM, SET, ROW, COMPOSITE, DOMAIN, BIT, YEAR, DATETIME, MEDIUMINT, GEOMETRY, JSONPATH, INT8/16/128, UINT8/16/32/64/128, FLOAT32, TIME_TZ, TIMESTAMP_TZ, RANGE, ARRAY, VARIANT, TSVECTOR, TSQUERY, BLOB_LOCATOR).
[~] Fields are broadly similar but use `U128` for catalog IDs and `TypeName` for range/array element types instead of spec `TypeSpec`.

Key references:
- `include/scratchbird/parser/ast_v3.h:2691-2952` (Literal*Expr classes)

## Notes
- Spec’s requirement that AST nodes carry catalog UUIDs for catalog-backed types is only partially represented (U128 fields exist on some literal nodes, but type nodes do not carry catalog IDs).
- SBLR emission mappings in the spec were not verified in this pass.
