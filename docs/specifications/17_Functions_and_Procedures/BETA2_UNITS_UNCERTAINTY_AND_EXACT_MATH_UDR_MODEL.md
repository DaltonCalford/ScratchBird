# Beta 2 Units Uncertainty And Exact Math UDR Model

## Purpose

This document defines the measurement-safety, uncertainty-propagation, and
arbitrary-precision UDR families used to make scientific, engineering,
financial, and educational calculations safer and more reproducible.

This group is the ScratchBird-native replacement target for the highest-value
portions of `Pint`, `uncertainties`, and `mpmath`.

## Owning packages

- `sb_pkg_units_udr`
- `sb_pkg_exact_math_udr`

## Mandatory surfaces

### Units and dimensions

`sb_pkg_units_udr` shall provide:

- unit declaration and lookup
- dimension checking
- quantity construction
- unit conversion
- unit-aware arithmetic
- unit-aware formatting and string casts

### Uncertainty propagation

The package shall provide:

- uncertain quantity construction
- mean/stddev or interval-based uncertainty payloads
- propagation through admitted arithmetic and function families
- extraction of nominal value, variance, stddev, and interval

### Exact and arbitrary-precision math

`sb_pkg_exact_math_udr` shall provide:

- arbitrary-precision floating arithmetic
- arbitrary-precision transcendental functions
- interval arithmetic for the admitted subset
- exact rational helpers
- controlled-precision linear algebra for the admitted subset

## Required routine families

At minimum the following families shall exist:

- `sb_units.quantity(...)`
- `sb_units.convert(...)`
- `sb_units.dimension_of(...)`
- `sb_units.uncertain(...)`
- `sb_units.nominal(...)`
- `sb_units.stddev(...)`
- `sb_exact_math.mp_*`
- `sb_exact_math.interval_*`
- `sb_exact_math.rational_*`

## Example contract

```sql
select sb_units.convert(
    sb_units.quantity(100, 'km/h'),
    'm/s'
);

select sb_exact_math.mp_exp('1.234567890123456789', precision_digits => 80);
```

## Execution rules

1. Unit checking shall fail closed on dimension mismatch.
2. Precision must be explicit for arbitrary-precision surfaces when the default
   session precision is not used.
3. Exact-math routines shall publish precision cost estimates to the optimizer
   and executor metrics layer.
4. Educational-safe renderings may round for display, but the underlying value
   must remain exact or explicitly precision-bounded.

## Explicit exclusions

- symbolic theorem proving
- arbitrary custom unit systems without admission and namespace control
- unbounded exact-math workloads without precision or runtime ceilings
