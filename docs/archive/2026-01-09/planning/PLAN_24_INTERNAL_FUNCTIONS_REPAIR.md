# Plan 24 - Internal Functions Repair (Alpha)

Status: In progress. Owner: Core/SBLR. Source audit: `docs/audit/24_internal_functions_actual.md`.

## Goals
- Expand V2 to emit and execute all internal functions already implemented in the executor (SBLR opcodes/extended opcodes).
- Make temporal built-ins return typed DATE/TIME/TIMESTAMP values (TypedValue microseconds + timezone offsets).
- Formalize JSON array string semantics and align return types/validators.
- Prioritize real JSONB semantics (binary representation, canonicalization, JSONB_* ops).
- Enable expression calls to stored PSQL functions and UDR functions (no legacy binary UDFs).

## Non-goals
- No changes to the legacy Firebird-style binary UDF loading path (it should remain removed).
- No new SQL features outside the existing executor surface area.
- No optimizer changes beyond function/type correctness.

## Decisions (from user)
- V2 must expand to surface executor functionality.
- Temporal built-ins return typed DATE/TIME/TIMESTAMP.
- Arrays are JSON array strings; update function return types accordingly.
- JSONB gets real semantics (not just JSON strings).
- PSQL stored functions and UDR functions must be callable in expressions.

## Current gaps to address
- Semantic analyzer recognizes functions that the bytecode generator does not emit.
- Temporal built-ins return INT64 seconds; type system expects typed values.
- Array functions operate on JSON strings but are typed as ARRAY in some places.
- JSONB functions return JSON strings; JSONB storage is not binary.
- Unknown functions serialize as names only; executor cannot dispatch them.
- Expression evaluator diverges from executor (ASCII case and byte length).
- Duplicate/unreachable XML/OVERLAY handlers in executor.

## Progress
- Phase 1: V2 emission parity complete (new built-in opcodes and no unknown built-ins).
- Phase 2: Temporal built-ins return typed DATE/TIME/TIMESTAMP values.
- Phase 3: JSON array string semantics aligned (ARRAY_AGG returns JSON; stats accept JSON arrays).
- Phase 4: JSONB stored as canonical CBOR (TypedValue + casts).
- Phase 5: Stored function calls implemented; UDR execution path stubbed (engine pending).
- Phase 6: ExpressionEvaluator parity updated (UTF-8 case/length + string, math, temporal, EXTRACT, LIKE/ILIKE/regex, IN).

## Work plan (phased)

### Phase 0: Function surface map + type contract
- Produce a function matrix that maps:
  - V2 semantic analyzer name -> bytecode opcode -> executor handler -> return type.
- Define return types for each function in `docs/specifications/INTERNAL_FUNCTIONS.md` (new doc) or append to `docs/specifications/03_TYPES_AND_DOMAINS.md`.
- Decide final type contracts for:
  - DATE_ADD/SUB/DIFF
  - NOW/CURRENT_TIMESTAMP/CURRENT_DATE/CURRENT_TIME
  - AT TIME ZONE
  - ARRAY_* functions and ARRAY_AGG
  - JSONB_* functions

Deliverable: Updated function matrix + explicit type contracts.

### Phase 1: V2 emission parity (built-ins already recognized)
- Add bytecode emission for:
  - LTRIM, RTRIM
  - CONCAT, CONCAT_WS
  - CURRENT_TIME
- Ensure semantic analyzer validates arg counts for all emitted built-ins.
- Stop emitting raw function-name calls without opcode dispatch.
  - If function is not builtin, resolve as stored function or UDR.
  - If unresolved, return a compile-time error (no more unknown-builtin warnings).

Files (expected):
- `src/sblr/semantic_analyzer_v2.cpp`
- `src/sblr/bytecode_generator_v2.cpp`
- `include/scratchbird/sblr/resolved_ast_v2.h`

### Phase 2: Temporal built-ins return typed values
- Update executor implementations of:
  - NOW/CURRENT_TIMESTAMP -> `TypedValue::makeTimestamp(micros, tz_offset)`
  - CURRENT_DATE -> `TypedValue::makeDate(days, tz_offset)`
  - CURRENT_TIME -> `TypedValue::makeTime(micros, tz_offset)`
  - DATE_ADD/SUB -> return same type as input (DATE/TIME/TIMESTAMP)
  - DATE_DIFF -> return INT64 days (or INTERVAL if spec decides otherwise)
  - AT TIME ZONE -> return TIMESTAMP WITH TIME ZONE (TypeInfo.with_timezone = true)
- Update semantic analyzer return types to match the executor.
- Update any cast/formatting paths to keep typed values consistent.

