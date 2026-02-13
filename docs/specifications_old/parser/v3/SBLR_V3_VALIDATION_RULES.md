# SBLR V3 Validation Rules
Status: Authoritative (V3)
Last Updated: 2026-02-08

This document defines the formal validation rules for V3 SBLR bytecode. A
conforming verifier MUST reject any module that violates these rules.

## Goals
- Ensure bytecode is structurally valid before execution.
- Guarantee stack safety (no underflow/overflow).
- Enforce opcode ordering and dependency constraints.
- Provide deterministic and reproducible validation behavior.

## Terminology
- **Verifier**: Component that statically validates bytecode before execution.
- **Frame**: Execution scope with its own stack and local variables (PSQL).
- **Type Stack**: Compile-time stack of type descriptors used to validate
  LITERAL and CAST opcodes.

## Global Module Validation
- Container must pass all rules in `SBLR_V3_BYTECODE_CONTAINER.md`.
- Symbol and constant pools must pass `SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`.
- BYTECODE_STREAM must begin with `SBLR3_VERSION` and end with `SBLR3_END`.
- No unknown opcode values are permitted.

## Verifier Error Codes (Authoritative)

Each failure MUST return a deterministic error code and message. Codes are
stable across versions.

Error code format: `V3E-XXXX`.

| Code | Category | Condition |
| --- | --- | --- |
| `V3E-0001` | STRUCT | Instruction header length mismatch (payload_len does not match actual payload size). |
| `V3E-0002` | STRUCT | Unknown opcode (not present in `SBLR_V3_OPCODE_SPEC.md`). |
| `V3E-0003` | STRUCT | Extended opcode used without `SBLR3_EXTENDED_OPCODE` envelope. |
| `V3E-0010` | TYPE | Literal opcode used where non-literal `expr` is required. |
| `V3E-0011` | TYPE | Non-literal opcode used where `VALUE_SPEC` is required. |
| `V3E-0012` | TYPE | TYPE_SPEC missing required flags for provided optional fields. |
| `V3E-0020` | RANGE | RANGE literal with invalid bounds (lower > upper). |
| `V3E-0021` | RANGE | RANGE literal has lower/upper present while marked INF/EMPTY. |
| `V3E-0022` | RANGE | RANGE literal base type mismatch for embedded VALUE_SPEC. |
| `V3E-0030` | ARRAY | ARRAY literal element count does not match dimensions. |
| `V3E-0031` | ARRAY | ARRAY literal element types inconsistent with element_type. |
| `V3E-0040` | TIME | TIME/TIMESTAMP TZ literal has invalid timezone identifier. |
| `V3E-0041` | TIME | TIME/TIMESTAMP TZ literal has invalid offset for named zone. |
| `V3E-0050` | TEXT | TSVECTOR/TSQUERY literal is not in canonical form. |
| `V3E-0060` | DDL | DDL opcode missing required schema_path or invalid identifier. |
| `V3E-0061` | DDL | DDL opcode contains duplicate column names in COLUMN_DEF list. |
| `V3E-0070` | DML | SELECT flags contain mutually exclusive bits (DISTINCT+ALL). |
| `V3E-0071` | DML | INSERT source kind mismatches provided values/select. |
| `V3E-0072` | DML | UPDATE/DELETE alias provided but target is empty. |
| `V3E-0080` | PSQL | PSQL block has invalid handler ordering (WHEN before statements). |
| `V3E-0090` | REF | schema_path references empty ident element. |
| `V3E-0091` | REF | ident length exceeds 128 bytes. |
| `V3E-0100` | CANON | SYMBOL_TABLE not sorted by canonical order. |
| `V3E-0101` | CANON | CONSTANT_POOL not sorted by canonical order. |
| `V3E-0102` | CANON | OPTION_KV list not sorted by key. |
| `V3E-0103` | CANON | GRANT/REVOKE privilege list not sorted. |
| `V3E-0104` | CANON | ON CONFLICT column list not sorted. |
| `V3E-0105` | CANON | Index INCLUDE list not sorted. |

## Stack Discipline (Normative)
### Max Stack Depth
- Each module declares `max_stack_depth` in the MODULE_METADATA payload.
- Verifier computes the maximum runtime stack depth from opcode effects.
- If computed depth exceeds `max_stack_depth`, validation fails.

