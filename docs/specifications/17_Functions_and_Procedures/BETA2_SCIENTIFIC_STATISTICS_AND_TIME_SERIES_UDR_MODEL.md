# Beta 2 Scientific Statistics And Time-Series UDR Model

## Purpose

This document defines the higher-order numerical, sparse, statistical,
econometric, and forecasting UDR families that extend the Stage 1 numeric
substrate.

This group is the ScratchBird-native replacement target for the highest-value
portions of `SciPy` and `statsmodels`.

## Owning packages

- `sb_pkg_sci_udr`
- `sb_pkg_stats_udr`

## Stage dependency

This group depends on:

- `sb_pkg_num_array_udr`
- `sb_pkg_expr_udr`

## Mandatory scientific surfaces

`sb_pkg_sci_udr` shall provide:

- root finding
- numerical optimization for bounded local problems
- interpolation
- numerical integration
- special functions commonly used in scientific and statistical workflows
- sparse matrix construction and algebra for admitted sparse formats
- distance and similarity helpers for numerical arrays
- signal transforms required for non-graphical analytical workflows

## Mandatory statistical surfaces

`sb_pkg_stats_udr` shall provide:

- descriptive statistics
- distributions and p-value helpers
- hypothesis tests
- correlation and covariance families
- linear regression
- generalized linear models
- ANOVA-class helpers
- time-series decomposition
- AR, MA, ARIMA, and related bounded forecasting surfaces

## Required routine families

At minimum the following families shall exist:

- `sb_sci.integrate_*`
- `sb_sci.interpolate_*`
- `sb_sci.optimize_*`
- `sb_sci.root_*`
- `sb_sci.sparse_*`
- `sb_stats.describe(...)`
- `sb_stats.test_*`
- `sb_stats.regress_*`
- `sb_stats.glm_*`
- `sb_stats.forecast_*`

## Example contract

```sql
select *
from sb_stats.regress_ols(
    x_columns => array['income', 'age', 'score'],
    y_column => 'spend',
    source_query => 'select income, age, score, spend from analytics.training'
);

select sb_sci.interpolate_linear(
    x => :x_known,
    y => :y_known,
    x_target => :x_eval
);
```

## Output rules

1. Statistical models shall return structured result payloads, not opaque
   human-readable text.
2. Structured outputs shall include:
   - coefficients
   - fit metadata
   - error metrics
   - convergence or termination status
   - warnings
3. Forecasting routines shall emit forecast vectors plus confidence intervals
   where mathematically supported.

## Operational rules

1. Model-fitting routines shall use the `stateful_fit` execution class.
2. Pure descriptive or hypothesis-test routines may use
   `vectorized_deterministic`.
3. Long-running optimization or fitting routines shall publish iteration count,
   convergence state, and termination reason metrics.
4. Sparse routines shall preserve admitted storage formats and may not densify
   silently past configured memory limits.

## Explicit exclusions

- unconstrained distributed scientific computing
- unrestricted notebook-style scripting
- black-box auto-ml
- arbitrary external solver plugins that bypass ScratchBird quota and sandbox
  control
