# MongoDB Audit

## Architectural Summary

MongoDB is a strong donor for candidate-plan generation, index tagging, memo-based plan enumeration, and execution-engine lowering. It handles a wider set of special index rules than many SQL engines because it must reason about multikey, sparse, partial, wildcard, text, geo, and clustered collection cases inside one planner family.

## Planning Flow

1. Query is canonicalized into a `CanonicalQuery`.
2. Heuristic rewrites simplify or normalize the predicate tree.
3. `QueryPlanner::plan()` starts index tagging.
4. Index selection logic identifies indexable fields and rates compatible indexes for each predicate.
5. A rated predicate tree feeds the `PlanEnumerator`.
6. The plan enumerator builds a memo of reusable subplans and generates tagged plan combinations.
7. Data-access plans and sort/coverage analysis turn those combinations into `QuerySolution` candidates.
8. The multiplanner or cost-based ranker chooses a winning plan.
9. The winner is lowered into SBE or classic execution stages.

## How MongoDB Uses Indexes

MongoDB’s key strength is that index use is rule-rich:

- sparse indexes only where semantics allow
- multikey restrictions and array semantics
- wildcard expansion and filtering
- partial-index filter compatibility
- text and geo indexes with dedicated rules
- clustered scans and hinted plans
- index intersections and OR subplanning

This is not “one index matcher.” It is a compatibility engine plus a plan generator.

## Plan Competition and Caching

- MongoDB does not just pick one plan statically; it can trial multiple candidates.
- Classic and SBE plan caches keep shape-sensitive decisions.
- Cost-based ranking is increasingly present, but runtime multiplanning remains an important discipline.

## Visibility and Transaction Interaction

MongoDB’s planner is not MGA-centric, but it still respects storage constraints:

- snapshot semantics come from storage/executor
- multikey metadata changes what an index can mean
- clustered and collection scans are planner-visible choices

The most important donor lesson is not transaction truth, but plan-shape correctness under rich index metadata.

## What ScratchBird Should Borrow

- Index tagging before full plan enumeration
- Explicit compatibility filtering for advanced families
- Memo-based plan generation where combinations are reusable
- Plan cache invalidation keyed to family semantics, not just chosen leaf path

## ScratchBird Comparison Hooks

- Compare ScratchBird advanced family legality rules to MongoDB’s multikey/sparse/partial/wildcard handling.
- Compare ScratchBird future candidate-plan search to MongoDB’s tag -> enumerate -> rank pipeline.
