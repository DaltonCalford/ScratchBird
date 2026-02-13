# Findings: core/INTERNAL_FUNCTIONS.md

Spec file: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/INTERNAL_FUNCTIONS.md`

## Authoritativeness
- The file header marks it “Non-Authoritative Reference” but also claims “Status: Authoritative (V3)”. It is **not listed** in `docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`, so it should be treated as non-authoritative unless promoted.

## Summary
Function support is **partial and split across legacy/extended opcodes vs V3 SBLR opcodes**. Many functions listed here exist as V3 opcodes, but the V3 executor evaluation path does not implement them; instead, legacy extended-opcode handlers implement several of these functions. The V3 emitter only maps a small subset of function names to V3 opcodes, so most of the “Required Additions” are not reachable via the V3 pipeline.

## Implemented (Partial)
- **DIV / CONTAINING / STARTING WITH** parsing and V3 emission exist.
  - Evidence: `src/parser/parser_v3.cpp` (DIV_INT, CONTAINING/STARTING), `src/parser/v3_emitter.cpp` (SBLR3_EXPR_DIV_INT, SBLR3_PRED_CONTAINING/STARTING_WITH).
- **JSON_EXISTS** exists in V3 binary op mapping and opcode list.
  - Evidence: `src/parser/parser_v3.cpp` (BinaryOp::JSON_EXISTS), `src/parser/v3_emitter.cpp` (SBLR3_FUNC_JSON_EXISTS), `src/sblr/v3_opcodes.generated.cpp`.
- **Temporal functions** (NOW/CURRENT_DATE/CURRENT_TIME/DATE_ADD/DATE_SUB/DATE_DIFF) implemented in expression evaluator via function-name dispatch.
  - Evidence: `src/sblr/expression_evaluator.cpp`.
- **Extended opcode implementations** exist for many required functions: LTRIM/RTRIM/CONCAT_WS/REPLACE/ENDS_WITH/ARRAY_POSITION/ARRAY_SLICE/JSON_EXISTS/JSON_HAS_KEY/TO_CHAR/TO_DATE/TO_TIMESTAMP/LEAST/GREATEST, plus EXT_EXPR_FUNCTION_CALL handling.
  - Evidence: `src/sblr/executor.cpp`, `include/scratchbird/sblr/opcodes.h`.

## Gaps / Discrepancies
1. **V3 opcode vs extended opcode mismatch**
- Spec expects V3 opcode emission and V3 executor handling. In practice, many functions are only implemented in the legacy extended-opcode path; the V3 executor’s `SBLR3_EXPR_FUNCTION_CALL` evaluation does not implement them and returns NULL for unknown functions.
  - Evidence: `src/sblr/executor.cpp` V3 eval handles only COALESCE/NULLIF; extended handlers implement the rest.

2. **V3 emitter function mapping is incomplete**
- `V3Emitter::emitFunctionCall` maps only a small set of names (COALESCE/NULLIF/POWER/ABS/SIN/COS/TAN/CONCAT, aggregates, ARRAY_AGG). Functions like CONCAT_WS, REPLACE, ENDS_WITH, TO_CHAR/TO_DATE/TO_TIMESTAMP, LEAST/GREATEST, ARRAY_POSITION are not mapped to V3 opcodes here.
  - Evidence: `src/parser/v3_emitter.cpp` function map.

3. **Parser support for required additions is minimal**
- Parser V3 only explicitly constructs `ARRAY_SLICE` as a function call. Other listed required functions do not have explicit parse hooks and rely on generic function parsing; without emitter mapping, they won’t reach the correct opcodes.
  - Evidence: `src/parser/parser_v3.cpp` (only explicit `makeFunctionCall("ARRAY_SLICE")`).

4. **JSONB semantics and array string semantics are not enforced via V3 path**
- JSONB canonical CBOR behavior is not clearly enforced in the V3 opcode path; JSON/JSONB operations appear to be implemented via legacy handlers and helper conversions.

5. **Expression-level stored/UDR calls**
- Spec expects `EXT_EXPR_FUNCTION_CALL` payloads for stored/UDR functions. V3 opcode `SBLR3_EXPR_FUNCTION_CALL` is mapped but not executed with function-resolution logic in the V3 evaluator. This suggests stored/UDR calls in V3 are not fully wired.

## Notes
- This spec needs clarification: it claims “Authoritative (V3)” but is not in the authoritative inventory and conflicts with observed V3 executor/emitter wiring.

## Suggested Next Steps
- Decide whether to promote this spec to authoritative and align V3 emitter/executor to it, or mark it strictly non-authoritative.
- If authoritative, add V3 executor handling for `SBLR3_FUNC_*` opcodes and map function names in `V3Emitter::emitFunctionCall` accordingly.
- Ensure stored/UDR function calls are executed in the V3 path (not only legacy extended opcodes).
