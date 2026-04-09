# Planner Unification Physical Property and Access Trust Beta 2 Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 canonical planner front door, the physical-property-aware search contract, and the access-family trust model needed for commercial-grade static optimization.

## One canonical planner API

Beta 2 shall expose one planner front door for canonical lowered statements.

That front door must:

1. accept one frozen rewrite product
2. bind against one canonical cost and statistics identity frame
3. produce one canonical plan candidate set
4. publish one winning plan plus one rejected-alternatives explanation set

No Beta 2 implementation may keep multiple planner pipelines that bypass one another for ordinary SQL planning.

## Canonical planning pipeline

The Beta 2 planning pipeline shall be:

1. accept frozen lowered query
2. derive required physical properties and semantic constraints
3. enumerate base access candidates
4. annotate each candidate with exactness, recheck, ordering, exchange, residency, and parallel posture
5. preserve property-distinct alternatives rather than collapsing to one cheapest base path too early
6. build join, aggregate, limit, sort, exchange, and enforcement candidates
7. compare legal candidates under a unified cost object
8. publish winner and full traceable loser set

## Physical property search requirements

Beta 2 search shall treat these properties as first-class:

- ordering and ordered prefix
- exactness versus exact-plus-recheck versus approximate posture
- exchange and gather posture
- parallel worker count
- late materialization posture
- accelerator and resident-memory posture

The planner must preserve multiple candidates when they differ materially on those properties even when one is temporarily cheaper at an intermediate stage.

## Merge, ordering, and exchange-aware search

Beta 2 must not model merge, gather-merge, or order-preserving execution as purely late wrappers over an already collapsed search space.

The planner shall:

1. retain merge-capable join alternatives during join enumeration
2. retain order-preserving access paths as join and upper-stage inputs
3. compare exchange and gather variants as ordinary search citizens
4. preserve top-N and order-delivery opportunities instead of forcing sort-first heuristics

## Access trust classes

Every admitted access family shall be assigned one canonical trust class:

| Trust class | Meaning | Beta 2 planning treatment |
| --- | --- | --- |
| `EXACT_NATIVE` | family can satisfy the predicate or ordering semantics directly | fully trusted native competitor |
| `EXACT_WITH_RECHECK` | family returns a correct candidate set with mandatory residual or visibility recheck | fully trusted native competitor with explicit recheck cost |
| `APPROXIMATE_GOVERNED` | family can return bounded approximate results only under explicit query semantics or policy | native competitor only where approximate semantics are legal |

No shipped family may be demoted to "secondary" or "manual only" because its trust class is not `EXACT_NATIVE`.

## Native family competition requirements

Beta 2 must allow real competition among:

- ordered and exact families
- summary and pruning families
- columnstore and late-materialization families
- text and ranked inverted families
- spatial and generalized-search families
- exact vector and ANN families
- accelerator-backed variants

The winner rule is not "old dominant family first."
The winner rule is "lowest legal governed cost within the requested semantic contract."

## Mixed workload crossover rules

Beta 2 planning shall explicitly model crossover among:

- point-lookup OLTP work
- medium-selective join work
- scan-heavy analytic work
- order-preserving top-N work
- projection-heavy columnar work
- approximate or ranked search work where legal

For each query class, the planner must retain materially competitive alternatives until the crossover decision can be explained by cost, exactness, and resource posture.

## Parallel path rule

Parallel path variants shall appear during ordinary search, not as a disconnected late-stage experiment.

Parallel alternatives must include:

- scan variants
- join variants
- partial aggregate and final aggregate variants
- exchange and gather or gather-merge variants

## Diagnostics requirements

A Beta 2 planner trace must be able to explain:

- why a property-distinct candidate was preserved
- why a merge, order-preserving, or parallel path lost
- why a family was trusted as native, trusted with recheck, or refused
- why a mixed-workload crossover favored one family over another

## Cross-section ownership

- section `18` owns family-local planner semantics and metrics payloads
- section `23` owns plan-cache and execution publication rules
- this file owns Beta 2 planner unification and cross-family search behavior

## Non-guarantees

- this file does not claim current implementation already satisfies full Beta 2 search behavior
- this file does not authorize approximate families where the query contract requires exact semantics
