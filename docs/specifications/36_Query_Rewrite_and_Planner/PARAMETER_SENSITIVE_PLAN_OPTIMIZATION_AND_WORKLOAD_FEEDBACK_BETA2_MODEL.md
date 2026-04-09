# Parameter Sensitive Plan Optimization and Workload Feedback Beta 2 Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 parameter-sensitive multi-plan, persisted feedback, DOP-feedback, and workload-aware optimizer model.

## Parameter-sensitive plan model

Beta 2 shall allow more than one cached or persisted plan per normalized query when runtime parameter regimes differ materially.

The governing objects are:

- normalized query identity
- parameter regime bucket
- plan set
- selection policy

## Parameter regime bucketing

A parameter regime bucket must be derived from stable, explainable properties such as:

- selectivity class
- key skew or hot-value class
- range-width class
- result-size class

Buckets must not be opaque or unbounded.

## Multi-plan cache rules

When Beta 2 enables PSP:

1. each normalized query may own multiple legal plans
2. each plan must declare its admitted regime bucket set
3. runtime selection must choose the bucket-matching plan deterministically
4. bounded cache growth is mandatory
5. stale or low-confidence bucket splits must be mergeable or removable by policy

## Persisted optimizer feedback families

Beta 2 feedback persistence shall include at least:

- CE correction feedback
- memory grant feedback
- spill feedback
- family-specific cost correction feedback
- parameter regime behavior feedback
- DOP efficiency feedback

## DOP feedback

For parallel-capable governed queries, Beta 2 shall record:

- chosen DOP
- actual worker efficiency
- parallel overhead burden
- gather or exchange overhead
- recommended DOP adjustment posture

Future plan selection may use that feedback only through an explicit policy-governed path.

## Workload-aware resource integration

Optimizer choices must consume workload-governance outcomes where available, including:

- memory class
- concurrency class
- queue priority
- admission result
- accelerator or residency availability

The optimizer must not pretend unlimited resources when governance has already constrained the statement.

## Automatic tuning loop

Beta 2 may expose an automatic tuning loop only if it is bounded by explicit policy.

Allowed tuning actions are:

- suggest baseline adoption
- suggest stats refresh
- suggest family-promotion or family-health investigation
- suggest index or summary design changes
- auto-force a plan only when policy explicitly permits it

Every automatic action must be:

- auditable
- reversible
- attributable to a triggering evidence set

## Safety rules

1. PSP must not break semantic determinism for the same parameter regime and same planning envelope.
2. Feedback must not silently rewrite a published historical plan record.
3. Automatic tuning must not override explicit operator policy without traceable authority.
4. Workload-governance outcomes remain authoritative over optimizer ambition.

## Diagnostics requirements

A Beta 2 PSP or feedback-capable system must be able to explain:

- why a parameter regime bucket was chosen
- why one plan in a multi-plan set won
- what persisted feedback influenced the choice
- whether DOP feedback or workload governance constrained the winner

## Non-guarantees

- this file does not claim the current engine already supports PSP or persisted optimizer feedback
- this file does not require autonomous tuning to be enabled by default
