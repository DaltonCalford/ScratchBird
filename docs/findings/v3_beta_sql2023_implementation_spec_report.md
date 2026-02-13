# BETA SQL:2023 Implementation Specification - V3 Findings

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/BETA_SQL2023_IMPLEMENTATION_SPECIFICATION.md`

Status: **Partially implemented** (foundational JSON/JSONB types and legacy JSON operators exist, but SQL:2023 features in this spec are largely missing or only partially covered).

## Summary Of Key Gaps
- JSON:2023 native JSON type is only partially present (type names exist; no SQL:2023 JSON literals, unique key enforcement, IS JSON predicate, simplified accessors, or item methods).
- JSON comparison operators are not implemented for JSON/JSONB types in core value comparisons.
- SQL:2023 numeric literal enhancements are incomplete: `0x` and `0b` are supported, `0o` is not, and underscore digit separators are not supported.
- UNIQUE NULL handling (`NULLS DISTINCT` / `NULLS NOT DISTINCT`) is not parsed or enforced.
- Multi-character TRIM is not supported (TRIM is single-argument whitespace-only).
- Rejected features (SQL/PGQ, SQL/MDA, F868 TABLE) do not have explicit `ERR_FEATURE_DISABLED` handling; currently they would parse-fail or behave as unknown.

## Evidence Highlights
- JSON/JSONB types are present as contextual type names and SBDB$ domains.
  - `src/parser/lexer_v3.cpp:1130-1131` (JSON/JSONB contextual keywords)
  - `src/core/domain_manager.cpp:88-89` (SBDB$JSON / SBDB$JSONB domains)
- JSON operators (`->`, `->>`, `#>`, `#>>`, `?`, `?|`, `?&`) and JSON functions exist, but are PostgreSQL-style, not SQL:2023 simplified accessors/item methods.
  - `src/parser/parser_v3.cpp:7791-7808`, `src/parser/parser_v3.cpp:8263-8279`
  - `src/sblr/executor.cpp:29816-29963`
- JSONB storage uses CBOR encoding, not the JSONB layout specified in the spec.
  - `src/core/typed_value.cpp:343-350`
- Numeric literals only support decimal, `0x`, `0b` (no `0o`, no underscores).
  - `src/parser/lexer_v3.cpp:495-571`
- UNIQUE constraint parsing does not include NULLS DISTINCT / NOT DISTINCT.
  - `src/parser/parser_v3.cpp:1497-1511`
- TRIM only supports single-argument whitespace trimming.
  - `src/sblr/expression_evaluator.cpp:935-952`
- JSON comparisons are not supported in core comparisons.
  - `src/core/typed_value.cpp:3792-3794`, `src/core/typed_value.cpp:3904-3905`

## Requirement-by-Requirement Checklist

### T801: Native JSON Data Type
[*] JSON/JSONB type names recognized as contextual types (`src/parser/lexer_v3.cpp:1130-1131`).
[*] SBDB$JSON / SBDB$JSONB domain entries exist (`src/core/domain_manager.cpp:88-89`).
[ ] JSON type options (`WITH UNIQUE KEYS`, storage params payload) not parsed or stored (no handling in `Parser::parseTypeName`, `src/parser/parser_v3.cpp:1360-1477`).
[ ] JSON literal syntax (`JSON '...'`, `JSON('...' [WITH UNIQUE KEYS])`) not supported in v3 parser (no JSON literal parsing paths).
[ ] IS JSON predicate not implemented (no `IS JSON` handling in comparison parsing).
[~] JSONB storage format does not match spec payload; current implementation uses CBOR (`src/core/typed_value.cpp:343-350`).
[ ] JSON index behavior per spec not verified (no JSON-specific index implementation found in v3; executor uses generic JSON functions).

### T802: JSON Type With Unique Keys
[ ] `WITH UNIQUE KEYS` not parsed in type declarations or JSON literal forms (no parser support in `Parser::parseTypeName` and no JSON literal syntax).
[ ] Unique key validation not implemented (no JSON validation/unique key checks found in core JSONB or executor).

### T803: String-Based JSON (Backward Compatibility)
[~] JSON stored as string in `TypedValue::makeJSON` (`src/core/typed_value.cpp:2015-2021`).
[ ] No explicit SQL:2023 compatibility behavior defined in v3 parser or executor.

### T840: Hexadecimal Literals in SQL/JSON Path
[ ] No SQL/JSON path support in v3 parser; no hex-literal handling within JSON path expressions.

### T860-T864: SQL/JSON Simplified Accessors
[ ] Dot-accessor (`data.field`) for JSON is not implemented; parser treats dot as qualified identifiers, not JSON accessors.
[ ] Array subscript accessor (`data[0]`) for JSON is not implemented in v3 expression grammar.
[ ] Chained accessors and NULL handling semantics not implemented.
[ ] T864 array slicing is rejected in the spec; no explicit `ERR_FEATURE_DISABLED` handling found.

### T865-T878: SQL/JSON Item Methods
[ ] Item methods (`.string()`, `.number()`, `.boolean()`, `.date()`, `.time()`, `.timestamp()`, `.bigint()`, `.integer()`, `.double()`) not parsed or executed in v3.

### T879-T882: JSON Comparison Operators
[ ] Core comparisons do not support JSON/JSONB; `TypedValue` comparison switches do not handle JSON/JSONB and default to false/throw (`src/core/typed_value.cpp:3792-3794`, `src/core/typed_value.cpp:3904-3905`).

### T661: Non-Decimal Integer Literals
[~] `0x` and `0b` literals are supported in lexer (`src/parser/lexer_v3.cpp:502-519`).
[ ] `0o` octal literals are not supported.

### T662: Underscores in Numeric Literals
[ ] Underscore digit separators not supported in lexer (no underscore handling in `scanNumber`, `src/parser/lexer_v3.cpp:495-571`).

### F401: NULL Handling in UNIQUE Constraints
[ ] `NULLS DISTINCT` / `NULLS NOT DISTINCT` not parsed in column or table constraints; `UNIQUE` parsing has no nulls clause (`src/parser/parser_v3.cpp:1497-1511`).
[ ] No enforcement found in executor/index for nulls distinct mode.

### T056: Multi-Character TRIM
[ ] TRIM only supports single-argument whitespace trimming; no multi-character trim arguments or LEADING/TRAILING/BOTH handling (`src/sblr/expression_evaluator.cpp:935-952`).

### F868: TABLE Keyword (Rejected in V3)
[ ] No explicit `ERR_FEATURE_DISABLED` handling found for the SQL:2023 TABLE keyword feature.

### SQL/PGQ (Rejected In V3)
[ ] No `ERR_FEATURE_DISABLED` handling found for CREATE PROPERTY GRAPH, GRAPH_TABLE, or pattern matching syntax.

### SQL/MDA (Deferred/Rejected In V3)
[ ] No explicit `ERR_FEATURE_DISABLED` handling found for multidimensional array features.

## Notes
- Existing JSON functionality appears to target PostgreSQL-style operators/functions rather than the SQL:2023 simplified accessor/method model.
- JSONB storage currently uses CBOR canonicalization, which conflicts with the JSONB layout described in the spec.

