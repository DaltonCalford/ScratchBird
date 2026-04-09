# Beta 2 Probability Distribution And Stochastic Modeling UDR Model

## Purpose

This document defines the probability, distribution, and stochastic-modeling
UDR family used by science, finance, simulation, and education workloads.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `Distributions.jl`.

## Owning package

- `sb_pkg_prob_udr`

## Dependencies

This package depends on:

- `sb_pkg_num_array_udr`
- `sb_pkg_stats_udr`
- `sb_pkg_exact_math_udr`

## Mandatory surfaces

The package shall provide:

- distribution construction for the admitted common families
- pdf/pmf, cdf, inverse cdf, mean, variance, skew, kurtosis
- random sampling
- fitting helpers for the admitted bounded families
- mixture support for the admitted subset
- stochastic process helpers for the admitted subset

## Admitted common families

Beta 2 shall at minimum admit:

- normal
- lognormal
- uniform
- bernoulli
- binomial
- poisson
- exponential
- gamma
- beta
- student-t
- multivariate normal

## Required routine families

- `sb_prob.dist_*`
- `sb_prob.pdf_*`
- `sb_prob.cdf_*`
- `sb_prob.icdf_*`
- `sb_prob.sample_*`
- `sb_prob.fit_*`

## Example contract

```sql
select sb_prob.cdf_normal(x => 1.96, mean => 0.0, stddev => 1.0);
```

## Operational rules

1. Sampling routines shall require explicit seed when determinism is required.
2. Fitting routines shall return structured fit quality metadata.
3. Distribution objects may be transient or stored as typed artifacts.
4. Distribution and process routines shall expose parameter bounds and
   validation failures explicitly.

## Explicit exclusions

- unrestricted probabilistic programming
- MCMC frameworks as a baseline Beta 2 requirement
- automatic differentiation as a user-facing requirement
