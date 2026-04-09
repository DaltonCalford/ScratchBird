# Beta 2 Science Verticals And Education UDR Model

## Purpose

This document defines the optional but admitted Beta 2 vertical UDR families
for astronomy, chemistry, and education-focused mathematical workflows.

This group uses `Astropy`, `RDKit`, and `SageMath` as design references.

## Owning packages

- `sb_pkg_astro_udr`
- `sb_pkg_chem_udr`
- `sb_pkg_edu_math_udr`

## Vertical package rules

1. These packages are domain overlays over the analytical and exact-math base.
2. None of these packages may duplicate shared functionality that already
   belongs in the numeric, stats, symbolic, graph, or exact-math packages.
3. Vertical packages shall focus on domain objects, domain transforms, and
   domain-safe result contracts.

## Astronomy package

`sb_pkg_astro_udr` shall provide:

- time-scale aware astronomy time handling
- coordinate transforms for the admitted coordinate systems
- units integration through `sb_pkg_units_udr`
- table and catalog matching helpers for the admitted subset

Required routine families:

- `sb_astro.time_*`
- `sb_astro.coord_*`
- `sb_astro.match_*`

## Chemistry package

`sb_pkg_chem_udr` shall provide:

- molecule parse and normalization for admitted representations
- descriptor extraction
- fingerprint generation
- similarity helpers

Required routine families:

- `sb_chem.parse_*`
- `sb_chem.descriptor_*`
- `sb_chem.fingerprint_*`
- `sb_chem.similarity_*`

## Education math package

`sb_pkg_edu_math_udr` shall provide:

- stable pedagogical wrappers over symbolic, exact, graph, and probability
  surfaces
- stepwise derivation renderings for the admitted subset
- education-safe formatting
- curriculum-oriented helper routines for algebra, calculus, linear algebra,
  graph theory, and probability

Required routine families:

- `sb_edu_math.solve_stepwise(...)`
- `sb_edu_math.diff_stepwise(...)`
- `sb_edu_math.integrate_stepwise(...)`
- `sb_edu_math.graph_example_*`
- `sb_edu_math.probability_example_*`

## Example contract

```sql
select *
from sb_edu_math.solve_stepwise('2*x + 3 = 11');
```

## Explicit exclusions

- telescope/device control
- chemistry rendering/GUI toolkits
- unrestricted proof-assistant or computer-algebra environments
