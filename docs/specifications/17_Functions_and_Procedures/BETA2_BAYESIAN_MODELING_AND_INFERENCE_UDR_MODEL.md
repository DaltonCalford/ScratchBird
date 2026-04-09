# Beta 2 Bayesian Modeling And Inference UDR Model

## Purpose

This document defines the bounded Bayesian modeling and inference UDR family
for ScratchBird.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `PyMC`, while remaining within ScratchBird's bounded
execution, quota, and audit model.

## Owning package

- `sb_pkg_bayes_udr`

## Dependencies

This package depends on:

- `sb_pkg_prob_udr`
- `sb_pkg_stats_udr`
- `sb_pkg_num_array_udr`
- `sb_pkg_autodiff_udr`
- `sb_pkg_ml_udr`

## Mandatory surfaces

The package shall provide:

- bounded Bayesian model definition
- prior specification for the admitted distributions
- posterior inference for the admitted algorithm set
- posterior summary and diagnostics
- posterior predictive sampling
- model serialization and reuse

## Admitted inference families

Beta 2 shall admit only bounded inference families with explicit runtime and
memory ceilings, including:

- conjugate closed-form updates where available
- bounded variational inference for the admitted subset
- bounded MCMC for the admitted subset with explicit chain, draw, and timeout
  ceilings

## Required routine families

At minimum the following families shall exist:

- `sb_bayes.model_create(...)`
- `sb_bayes.add_prior(...)`
- `sb_bayes.fit_vi(...)`
- `sb_bayes.fit_mcmc(...)`
- `sb_bayes.posterior_summary(...)`
- `sb_bayes.posterior_predict(...)`
- `sb_bayes.model_save(...)`
- `sb_bayes.model_load(...)`

## Example contract

```sql
select *
from sb_bayes.posterior_summary(
    model_artifact => :bayes_model
);
```

## Operational rules

1. Every inference request shall declare or inherit:
   - algorithm id
   - max runtime
   - max draws/iterations
   - max chains where applicable
   - memory ceiling
   - seed policy
2. Inference shall emit structured diagnostics including convergence or
   termination status, acceptance metrics where applicable, and posterior fit
   quality indicators.
3. Bayesian routines may not run as unbounded open-ended jobs through the
   ordinary function surface.

## Explicit exclusions

- unrestricted probabilistic programming environments
- arbitrary user-defined samplers without admission control
- GPU-dependent Bayesian runtimes as a baseline requirement
