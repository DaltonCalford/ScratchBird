# Section 14 Outline

This section is organized around the currently shipped shared type runtime.

## Canonical files

### `SCALAR_STORAGE_FORMAT.md`

Defines the `TypeSerializer` contract, serializer dispatch rules, and exact minimum-size admission gates.

### `TYPE_IO_AND_ERROR_SEMANTICS.md`

Defines `TypedValue` conversion, parse/format, and named error semantics currently surfaced by the runtime.

### `EMULATED_SCALAR_TYPE_MATRIX.md`

Defines the audited engine/type mapping and mutation-boundary contract currently owned by `TypeSystem` and the UDR mapping layer.

### `EMBEDDED_FIELD_ACCESSORS.md`

Defines read-only accessor truth for `EXTRACT(...)` and composite-field extraction.

### `DEPENDENCIES.md`

Defines section-local code ownership and cross-section dependencies.

### `TEST_CONTRACT.md`

Defines the proof artifacts required for this section.

### `DECISION_RECORD.md`

Defines the section-level decisions that replace the older storage-first and dialect-wide prose.
