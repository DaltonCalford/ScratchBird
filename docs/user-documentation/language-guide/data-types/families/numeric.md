# Type Family: Numeric
Last modified: 2026-02-19

Back links:
- [Type Families README](README.md)
- [Data Types README](../README.md)

Next in series:
- [Text And Binary](text-and-binary.md)

## Parser-Accepted Numeric Names
- Signed/unsigned integer families: `TINYINT`, `SMALLINT`, `INT`, `INTEGER`, `BIGINT`, `INT128`, `UINT8`, `UINT16`, `UINT32`, `UINT64`, `UINT128`, aliases `INT2`, `INT4`, `INT8`.
- Decimal/floating families: `DECIMAL`, `NUMERIC`, `BIGNUM`, `REAL`, `FLOAT`, `DOUBLE`, `DOUBLE PRECISION`, `MONEY`, `MEDIUMINT`.
- Extended engine-opcode families in runtime mapping: `DECFLOAT16`, `DECFLOAT34`, nullable type opcode paths.

## Precision/Scale And Limits
- Parser accepts precision/scale declarations where type syntax supports them.
- Hard numeric min/max value limits are enforced by runtime value-type semantics, not parser tokenization.
- Wide numeric operators (`INT256` / `UINT256` / `DECIMAL256` families) are partial for `DIV`, modulo, and bitwise operations.

## Operator Compatibility
- Fully closed in 0.1.0 for: arithmetic, comparison, and null-safe comparison families.
- Coercion behavior: implicit text-to-numeric conversion is available unless `operator.strict_mode` is enabled.
