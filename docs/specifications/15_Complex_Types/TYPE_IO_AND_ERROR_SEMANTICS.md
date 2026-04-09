# Type I/O and Error Semantics

Status: current_authority

## Current authoritative runtime surface

The current code-backed I/O and error surface for section 15 is split across:
- TypedValue conversion and plain-value serialization and deserialization
- extract_element_ops selector execution for complex families
- DomainManager domain-kind creation, extraction, set algebra, and validation rules

## Complex I/O and selector capability matrix

| Area | Current authority | Main boundary |
| --- | --- | --- |
| ARRAY selectors | supported with fail-closed negative paths for CARDINALITY, NDIMS, DIMS, LOWER, UPPER, ELEMENT, LENGTH, and VALUE | selector-by-selector gate closure still needs dedicated tests |
| COMPOSITE selectors | supported with fail-closed negative paths for FIELD, FIELD_NAMES, DATATYPE, and VALUE | field-mismatch matrix still needs tighter dedicated tests |
| VARIANT selectors | supported with fail-closed negative paths for TYPE, DATATYPE, VALUE, FIELD, FIELD_NAMES, and ELEMENT | broader parity remains unproven |
| JSON selectors | bounded runtime support with fail-closed negative paths | broader JSON parity is not current proof |
| XML selectors | bounded runtime support with fail-closed negative paths | broader XML parity is not current proof |
| record-domain extraction | bounded support through DomainManager extractField | broader field-access syntax is not current proof |
| set helper operations | bounded runtime helper truth | not universal set-language parity |

## Direct fail-closed behavior

- ARRAY and LIST serialization require homogeneous non-null element types
- MAP shares the array-like carrier path and remains bounded rather than fully closed as a standalone key/value payload family
- deserialization fails closed on invalid array headers, invalid dimension counts, invalid null bitmaps, oversized element counts, missing element type, and element payload overruns
- composite and row deserialization fail closed on invalid column counts, invalid field headers, invalid metadata flags, and field payload overruns
- VARIANT fails closed when payload count is not exactly one or explicit tag metadata is invalid
- TAGGED_UNION fails closed on undersized payloads, invalid tags, invalid length encodings, and tag or type mismatches during conversion
- DICT_ENCODED fails closed on invalid payload size, malformed payloads, and missing dictionary keys during conversion

## Fail-closed areas

This section does not promote older prose into proof for:
- full SQL literal grammars for every complex family
- standalone streaming LOB API behavior
- complete text I/O semantics for every emulated complex type across all donor engines
