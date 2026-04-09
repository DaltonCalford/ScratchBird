# Beta 2 Python-Inspired UDR Program And Staging Model

## Purpose

This document defines the Beta 2 canonical package program for scientific,
mathematical, analytical, and columnar user-defined runtime surfaces that are
intended to cover the non-graphical and non-network workloads for which users
would otherwise commonly reach for Python.

The goal is not to embed Python or reproduce the entire Python ecosystem.
The goal is to admit the highest-value scientific and analytical capabilities
as native ScratchBird UDR package families that execute inside ScratchBird's
catalog, compiler, executor, security, memory-budget, and metrics model.

## Canonical Beta 2 package groups

The admitted Beta 2 package groups are:

1. Core numeric runtime:
   - `sb_pkg_num_array_udr`
   - `sb_pkg_expr_udr`
2. Scientific and statistical computing:
   - `sb_pkg_sci_udr`
   - `sb_pkg_stats_udr`
3. Symbolic and formula engine:
   - `sb_pkg_symbolic_udr`
4. Optimization and solver services:
   - `sb_pkg_opt_udr`
5. Columnar and interchange layer:
   - `sb_pkg_arrow_udr`
6. Machine-learning utilities and inference:
   - `sb_pkg_ml_udr`
7. Labeled N-D and coordinate-aware analytics:
   - `sb_pkg_nd_udr`
8. Bayesian modeling and inference:
   - `sb_pkg_bayes_udr`

These groups are modeled from the high-value portions of the Python
ecosystem commonly represented by `NumPy`, `numexpr`, `SciPy`,
`statsmodels`, `SymPy`, `OR-Tools`, `CVXPY`, `PyArrow`,
`scikit-learn`, `xarray`, and `PyMC`.

## Explicit non-goals

Beta 2 does not authorize:

- embedding a general CPython runtime inside the engine
- admitting a generic `pip`-style package installer
- GUI, plotting, notebook, or visualization surfaces
- network or HTTP client surfaces
- a full `pandas` or `polars` clone
- an unrestricted probabilistic programming runtime outside the admitted
  bounded Bayesian package
- unrestricted file-system or process-spawn access from analytical UDRs

## Shared package rules

All packages in this program shall obey the following rules:

1. Every package shall be a normal ScratchBird language UDR package and shall
   use the ordinary catalog registration, capability hash, sandbox, and quota
   fences defined for section `17`.
2. No package may require a Python interpreter at runtime.
3. No package may open arbitrary network connections.
4. No package may render graphics, windows, or plotting surfaces.
5. Every routine shall publish determinism class, memory class, spill
   eligibility, and vectorization eligibility as package metadata.
6. Every routine shall expose stable SQL-callable names under the `sb_*`
   namespace and stable package-local names under the owning `sb_pkg_*_udr`
   package.
7. Every package shall provide direct scalar entry points, table-oriented
   entry points where appropriate, and array/vector entry points where
   appropriate.
8. Every package shall emit metrics compatible with section `20`, section `23`,
   section `33`, and section `38`, including invocation count, cpu time,
   peak memory, spill bytes, and vector-lane utilization where applicable.
9. Every package shall fail closed when an operation would require a
   disallowed runtime capability, external code execution, or unbounded
   resource growth.

## Shared type and result rules

1. Package surfaces shall use admitted ScratchBird scalar, complex, and Beta 2
   expansion types only.
2. Packages may consume and return:
   - scalar numerics
   - decimal numerics
   - boolean
   - temporal types
   - `VECTOR`
   - typed `ARRAY`
   - `STRUCT`
   - `JSON`
   - table rowsets
   - model, expression, or artifact `BLOB` payloads when explicitly defined
3. `STRING` and `BLOB` interchange shall be provided where human-readable or
   serialized import/export is expected.
4. `NULL`, `NaN`, infinity, missing-value, and masked-value semantics shall be
   explicit per package and may not rely on hidden Python behavior.

## Shared execution classes

Each routine shall declare one of the following execution classes:

- `scalar_deterministic`
- `vectorized_deterministic`
- `stateful_fit`
- `stateful_inference`
- `bounded_solver`
- `artifact_translation`

The execution class controls admission, memory grants, spill eligibility,
parallelism, and timeout policy.

## Beta 2 staged rollout

All admitted work in this document is Beta 2 work. The rollout order is:

### Stage 1 - Foundation

- `sb_pkg_num_array_udr`
- `sb_pkg_expr_udr`
- `sb_pkg_arrow_udr`

Stage 1 creates the array, vector, expression, and columnar interchange
substrate required by all later stages.

### Stage 2 - Scientific and statistical

- `sb_pkg_sci_udr`
- `sb_pkg_stats_udr`
- `sb_pkg_nd_udr`

Stage 2 depends on Stage 1 and adds higher-order numerical, statistical,
forecasting, interpolation, sparse workloads, and coordinate-aware labeled
datasets.

### Stage 3 - Symbolic and optimization

- `sb_pkg_symbolic_udr`
- `sb_pkg_opt_udr`

Stage 3 depends on Stage 1. It may consume Stage 2 routines but may not block
Stage 2 completion.

### Stage 4 - Machine-learning utilities and inference

- `sb_pkg_ml_udr`

Stage 4 depends on Stage 1 and may consume surfaces from Stage 2 and Stage 3.
Model fitting is admitted only where deterministic, bounded, and operationally
safe. Inference is mandatory. Full open-ended training frameworks are not.

### Stage 5 - Bayesian modeling and inference

- `sb_pkg_bayes_udr`

Stage 5 depends on Stage 2, Stage 3, and Stage 4. Bayesian inference is
admitted only through bounded algorithms with explicit runtime, draw, chain,
and memory ceilings.

## Required package deliverables

Every package group in this program shall ship:

1. catalog package definition
2. capability manifest
3. routine inventory
4. input/output type matrix
5. determinism and quota matrix
6. observability and metrics contract
7. executor and memory-budget integration proof plan
8. failure and error-code matrix
9. SQL examples and migration notes for Python users

## Cross-section dependencies

- section `13` for coercion, casts, and extract/set behavior
- section `14` and section `15` for scalar and complex datatype surfaces
- section `20` for diagnostics and metrics
- section `21` and section `22` where package-generated code or expressions are
  lowered through parser or SBLR structures
- section `23` for execution classes, compiled kernels, and plan integration
- section `33` for memory grants, quotas, arenas, and spill policy
- section `39` for columnar import/export and bulk path coordination

## Mandatory outcome

Beta 2 shall provide a coherent native ScratchBird analytical stack so that
common scientific, mathematical, statistical, optimization, and lightweight
machine-learning tasks can be performed inside ScratchBird without depending on
an external Python process.

## Domain expansion note

The domain-oriented Beta 2 extension pack for finance, units, exact math,
differential equations, graph science, probability, science verticals, and
education overlays is defined separately in
`BETA2_DOMAIN_FINANCE_SCIENCE_AND_EDUCATION_UDR_PROGRAM_MODEL.md` and depends
on this core analytical program.
