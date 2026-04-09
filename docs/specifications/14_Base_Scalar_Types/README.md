# 14 Base Scalar Types

This section is authoritative for ScratchBird's current scalar and scalar-adjacent runtime type contract.

The primary authority surfaces are `types.h`, `TypedValue`, `TypeSerializer`, `TypeSystem`, the UDR type-mapping layer, and the SBLR `EXTRACT(...)` runtime.

## Current implemented truth

Shared type identity is centralized in `DataType`, `TypeInfo`, `CastFormat`, and `EmulatedTypeMapping`.

Runtime scalar values are carried through `TypedValue`.

Plain-value serialization and deserialization are active and routed through `TypeSerializer` and `TypedValue`.

Emulated type resolution and mutation-boundary rules are active in `TypeSystem`.

Protocol or external mapping surfaces are active for PostgreSQL, MySQL, Firebird, and SBWP.

Read-only accessor truth is centered on `EXTRACT(...)` and composite-record field extraction, not on universal scalar dot-member syntax.

## Primary implementation surfaces

`scratchbird/core/types.h`

`scratchbird/core/typed_value.h`

`scratchbird/core/type_serialization.h`

`src/core/type_serialization.cpp`

`src/core/type_system.cpp`

`src/udr/type_mapping.cpp`

`src/sblr/expression_evaluator.cpp`

`src/sblr/extract_element_catalog.cpp`

`src/sblr/extract_element_ops.cpp`

`src/core/domain_manager.cpp`

## Section boundary

Section `14` owns the current scalar-family runtime contract, serializer admission rules, conversion and error vocabulary, audited emulated type mappings, and read-only accessor behavior.

Section `14` does not claim full ownership of every complex or family-local type carried by the shared `DataType` enum.

Section `14` does not authorize universal semantic parity with every donor or dialect named in historical prose.

## Direct audit lookup anchors

- `src/core/type_serialization.cpp` search key `TypeSerializer::serialize(`
- `src/core/type_system.cpp` search key `TypeSystem::resolveEmulatedType(`
- `src/sblr/extract_element_ops.cpp` search key `extractElement(`

<!-- AUTO-GENERATED:FILE-LIST:START -->
- [BETA2_SCALAR_TEMPORAL_TEXT_AND_IDENTIFIER_TYPE_EXPANSION_MODEL.md](BETA2_SCALAR_TEMPORAL_TEXT_AND_IDENTIFIER_TYPE_EXPANSION_MODEL.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [EMBEDDED_FIELD_ACCESSORS.md](EMBEDDED_FIELD_ACCESSORS.md)
- [EMULATED_SCALAR_TYPE_MATRIX.md](EMULATED_SCALAR_TYPE_MATRIX.md)
- [SCALAR_STORAGE_FORMAT.md](SCALAR_STORAGE_FORMAT.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
- [TYPE_IO_AND_ERROR_SEMANTICS.md](TYPE_IO_AND_ERROR_SEMANTICS.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
