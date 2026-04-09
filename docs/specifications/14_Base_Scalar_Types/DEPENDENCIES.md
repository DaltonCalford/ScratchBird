# Section 14 Dependencies

## Purpose

This file defines the authoritative dependency contract for section `14`.

## Primary implementation dependencies

`types.h` owns `DataType`, `CastFormat`, `EmulatedStorageKind`, `EmulatedTypeMapping`, and `TypeInfo`.

`typed_value.h` and its implementation own the runtime carrier and the plain-value serialization and conversion branches consumed by this section.

`type_serialization.h` and `type_serialization.cpp` own the serializer API surface, dispatch, and minimum-size admission gates.

`type_system.cpp` owns emulated type resolution, whole-value update rules, element-level mutation rules, and TOAST-eligibility checks.

`type_mapping.cpp` owns PostgreSQL, MySQL, Firebird, and SBWP type-code mappings.

`expression_evaluator.cpp`, `extract_element_catalog.cpp`, `extract_element_ops.cpp`, and `opcodes.h` own the `EXTRACT(...)` evaluation and selector inventory consumed by this section.

`domain_manager.cpp` owns composite-record field extraction by name.

## Cross-section dependencies

Section `05` owns general persisted page and payload framing rules used by serialized values.

Section `11` owns TOAST and oversized-value behavior for TOAST-eligible types.

Section `13` owns cast and coercion policy outside the direct scalar runtime this section documents.

Section `22` owns the SBLR expression model and opcode surface that carry `EXTRACT(...)` semantics into execution.

Section `15` owns complex-type families and container-heavy surfaces beyond the directly audited scalar contract here.

Section `18` owns family-local index and operator behavior that must not be relabeled as section-wide scalar support.

## Dependency rule

Parser, connector, or legacy prose does not become authoritative for section `14` unless it resolves through the runtime surfaces named in this file.