Files (expected):
- `src/sblr/executor.cpp`
- `src/sblr/semantic_analyzer_v2.cpp`
- `include/scratchbird/core/types.h`
- `src/core/typed_value.cpp`

### Phase 3: Formalize JSON array string semantics
- Document that ARRAY_* and ARRAY_AGG operate on JSON array strings (not DataType::ARRAY).
- Update semantic analyzer return types:
  - ARRAY_AGG -> JSON (string)
  - ARRAY_* helpers -> JSON/TEXT/BOOLEAN/INT64 as implemented in executor
  - Scalar array stats -> accept JSON arrays (convert/parse internally)
- Update array stats opcodes to parse JSON array strings instead of DataType::ARRAY.
- Ensure JSON array string parsing is shared and validated consistently.

Files (expected):
- `src/sblr/semantic_analyzer_v2.cpp`
- `src/sblr/executor.cpp`
- `src/core/typed_value.cpp` (if helper parsing is added)
- `docs/specifications/INTERNAL_FUNCTIONS.md`

### Phase 4: JSONB semantics (binary)
- Define JSONB binary format and canonicalization rules.
  - Proposed: store canonical JSON in binary form (e.g., CBOR via nlohmann::json).
  - Normalize key ordering and remove whitespace on write.
- Update JSONB storage:
  - Serialize/deserialize JSONB using binary payload.
  - JSONB toString returns canonical JSON text.
- Update JSONB_* functions to operate on binary JSONB.
- Update casts and type coercions between JSON and JSONB.

Files (expected):
- `src/core/typed_value.cpp`
- `src/core/typed_value.h`
- `src/sblr/executor.cpp`
- `docs/specifications/03_TYPES_AND_DOMAINS.md`

### Phase 5: Stored function + UDR calls in expressions
- Extend semantic analyzer to resolve function calls to:
  - builtin
  - stored PSQL function
  - UDR function
- Define new expression opcode (or reuse EXT_FUNCTION) that includes:
  - function id or fully-qualified name
  - arg count + expression payloads
  - SQL SECURITY context if required
- Extend executor expression evaluation to call stored functions/UDRs with:
  - argument value list
  - permissions check
  - error propagation and NULL handling

Files (expected):
- `src/sblr/semantic_analyzer_v2.cpp`
- `src/sblr/bytecode_generator_v2.cpp`
- `include/scratchbird/sblr/opcodes.h`
- `src/sblr/executor.cpp`
- `src/core/catalog_manager.cpp` (function lookup)

### Phase 6: ExpressionEvaluator parity + cleanup
- Align ExpressionEvaluator behavior with executor:
  - UTF-8 case mapping
  - LENGTH/CHAR_LENGTH semantics
  - supported functions list (or strict subset enforced at compile time)
- Remove duplicate/unreachable XML and OVERLAY handlers.

Files (expected):
- `src/sblr/expression_evaluator.cpp`
- `src/sblr/executor.cpp`

## Tests
- Bytecode emission tests for newly surfaced functions.
- Temporal correctness tests for NOW/CURRENT_DATE/CURRENT_TIME/DATE_ADD/SUB/DIFF.
- JSON array string tests for ARRAY_* and ARRAY_AGG.
- JSONB storage + JSONB_* function tests (including canonicalization).
- Expression-level stored function + UDR call tests.
- ExpressionEvaluator parity tests (index evaluation vs executor).

## Acceptance criteria
- V2 emits opcodes for all supported internal functions in executor.
- Temporal built-ins return typed values consistent with TypedValue microseconds/TZ.
- JSON array functions operate on JSON array strings; types reflect that.
- JSONB is binary with canonical semantics; JSONB_* functions operate correctly.
- Stored PSQL functions and UDR functions are callable in expressions.
- No orphan or duplicate opcode handlers remain in executor.

## Dependencies/risks
- JSONB binary format choice affects storage compatibility (Alpha-only acceptable).
- UDR invocation from expressions must not bypass permission checks.
- ExpressionEvaluator parity requires shared helpers to avoid divergence.

## Proposed next step
- Review this plan and confirm:
  - JSONB binary format choice (CBOR vs MessagePack vs custom).
  - Desired return type for DATE_DIFF (INT64 days vs INTERVAL).
  - AT TIME ZONE return type (TIMESTAMP WITH TIME ZONE vs TIMESTAMP).

## Confirmed choices
- JSONB binary format: CBOR (canonicalized; keys sorted, minimal encoding).
- DATE_DIFF returns INT64 days.
- AT TIME ZONE returns TIMESTAMP WITH TIME ZONE.