### Underflow Checks
For each opcode:
- `stack_in` items MUST be available on the stack.
- Underflow is a hard error.

### Type Stack Discipline
- TYPE opcodes push a type descriptor onto the type stack.
- LITERAL opcodes require the expected type descriptor when applicable.
- CAST opcodes require a source type and target type on the type stack.
- Mismatched or missing type stack entries fail validation.

## Operand Type Validation
Verifier enforces operand compatibility:
- Numeric operators require numeric operands or valid numeric casts.
- Boolean operators require boolean operands.
- Comparison operators require comparable types (collation-aware for strings).
- Array/range operators require compatible element types.
- Spatial/textsearch operators require declared compatible types.

Type rules are derived from `SBLR_V3_OPCODE_PAYLOADS.md` and
`AST_TYPE_AND_LITERAL_SPEC.md`.

## Opcode Ordering Rules
### Stream Ordering
1. `SBLR3_VERSION` must be the first opcode.
2. `SBLR3_END` must be the final opcode.
3. No opcodes may follow `SBLR3_END`.

### DDL Ordering
- CREATE/ALTER/DROP opcodes must appear as a complete statement block and must
  not be interleaved with DML or PSQL opcodes unless explicitly allowed by
  payload (e.g., CREATE TABLE AS uses SELECT).
- Constraint-related opcodes (PRIMARY_KEY, FOREIGN_KEY, CHECK_CONSTRAINT, etc.)
  must follow the parent CREATE/ALTER TABLE opcode.

### DML Ordering
- `MERGE_START` must precede all MERGE sub-ops.
- `MERGE_END` must be last in a MERGE block.
- `FOR_UPDATE`/`FOR_SHARE` must appear before SELECT pipeline nodes they modify.

### PSQL Ordering
- `BLOCK` introduces a new scope; all DECLARE/PARAM ops must occur before
  executable statements in the same block.
- `LABEL` must appear before `JUMP` targets referencing it.
- `TRY`/`EXCEPTION_HANDLER` blocks must be properly nested.

### Query Structure Ordering
- `WITH_CLAUSE` must precede `SELECT`.
- `FROM` must precede `WHERE`, `GROUP BY`, `HAVING`, `ORDER BY`.
- `ORDER BY` must precede `LIMIT/OFFSET`.
- `WINDOW_SPEC` must appear before any WINDOW function referencing it.

## Canonicalization Validation (Normative)
The verifier MUST enforce canonicalization rules defined in
`SBLR_V3_BYTECODE_CANONICALIZATION.md`:
- SYMBOL_TABLE ordering (V3E‑0100)
- CONSTANT_POOL ordering (V3E‑0101)
- OPTION_KV sorting (V3E‑0102)
- GRANT/REVOKE privilege list sorting (V3E‑0103)
- ON CONFLICT column list sorting (V3E‑0104)
- Index INCLUDE list sorting (V3E‑0105)

## Must-Precede / Must-Follow Rules (Normative)
| Opcode | Must Precede | Must Follow |
| --- | --- | --- |
| SBLR3_MERGE_START | SBLR3_MERGE_SOURCE | - |
| SBLR3_MERGE_SOURCE | SBLR3_MERGE_ON | SBLR3_MERGE_START |
| SBLR3_MERGE_ON | SBLR3_MERGE_WHEN_MATCHED | SBLR3_MERGE_SOURCE |
| SBLR3_MERGE_WHEN_MATCHED | SBLR3_MERGE_WHEN_NOT_MATCHED | SBLR3_MERGE_ON |
| SBLR3_MERGE_WHEN_NOT_MATCHED | SBLR3_MERGE_END | SBLR3_MERGE_ON |
| SBLR3_MERGE_END | - | SBLR3_MERGE_START |
| SBLR3_BLOCK | - | (parent block or module) |
| SBLR3_DECLARE | executable stmt | SBLR3_BLOCK |
| SBLR3_PARAM_IN/OUT/INOUT | executable stmt | SBLR3_FUNCTION or SBLR3_PROCEDURE |
| SBLR3_WITH_CLAUSE | SBLR3_SELECT | - |
| SBLR3_FROM | SBLR3_WHERE | SBLR3_SELECT |
| SBLR3_ORDER_BY | SBLR3_FETCH_FIRST/NEXT/ONLY | SBLR3_SELECT or SBLR3_GROUP_BY/HAVING |
| SBLR3_WINDOW_SPEC | SBLR3_WINDOW_* opcodes | SBLR3_SELECT |

