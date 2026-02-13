# SBLR_V3_OPCODE_PAYLOADS.md - Implementation Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md` (Authoritative, updated 2026-02-08)

Summary:
- Core encoding primitives (varuint, string, bytes, schema_path, opt/list) are implemented in `src/sblr/v3_codec.cpp` and are used by schema-based encoding.
- The schema registry exists (`src/sblr/v3_schema.generated.cpp`) and is used by `src/sblr/v3_payloads.cpp` for opcode-to-schema mapping, but multiple schema/encoding mismatches exist versus the spec.
- Several opcodes emitted by the parser are not mapped to schemas (or use non-spec opcode names), which can cause `encodeInstructionWithSchema` failures or out-of-spec payloads.

Key findings (by spec section):

## Global Encoding Rules
[*] `string` / `bytes` / `schema_path` / `opt<T>` / `list<T>` encodings are implemented via `encodeString`, `encodeBytes`, `encodeVaruint`, and `encodeValue` in `src/sblr/v3_codec.cpp`.

## Shared Structs / Schemas
[ ] `TYPE_SPEC` type-specific payload rules are not enforced. `TypeSpec` is serialized as `[type_opcode:u16][type_payload:bytes]` with no per-type validation or canonicalization. See `src/sblr/v3_codec.cpp` and `src/sblr/v3_schema.cpp`.
[ ] `COLUMN_DEF` is missing repeated `check_expr` entries. Schema includes `check_count` but no list of check expressions (`src/sblr/v3_schema.generated.cpp`). Emitter only stores the first check expression in `check_expr` and drops additional checks (`src/parser/v3_emitter.cpp:4054`), which does not match `[check_expr...check_count]`.
[ ] `SCHEMA_EXPR_CASE` does not match the repeated `when/then` pairs. Schema defines `when_count` + single `when`/`then` fields (`src/sblr/v3_schema.generated.cpp:156`), and the emitter only emits the first WHEN/THEN pair (`src/parser/v3_emitter.cpp:3801`), not the full list required by the spec.
[ ] `SCHEMA_PSQL_CASE` mirrors the same structural issue: schema defines `when_count` + single `when`/`then` (`src/sblr/v3_schema.generated.cpp:707`), but the spec requires repeated WHEN/THEN statements.
[ ] `SCHEMA_WINDOW_SPEC` alias is missing from the schema registry. The payload map routes `SBLR3_WINDOW_SPEC`/`SBLR3_WINDOW` to `SCHEMA_WINDOW_SPEC` (`src/sblr/v3_payload_map.generated.cpp` and `src/sblr/v3_payloads.cpp`), but no `SCHEMA_WINDOW_SPEC` exists in `src/sblr/v3_schema.generated.cpp` (only `WINDOW_SPEC`). This makes schema lookup fail for those opcodes.

## Literal Schemas
[ ] `SCHEMA_LITERAL_DATE` is encoded as `i64` + `offset_seconds` in `lookupSchema` (`src/sblr/v3_schema.cpp`), but the spec requires `[value:i32][offset_seconds:i32]` for DATE.
[~] Several complex literal opcodes are emitted with ad-hoc byte layouts (e.g., `SBLR3_LITERAL_DATETIME`, `SBLR3_LITERAL_RANGE`, `SBLR3_LITERAL_ROW`, `SBLR3_LITERAL_DOMAIN`) in `src/parser/v3_emitter.cpp`. Their payloads are raw bytes, but the spec requires canonical storage encodings (see `types/VALUE_SPEC_STORAGE_ENCODINGS.md`). These encodings have not been verified.

## Opcode Schema Mapping Rules
[ ] Opcode naming is out-of-spec for several expressions and DDL statements. Examples:
- CASE uses `SBLR3_CASE_WHEN` instead of `SBLR3_CASE` (`src/parser/v3_emitter.cpp:3801`; `src/sblr/v3_payloads.cpp` special-case).
- IN uses `SBLR3_IN_LIST` / `SBLR3_SUBQUERY_IN` / `SBLR3_SUBQUERY_NOT_IN` instead of `SBLR3_IN` / `SBLR3_NOT_IN` (`src/parser/v3_emitter.cpp:3820`).
- LIKE uses `SBLR3_EXPR_LIKE` / `SBLR3_EXPR_ILIKE` instead of `SBLR3_LIKE` / `SBLR3_NOT_LIKE` (`src/parser/v3_emitter.cpp:3916`).
- CREATE/DROP opcodes include `_STMT` suffixes (e.g., `SBLR3_CREATE_FUNCTION_STMT`) which are mapped in code but do not match spec names (`src/sblr/v3_payloads.cpp:214`).

[ ] BETWEEN is not emitted as a `SBLR3_BETWEEN` / `SBLR3_NOT_BETWEEN` payload at all; the emitter lowers BETWEEN to `>=`/`<=` expressions (`src/parser/v3_emitter.cpp:3843`). This bypasses the `SCHEMA_EXPR_BETWEEN` payload spec entirely.

[ ] `SBLR3_SHOW_*` opcodes are emitted with `SCHEMA_SET_SHOW_RESET`-style payloads (`src/parser/v3_emitter.cpp:2440`), but there is no schema mapping for the individual `SBLR3_SHOW_*` opcodes in `schemaForOpcode` or the generated payload map (only `SBLR3_SHOW` / `SBLR3_SHOW_ALL`). This can cause `encodeInstructionWithSchema` to fail for show opcodes.

## Index / Job / Session Schemas
[~] Index/job schemas exist in `src/sblr/v3_schema.generated.cpp`, but no executor/serialization validation against the field-level rules was found in `src/sblr/v3_validator.cpp`. These payloads are structurally available but not validated per spec.

