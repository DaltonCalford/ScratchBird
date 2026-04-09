# Beta 2 Numeric Array And Expression UDR Model

## Purpose

This document defines the foundation UDR families for dense arrays,
broadcasting, vectorized math, reductions, matrix operations, random-number
generation, and compiled elementwise expression evaluation.

This group is the ScratchBird-native replacement target for the highest-value
parts of `NumPy` and `numexpr`.

## Owning packages

- `sb_pkg_num_array_udr`
- `sb_pkg_expr_udr`

## Mandatory surfaces

### Array construction and inspection

`sb_pkg_num_array_udr` shall provide:

- array constructors from scalar lists, rowsets, vectors, and ranges
- shape, rank, stride, and element-type inspection
- reshape, flatten, slice, transpose, concatenate, and stack operations
- identity, diagonal, zeros, ones, full, arange, linspace, and logspace helpers

### Elementwise math and broadcasting

The package shall provide:

- unary math: abs, sign, sqrt, exp, log, log10, log2, sin, cos, tan, asin,
  acos, atan, floor, ceil, round, trunc
- binary math: add, subtract, multiply, divide, power, remainder
- comparison and boolean masks
- broadcasting rules across compatible dimensions
- masked and null-aware operations

### Reductions and matrix algebra

The package shall provide:

- sum, min, max, mean, variance, stddev, median, quantile
- argmin, argmax, count_nonzero
- cumulative sum and cumulative product
- dot, matmul, transpose, determinant, inverse, rank, trace, eigensystem where
  numerically supported

### Random and sampling helpers

The package shall provide deterministic seeded generators for:

- uniform
- normal
- integers
- permutation
- choice

All random surfaces shall require an explicit seed in deterministic contexts.

### Expression engine

`sb_pkg_expr_udr` shall provide:

- validated expression compilation for array/scalar expressions
- compiled elementwise kernels
- expression caching by normalized expression plus type signature
- fused evaluation across common arithmetic and boolean operators
- explicit variable binding from rowset columns, vectors, arrays, or scalars

## Required SQL-visible routine families

At minimum the following routine families shall exist:

- `sb_num_array.array(...)`
- `sb_num_array.arange(...)`
- `sb_num_array.linspace(...)`
- `sb_num_array.reshape(...)`
- `sb_num_array.transpose(...)`
- `sb_num_array.sum(...)`
- `sb_num_array.matmul(...)`
- `sb_num_array.random_*`
- `sb_expr.compile(...)`
- `sb_expr.eval(...)`
- `sb_expr.eval_table(...)`

## Example contract

```sql
select sb_num_array.sum(sb_num_array.array[1, 2, 3, 4]);

select sb_expr.eval(
    expr => 'a * b + c',
    bindings => struct_pack('a', :vec_a, 'b', :vec_b, 'c', 5)
);
```

## Execution rules

1. Array and expression routines shall use `vectorized_deterministic`
   execution class unless they allocate long-lived artifacts.
2. Expression compilation shall validate operator whitelist, function
   whitelist, type compatibility, and maximum kernel complexity before
   admission.
3. Repeated expression execution shall reuse cached normalized kernels where
   signature-compatible.
4. Large temporary arrays shall participate in section `33` grant and spill
   policy.

## Index and planner interaction

1. Numeric-array routines may appear in projections, filters, and computed
   columns.
2. Deterministic scalarized array expressions may participate in expression
   indexes where section `18` and section `23` admit them.
3. Array routines must expose cost and row-width estimates so the optimizer can
   reason about projection and filter cost.

## Explicit exclusions

- sparse tensors beyond the formats admitted by `sb_pkg_sci_udr`
- distributed tensor runtime
- GPU-only execution as a baseline requirement
- arbitrary Python expression evaluation
