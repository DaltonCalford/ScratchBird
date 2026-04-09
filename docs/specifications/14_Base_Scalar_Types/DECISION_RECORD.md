# Section 14 Decision Record

## Current authoritative decisions

Shared type authority is centralized in `types.h` and consumed through `TypeInfo` and `TypedValue`.

Plain-value serialization is active and is routed through `TypeSerializer` into `TypedValue::serializePlainValue(...)` and `TypedValue::deserializePlainValue(...)`.

Emulated type resolution is a runtime feature, not parser-only prose. Its current authority is `TypeSystem`.

Protocol or external type-code mappings are authoritative only for the engines and wire layers implemented in `type_mapping.cpp`.

Read-only accessor truth is `EXTRACT(...)` plus composite-record field extraction. It is not universal scalar member access.

Money conversion and text formatting remain implementation-first and are defined by current `TypedValue` behavior, not by older stricter prose.

## Narrowed interpretations

The shared `DataType` enum spans scalar, scalar-adjacent, and complex families. Section `14` only claims the current audited scalar and scalar-adjacent contract.

Type mapping rows prove mapping and mutation-boundary truth. They do not automatically prove full semantic parity with every donor engine.

The existence of `ALTER_ELEMENT` does not widen this file into a write-semantic accessor section.

## Ruled-out claims

This section does not authorize universal scalar dot-member syntax.

This section does not authorize full semantic parity with every historical donor-engine matrix row.

This section does not authorize every `DataType` enum member as a fully proven base scalar family.
