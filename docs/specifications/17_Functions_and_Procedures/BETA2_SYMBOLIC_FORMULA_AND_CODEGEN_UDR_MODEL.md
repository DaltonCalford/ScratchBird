# Beta 2 Symbolic Formula And Codegen UDR Model

## Purpose

This document defines the symbolic mathematics, formula transformation, and
safe code-generation surfaces for ScratchBird.

This group is the ScratchBird-native replacement target for the highest-value
portions of `SymPy`.

## Owning package

- `sb_pkg_symbolic_udr`

## Mandatory surfaces

The package shall provide:

- symbolic expression construction
- simplify and normalize
- substitution
- differentiate
- integrate for the admitted symbolic subset
- solve for algebraic systems in the admitted subset
- series expansion
- matrix symbolic helpers for the admitted subset
- units and dimension-consistency helpers where a unit model is supplied
- code generation to validated ScratchBird routine bodies or expression kernels

## Required routine families

At minimum the following routine families shall exist:

- `sb_symbolic.parse(...)`
- `sb_symbolic.simplify(...)`
- `sb_symbolic.substitute(...)`
- `sb_symbolic.diff(...)`
- `sb_symbolic.integrate(...)`
- `sb_symbolic.solve(...)`
- `sb_symbolic.series(...)`
- `sb_symbolic.codegen_expr(...)`
- `sb_symbolic.codegen_routine(...)`

## Example contract

```sql
select sb_symbolic.simplify('sin(x)^2 + cos(x)^2');

select sb_symbolic.codegen_expr(
    expr => '3*x^2 + 2*x + 1',
    target => 'sblr_kernel'
);
```

## Code-generation rules

1. Generated code may target only:
   - a validated expression kernel
   - a validated stored function body
   - a validated projection/filter helper artifact
2. Generated code may not bypass section `21`, section `22`, or section `23`
   validation.
3. Generated code must pass the same whitelist, type-admission, and quota
   checks as human-authored code.
4. Symbolic code generation may not emit unrestricted imperative host code.

## Result rules

1. Symbolic expressions shall have a canonical serialized representation.
2. Structural equality and simplified equality shall be distinct operations.
3. Parse failures, unsupported operators, and non-admitted symbolic domains
   shall fail closed with structured diagnostics.

## Explicit exclusions

- unrestricted computer algebra beyond the admitted subset
- arbitrary theorem-proving or proof-assistant workflows
- host-language code generation outside the ScratchBird validation pipeline
