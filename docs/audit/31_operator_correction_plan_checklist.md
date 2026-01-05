# Operator Correction Plan Checklist (Actual)

Purpose: Checklist of missing or incorrect operator implementations identified in `29_operator_inventory_actual.md`.

Status: static code review snapshot; no runtime execution performed.

## V2 / Firebird (shared pipeline)
- [ ] Fix `||` concatenation encoding (BinaryOp::CONCAT is parsed but emitted as EXPR_ADD).
- [ ] Implement null-safe `IS DISTINCT FROM` / `IS NOT DISTINCT FROM` using EXT_NULL_SAFE_EQ.
- [ ] Replace unary NOT implementation with a boolean NOT opcode (preserve NULL).
- [ ] Add bitwise operators: `&`, `|`, `^`, `~`, `<<`, `>>` (parser + bytecode + executor mapping).
- [ ] Add array operators: `@>`, `<@`, `&&` (parser + bytecode; executor already supports).
- [ ] Add JSON existence operators: `?`, `?|`, `?&` (parser + bytecode mapping).
- [ ] Add `::` cast operator (parser only; bytecode/executor already support CAST).
- [ ] Decide and implement `^` (power) operator or document as unsupported (POWER() only).
- [ ] Implement Firebird `CONTAINING` and `STARTING WITH` match kinds in bytecode generation.

## PostgreSQL parser (direct SBLR emission)
- [ ] Fix `NOT` to emit boolean NOT, not EXT_BIT_NOT.
- [ ] Fix `||` to emit string concatenation, not EXT_ARRAY_CAT.
- [ ] Add regex operators `~`, `~*`, `!~`, `!~*` if required by dialect.
- [ ] Implement EXT_ARRAY_SUBSCRIPT in executor or stop emitting it.

## MySQL parser (direct SBLR emission)
- [ ] Fix `NOT` to emit boolean NOT, not EXT_BIT_NOT.
- [ ] Fix `XOR` to emit boolean XOR semantics (not EXT_BIT_XOR).
- [ ] Implement NOT IN as a first-class comparison (current parse order treats NOT as unary).
- [ ] Fix `DIV` to emit integer division instead of modulo.
