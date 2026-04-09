# Beta 2 Optimizer Certification and Regression Gates

Status: reconstructed_required_beta2

## Purpose

Define the certification and regression gate families required before ScratchBird may claim any Beta 2 optimizer maturity band.

## Gate families

### `OG-B2-01` Planner unification and physical-property search

Required proof:

- one canonical planner front door
- property-distinct candidate retention
- merge, ordering, exchange, and parallel alternative evidence
- deterministic winner and loser explanation output

### `OG-B2-02` Access-family trust and mixed workload competition

Required proof:

- every shipped family is admitted as a primary optimizer class
- exact, exact-plus-recheck, and approximate trust classes are disclosed correctly
- summary, columnstore, text, spatial, vector, and ordered families compete on supported query shapes
- mixed OLTP, analytic, and top-N crossover cases emit stable reasoning

### `OG-B2-03` Plan store, baselines, and regression governance

Required proof:

- persisted plan history
- stable plan hash and normalized query identity
- baseline and forcing legality checks
- regression classification with blessed baseline comparison
- CE version and confidence disclosure

### `OG-B2-04` Adaptive processing and grant correction

Required proof:

- adaptive branch publication
- threshold disclosure
- grant requested, granted, used, and spill outcome evidence
- interleaved-execution branch safety evidence
- safe refusal when adaptive behavior is ineligible

### `OG-B2-05` Parameter-sensitive and workload-aware optimization

Required proof:

- deterministic parameter regime bucketing
- bounded multi-plan cache growth
- persisted feedback identity
- DOP feedback evidence
- workload-governance integration evidence

## Required artifact set

Every Beta 2 optimizer gate run shall emit:

- normalized query corpus manifest
- plan publication manifest
- plan hash and plan-shape comparison report
- CE confidence and fallback report
- spill and memory grant report
- per-family winner and loser trace bundle
- regression disposition report
- parameter regime and multi-plan selection report where applicable
- DOP efficiency report where parallel behavior is claimed

## Advancement rules

1. A maturity-band claim may only be made after the matching gate families pass.
2. No Beta 2 optimizer claim may waive correctness, MGA, or security failures.
3. A family-trust claim is invalid if any shipped family still requires silent omission or manual-only optimizer routing.
4. Adaptive or PSP claims are invalid if the corresponding runtime trace fields are missing.

## Maturity-band mapping

| Beta 2 band | Required gates |
| --- | --- |
| `B2-M1` Commercial Static Optimizer | `OG-B2-01`, `OG-B2-02` |
| `B2-M2` Commercial Governed Optimizer | `OG-B2-01`, `OG-B2-02`, `OG-B2-03` |
| `B2-M3` Commercial Adaptive Optimizer | `OG-B2-01` through `OG-B2-04` |
| `B2-M4` Commercial Self-Tuning Optimizer | `OG-B2-01` through `OG-B2-05` |

## Non-authority rule

This file defines the required gate model for Beta 2 optimizer claims.
It does not claim those executed gate runners already exist today.