## Constant and Symbol Reference Validation
- All `string_id` references must be in range of SYMBOL_TABLE.
- All `const_id` references must be in range of CONSTANT_POOL.
- Catalog IDs must reference UUID constants only.
- Inline constants are forbidden unless explicitly allowed by opcode payload.

## Literal Validation Rules (Normative)

### Numeric Literal Bounds
- `SBLR3_LITERAL_INT8`: value must be in [-128, 127].
- `SBLR3_LITERAL_INT16`: value must be in [-32768, 32767].
- `SBLR3_LITERAL_INT32`: value must be in 32-bit signed range.
- `SBLR3_LITERAL_INT64`: value must be in 64-bit signed range.
- `SBLR3_LITERAL_INT128`: value must be in 128-bit signed range.
- `SBLR3_LITERAL_UINT8`: value must be in [0, 255].
- `SBLR3_LITERAL_UINT16`: value must be in [0, 65535].
- `SBLR3_LITERAL_UINT32`: value must be in [0, 2^32-1].
- `SBLR3_LITERAL_UINT64`: value must be in [0, 2^64-1].
- `SBLR3_LITERAL_UINT128`: value must be in [0, 2^128-1].
- `SBLR3_LITERAL_FLOAT32`: payload must be a finite IEEE-754 32-bit float.

### TIME/TIMESTAMP WITH TIME ZONE
- `SBLR3_LITERAL_TIME_TZ.time_usec` must be in [0, 86,399,999,999].
- `SBLR3_LITERAL_TIME_TZ.tz_offset_minutes` must be in [-1439, 1439].
- `SBLR3_LITERAL_TIMESTAMP_TZ.epoch_usec` must fit signed 64-bit.
- `SBLR3_LITERAL_TIMESTAMP_TZ.tz_offset_minutes` must be in [-1439, 1439].
- If `tz_name` is present, it MUST be non-empty UTF-8 and MUST pass canonical name rules (see `types/I18N_CANONICAL_LISTS.md`).

### RANGE Literals
- If `flags` contains `empty`, both `lower_present` and `upper_present` MUST be false.
- If `lower_inf` is set, `lower_present` MUST be false.
- If `upper_inf` is set, `upper_present` MUST be false.
- If both bounds are present, `lower` and `upper` types MUST match `range_base_type`.
- If both bounds are present and `lower > upper` (after type coercion), validation fails.

### ARRAY Literals
- `dimensions` MUST equal `len(dim_lengths)` and may be 0 for empty arrays.
- `element_count` MUST equal the product of `dim_lengths` (if dimensions > 0).
- All elements MUST be of `element_type` (or coercible under explicit CAST rules).

### VARIANT Literals
- `variant_type_id` must be a valid UUID constant.
- `tag_name` must be a valid tag defined by the variant type.
- `value` must match the tag's declared type.

### TSVECTOR / TSQUERY Literals
- Payload text must be valid UTF-8 and canonical per the parser rules in `types/README.md`.

### BLOB Locator Literals
- `blob_id` must be a valid UUID constant.
- `blob_length` may be zero; if non-zero, it must match the declared blob length in the catalog (if available).
- `blob_subtype` must match declared subtype if the target type is known.
- `compression` must be one of the declared codec IDs (0,1,2).

## Cross-Section Consistency
- Debug info offsets must resolve to valid bytecode offsets.
- Dependency list entries must resolve to valid schema/object paths when present.
- Module dialect must match opcode set (e.g., no MySQL-only ops if dialect disallows).

## Error Reporting
Verifier MUST provide:
- Failing opcode offset.
- Opcode name and numeric ID.
- Specific rule violated (stack underflow, type mismatch, ordering rule).

## Validation Checklist
- Container and section table valid.
- All opcodes known and ordered correctly.
- Stack and type stack are safe for all paths.
- Symbols/constants referenced are in range.
- All must-precede/must-follow rules satisfied.
