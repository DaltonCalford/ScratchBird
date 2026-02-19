# DDL Management: Object Naming And Identifiers
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../README.md)
- [DDL README](../README.md)
- [Management README](README.md)

## Identifier Validation And Storage

Engine-backed identifier facts:
- UTF-8 identifier length validation uses character count range `1..128` in `UTF8Utils::isValidIdentifierLength(...)` (`src/core/utf8_utils.cpp`).
- Resolver normalization uppercases non-delimited identifiers via `normalizeResolverName(name, name_is_delimited)` (`src/core/catalog_manager.cpp`).
- Delimited (quoted) identifiers keep provided casing; unquoted identifiers are resolved case-insensitively through canonical uppercase normalization.

Catalog/domain storage notes:
- Canonical catalog-name domains such as `[sb_dom]cat_identifier`, `[sb_dom]cat_schema_name`, and `[sb_dom]cat_object_name` are defined as `VARCHAR` with `length_chars=252` in `include/scratchbird/core/system_domain_registry_rows.inc`.
- Practical object-name acceptance is still governed by identifier validation and parser/resolver contracts, not just raw storage width.

## Naming Style Guidance

Supported naming patterns:
- Unquoted: case-insensitive, normalized (recommended for most DDL).
- Quoted: case-preserving, use when case-sensitive or mixed-case object names are required.

Recommended conventions for v3 projects:
- Use one canonical style per schema (`snake_case`, `CamelCase`, or `PascalCase`) and avoid mixed conventions in the same object family.
- Prefer unquoted identifiers unless exact case preservation is a hard requirement.
- Keep object names under UTF-8 128-character logical identifier limit.
