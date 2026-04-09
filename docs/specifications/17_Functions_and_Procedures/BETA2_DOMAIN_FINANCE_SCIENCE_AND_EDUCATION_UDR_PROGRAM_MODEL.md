# Beta 2 Domain Finance Science And Education UDR Program Model

## Purpose

This document defines the Beta 2 domain-oriented UDR expansion pack that
extends the core analytical UDR stack with the highest-value surfaces for
financial analysis, scientific computation, engineering safety, and education.

This program sits on top of the Beta 2 analytical base defined by:

- `sb_pkg_num_array_udr`
- `sb_pkg_expr_udr`
- `sb_pkg_sci_udr`
- `sb_pkg_stats_udr`
- `sb_pkg_symbolic_udr`
- `sb_pkg_opt_udr`
- `sb_pkg_arrow_udr`
- `sb_pkg_ml_udr`
- `sb_pkg_nd_udr`
- `sb_pkg_bayes_udr`

## Package groups

The admitted Beta 2 domain package groups are:

1. finance and risk:
   - `sb_pkg_fin_udr`
2. units, uncertainty, and exact math:
   - `sb_pkg_units_udr`
   - `sb_pkg_exact_math_udr`
3. differential equations and simulation:
   - `sb_pkg_diff_eq_udr`
4. graph science and network analysis:
   - `sb_pkg_graph_udr`
5. probability distributions and stochastic modeling:
   - `sb_pkg_prob_udr`
6. autodiff and differentiable kernels:
   - `sb_pkg_autodiff_udr`
7. science verticals and education overlays:
   - `sb_pkg_astro_udr`
   - `sb_pkg_chem_udr`
   - `sb_pkg_edu_math_udr`

## Reference inspiration

This program is modeled from the highest-value portions of:

- `QuantLib`
- `arch`
- `rugarch`
- `PyPortfolioOpt`
- `PerformanceAnalytics`
- `Pint`
- `uncertainties`
- `mpmath`
- `DifferentialEquations.jl`
- `NetworkX`
- `Distributions.jl`
- `Astropy`
- `RDKit`
- `SageMath`
- `JAX` as an internal differentiation and kernel-design reference only

## Shared rules

1. These packages are extensions of the section `17` UDR platform and shall
   obey the same registration, sandbox, quota, and metrics rules as the
   analytical Beta 2 base.
2. No package may require a general Python, Julia, or R runtime in the engine.
3. No package may bypass ScratchBird memory governance, timeout policy, or
   diagnostics policy.
4. Domain packages shall build on shared base artifacts instead of creating
   isolated incompatible runtimes.
5. Domain packages shall publish deterministic vs stochastic behavior
   explicitly.
6. Domain packages shall expose SQL-facing names under stable `sb_*`
   namespaces and stable package-local routines under their owning package id.

## Beta 2 stage order

### Stage 6 - Safety and exactness

- `sb_pkg_units_udr`
- `sb_pkg_exact_math_udr`
- `sb_pkg_prob_udr`

Stage 6 creates the correctness and stochastic substrate used by finance,
engineering, and education.

### Stage 7 - Finance and graph workloads

- `sb_pkg_fin_udr`
- `sb_pkg_graph_udr`

Stage 7 depends on Stage 6 and the analytical base.

### Stage 8 - Scientific simulation

- `sb_pkg_diff_eq_udr`

Stage 8 depends on the analytical base and may consume exact math, units, and
probability surfaces.

### Stage 9 - Differentiation and calibration

- `sb_pkg_autodiff_udr`

Stage 9 depends on the analytical base and may be consumed by finance,
simulation, optimization, and machine-learning packages.

### Stage 10 - Science verticals and education

- `sb_pkg_astro_udr`
- `sb_pkg_chem_udr`
- `sb_pkg_edu_math_udr`

Stage 10 depends on the earlier stages but does not block them.

## Cross-section dependencies

- section `13` for coercion, casts, units/text conversion, and extract/set
- section `14` and section `15` for scalar, vector, array, struct, and blob
  surfaces
- section `18` for graph, geospatial, and science-adjacent index integration
- section `20` for diagnostics and metrics
- section `21`, section `22`, and section `23` for compiled kernels,
  expression lowering, symbolic codegen, and execution classes
- section `33` for exact-math memory policy, simulation budgets, and solver
  quotas

## Mandatory outcome

Beta 2 shall expose a coherent domain-oriented UDR platform so ScratchBird can
cover a meaningful portion of the financial, educational, and scientific
workflow space without requiring a separate external analytical application
stack for common tasks.

## Related extension note

The cross-industry Beta 2 extension pack for geospatial, text/document,
calendar, contract, quality, rules, healthcare, imaging, and matching
workloads is defined separately in
`BETA2_INDUSTRY_INTERCHANGE_GOVERNANCE_AND_WORKFLOW_UDR_PROGRAM_MODEL.md`.
