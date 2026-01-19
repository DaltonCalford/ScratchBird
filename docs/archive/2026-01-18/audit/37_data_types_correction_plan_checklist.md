# Data Types Correction Plan Checklist (Actual)

Purpose: Focused checklist of remaining data-type deviations between canonical encoding/cast rules and the current implementation.

Status: static code review snapshot; no runtime execution performed.

Scope and sources:
- `ScratchBird-Analysis/reports/ai_docs/13_data_types_actual.md`
- `ScratchBird/docs/specifications/DATA_TYPE_PERSISTENCE_AND_CASTS.md`
- `ScratchBird/include/scratchbird/core/types.h`
- `ScratchBird/include/scratchbird/core/plain_value_reader.h`
- `ScratchBird/src/core/typed_value.cpp`
- `ScratchBird/src/sblr/executor.cpp`
- `ScratchBird/src/protocol/adapters/postgresql_adapter.cpp`
- `ScratchBird/src/core/sqlstate.cpp`

## Checklist
- [x] DT-01: Implement fixed-length semantics for CHAR and BINARY (padding/truncation rules) and enforce BINARY length based on `TypeInfo.precision` during CAST and tuple serialization.
- [x] DT-02: Apply `TypeInfo.with_timezone` and `timezone_hint` in temporal conversion/formatting (or remove the metadata if not supported) to align with timezone catalog usage.
- [x] DT-03: Standardize CAST/TRY_CAST error messaging so the source value and target type are always included (beyond current executor-only context hints).
- [x] DT-04: Align protocol adapter SQLSTATE mapping with core `statusToSQLState` for `DATETIME_FIELD_OVERFLOW` (22008) and `DATATYPE_MISMATCH` (42804).
- [x] DT-05: Update temporal storage to microsecond resolution per spec (TIME/TIMESTAMP as int64 microseconds + offset seconds); update serialize/deserialize, `computePlainValueSize`/`plain_value_reader`, and parse/format of fractional seconds (1-6 digits), including `server.time.date_default_time` handling.
- [x] DT-06: Clarify INT128 range policy. `convertTo` currently limits INT128 to 38 decimal digits; lift to full 128-bit range or codify the 38-digit limit in specs and validators.
- [x] DT-07: Implement SQL hex integer literal parsing (`0x`/`0X`) in lexers/parsers and literal typing so `SELECT 0x6FAA0D3 ...` yields an integer value.
- [x] DT-08: Support numeric CAST/TRY_CAST `USING hex|hexadecimal` (format integers with `0x` prefix; parse hex strings) and carry the cast format through SBLR/executor payloads.
- [x] DT-09: Enforce canonical round-trip text formats for all types (`TypedValue::toString` output matches spec; `convertTo` accepts any canonical string), including hex-prefixed integers, temporal offsets, UUID variants, and binary `0x`/`\\x` prefixes.
- [x] DT-10: Enforce CHAR/VARCHAR length limits by character count (UTF-8) rather than byte length in CAST and row writes.
- [x] DT-11: Add full UINT128 support (0..2^128-1) end-to-end.
  - Type system: add `DataType::UINT128` in `ScratchBird/include/scratchbird/core/types.h`; update name parsing in `ScratchBird/src/core/type_system.cpp`; update integer helpers (`isIntegerType`, `isUnsignedType`, `isNumericType`, `isBinaryLike`) in `ScratchBird/src/core/typed_value.cpp`.
  - Parser/lexer: add UINT128 keyword and type mapping in `ScratchBird/src/parser/firebird/firebird_lexer.cpp`, `ScratchBird/src/parser/firebird/firebird_parser.cpp`, `ScratchBird/src/parser/lexer_v2.cpp`, `ScratchBird/src/parser/mysql/mysql_lexer.cpp`, `ScratchBird/src/parser/mysql/mysql_parser.cpp`, `ScratchBird/src/parser/postgresql/pg_lexer.cpp`, `ScratchBird/src/parser/postgresql/pg_parser_ddl.cpp`, `ScratchBird/src/parser/postgresql/pg_parser.cpp` (V2 lexer uses contextual identifiers, so no gatekeeper keyword needed).
  - SBLR: add `TYPE_UINT128` opcode in `ScratchBird/include/scratchbird/sblr/opcodes.h`, map type emission in dialect parsers, and update `ScratchBird/src/sblr/semantic_analyzer_v2.cpp` plus executor type decoding to produce `TypeInfo(DataType::UINT128)`.
  - Storage/serialization: extend `TypedValue` to store UINT128 as 16-byte little-endian; update `TypedValue::serializePlainValue`, `deserializePlainValue`, `computePlainValueSize`, and `ScratchBird/include/scratchbird/core/plain_value_reader.h`; update `ScratchBird/src/core/columnstore.cpp` and `ScratchBird/src/optimizer/index_advisor.cpp` for size/paths.
  - Conversions/formatting: add unsigned parse helpers (decimal + `0x` hex) returning `uint128_t`; update `TypedValue::convertTo` for UINT128 conversions/overflow checks; update `toString` to canonical decimal; support `USING hex/hexadecimal` for UINT128 formatting; treat UINT128 as binary-like for 16-byte casts where needed.
  - Arithmetic/comparison/aggregation: ensure expression evaluator/executor math ops, comparisons, hashing, and aggregates support UINT128 without sign extension or overflow bugs.
  - Catalog/type widening: extend integer widening rules in `ScratchBird/src/core/catalog_manager.cpp` to include UINT8→UINT16→UINT32→UINT64→UINT128 and update type compatibility checks.
  - Specs/tests: update `ScratchBird/docs/specifications/DATA_TYPE_PERSISTENCE_AND_CASTS.md` and add tests for parser literals, casts, storage round-trip, and arithmetic boundaries.
