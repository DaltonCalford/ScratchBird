# Commercial Plan Store Baselines and Regression Governance Beta 2 Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 persisted optimizer history, plan baseline, plan forcing, and regression-governance model.

## Distinction from the immutable plan cache

The Beta 2 plan store is not the same thing as the immutable plan cache defined in section `23`.

The immutable plan cache owns:

- cache key validation
- immutable published plan values
- hit, miss, insert, and invalidation behavior

The Beta 2 plan store owns:

- persisted plan history
- runtime outcome recording
- regression classification
- baseline and forcing policy
- optimizer governance reporting

No implementation may silently mutate a live cache entry in order to express baseline or feedback state.

## Required persisted row families

Beta 2 must persist at least these logical row families:

1. normalized query identity rows
2. plan publication rows
3. execution outcome rows
4. regression event rows
5. baseline and forcing policy rows
6. CE-governance rows

## Minimum plan publication fields

Every persisted plan publication row shall preserve:

- normalized query id
- canonical lowered query or statement shape id
- plan hash
- plan shape identity
- chosen family set
- estimated rows and cost summary
- cost profile id
- statistics snapshot id
- catalog epoch
- security epoch
- planner profile id
- engine version and capability profile

## Minimum execution outcome fields

Every persisted execution outcome row shall preserve:

- plan hash
- parameter regime signature
- actual row counts
- elapsed time summary
- memory grant requested
- memory grant used
- spill incidents
- recheck burden
- chosen DOP and actual worker efficiency where applicable
- fallback reason or degraded-execution reason when present

## Baseline and forcing model

Beta 2 shall support these policy states:

| State | Meaning |
| --- | --- |
| `NONE` | no baseline or forcing policy is active |
| `PREFERRED_BASELINE` | planner should prefer this plan when legality and freshness rules still hold |
| `FORCED_BASELINE` | planner must choose this plan unless legality or safety checks refuse it |
| `QUARANTINED` | plan is known-bad and must not be chosen automatically |

### Baseline legality rules

A baseline may only be reused when:

- schema and capability identity still match
- statistics and cost-profile state are within the declared freshness envelope
- security and role context remain compatible
- exactness and semantic contract still match

If those checks fail, the baseline must be refused with an explicit reason code.

## Regression classification model

Beta 2 regression governance shall classify at least:

- plan-shape regression
- CE regression
- spill regression
- DOP regression
- parameter-skew regression
- family-trust regression

Each regression event shall record:

- prior blessed baseline id
- observed regressed plan id
- workload class
- severity
- disposition state

## CE governance requirements

Beta 2 optimizer governance must preserve:

- CE model version
- confidence class
- sampled-refresh usage
- multivariate usage
- fallback heuristic usage
- explicit reason for low-confidence estimates

## Query-store-equivalent rule

Beta 2 must provide a persisted optimizer history facility equivalent in role to a commercial query store.

That facility must support:

- historical plan inspection
- comparison against prior baselines
- regression detection after stats or engine changes
- audit of plan forcing or baseline changes

## Operational rules

1. plan-store persistence must be append-safe and queryable
2. forcing decisions must be auditable
3. automatic forcing, if enabled later, must be explicitly policy-bound
4. old history retention and pruning must preserve at least one blessed baseline lineage per governed query

## Non-guarantees

- this file does not require current Alpha cache behavior to become mutable
- this file does not claim current ScratchBird already has a full query-store-equivalent implementation
