# EXTRACT/ALTER_ELEMENT Correction Plan Checklist

Purpose: Track implementation work required to add ALTER_ELEMENT and the expanded EXTRACT element catalog defined in `ScratchBird/docs/specifications/EXTRACT_AND_ALTER_ELEMENT.md`.

Status: Implemented; checklist complete and docs/tests updated.

## Parser and grammar
- [x] Add `ALTER_ELEMENT` as a single-token keyword in lexer(s) (V2, Firebird, PostgreSQL, MySQL).
- [x] Extend V2 grammar to parse `ALTER_ELEMENT(<selector> IN <expr> TO <expr>)` as an expression.
- [x] Extend dialect parsers to accept `ALTER_ELEMENT` as an expression and map to the same AST node.
- [x] Expand EXTRACT field tokenization: allow new element names and aliases (e.g., DOW/DOY/QUARTER/WEEK/ISO_WEEK/TIMEZONE_*), or accept general identifiers and resolve at semantic stage.
- [x] Define element-selector parsing (identifier with optional args, string literal, integer index) and wire in shorthand rules (array index, composite field name, JSON/XML path).
- [x] Update BNF specs (`00_GRAMMAR_BNF.md`, `SCRATCHBIRD_SQL_COMPLETE_BNF.md`) to include `ALTER_ELEMENT` and expanded EXTRACT selectors.

## AST and expression serialization
- [x] Add `ExprKind::ALTER_ELEMENT` to `scratchbird/core/expression.h`.
- [x] Create `AlterElementExpr` node with selector info (element name/id, arg list, source expr, new value expr).
- [x] Extend expression serializer/deserializer with a new `SerializedNodeType::ALTER_ELEMENT`.
- [x] Version any on-disk expression format if required by backward compatibility rules.

## Semantic analysis and type checking
- [x] Add semantic rules that map selector -> element type per DataType, using the spec’s element catalog.
- [x] Enforce read-only elements: error if used in `ALTER_ELEMENT` (e.g., DOW, DOY, QUARTER, ISO_*).
- [x] Cast `<new-value-expr>` to the element type before alteration; enforce range/precision checks.
- [x] Apply shorthand equivalences (ARRAY index, COMPOSITE field name, JSON/XML path) before type checks.
- [x] Decide and implement NULL handling for `ALTER_ELEMENT` (NULL source => NULL result; NULL new value behavior by element).

## SBLR/bytecode surface
- [x] Define a new opcode (e.g., `EXT_ALTER_ELEMENT`) in `scratchbird/sblr/opcodes.h`.
- [x] Define a bytecode payload for element selectors (field id + arg count + arg literals/expressions).
- [x] Update bytecode generator to compile `ALTER_ELEMENT` into the new opcode and emit selector payload.
- [x] Update optimizer/expression matcher if it assumes EXTRACT is the only component operator.

## Executor/runtime implementation
- [x] Implement `ALTER_ELEMENT` in the executor with per-type handlers following the spec.
- [x] Expand `executeExtract()` (or equivalent) to support the new element list and type coverage.
- [x] Keep `ExtractField` enum aligned with new fields and aliases; define mapping for synonyms.
- [x] Temporal alteration: implement local-time component update and UTC re-normalization; support TIMEZONE_HOUR/MINUTE and TZ_OFFSET.
- [x] Array: implement ELEMENT(index), NDIMS, DIMS, LOWER(dim), UPPER(dim); confirm 1-based indexing.
- [x] Composite/Variant: implement FIELD(name/index), FIELD_NAMES, DATATYPE (variant tag handling).
- [x] Network: implement INET/CIDR/MACADDR/MACADDR8 elements (family/netmask/address/network/broadcast/hostmask, OUI/NIC flags).
- [x] Range: implement LOWER/UPPER/BOUND/EMPTY flags and infinite bounds toggles.
- [x] Spatial: implement SRID, NUM_POINTS, NUM_RINGS, NUM_GEOMETRIES, etc., and validate geometry structure.
- [x] JSON/XML: implement PATH extraction/replace (requires JSONPath/XPath support). Decide error vs NULL on missing path.
- [x] Vector: implement DIMENSION, ELEMENT, NORM_L2, DOT.
- [x] Numeric: implement SIGN/ABS/BITS/BYTES/HI64/LO64 where applicable.

## Supporting utilities
- [x] Add TypeExtractor helpers for ISO week/year, century/decade/millennium, timezone parts, and week-of-year rules.
- [x] Add JSON path parser and mutation helper (or integrate existing JSON extraction opcodes).
- [x] Add XML XPath mutation helper if XML PATH is supported for ALTER_ELEMENT.

## Error handling and SQLSTATE
- [x] Define errors for invalid element names per type (DATATYPE_MISMATCH or INVALID_ARGUMENT).
- [x] Define errors for out-of-range component assignments (DATETIME_FIELD_OVERFLOW, NUMERIC_VALUE_OUT_OF_RANGE).
- [x] Define errors for invalid selector args (INVALID_TEXT_REPRESENTATION or INVALID_ARGUMENT).

## Tests (spec coverage)
- [x] Add unit tests for EXTRACT expanded fields for all supported types.
- [x] Add ALTER_ELEMENT tests for temporal edge cases (month length, leap years, TZ offsets).
- [x] Add array/composite/variant and JSON/XML element update tests.
- [x] Add network and range element update tests.
- [x] Add serialization/deserialization round-trip tests for ALTER_ELEMENT expressions.

## Documentation sync
- [x] Link `EXTRACT_AND_ALTER_ELEMENT.md` from the core specs index.
- [x] Update `03_TYPES_AND_DOMAINS.md` examples to include ALTER_ELEMENT and the new element catalog.
- [x] Update parser specs to include ALTER_ELEMENT syntax in dialect sections.
