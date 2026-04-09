# Logical Transformation Catalog

This file defines the exact logical transformation classes allowed under section 36.

## Transformation catalog

| Transformation class | Required status | Current implementation posture | Semantic boundary |
| --- | --- | --- | --- |
| semantic normalization | required | active | canonicalize equivalent query forms into a stable internal shape |
| predicate shape classification | required | active | classify predicate forms for matcher and lowering stages without changing truth conditions |
| index-family lowering | required | active | map generalized access intent into a planner-family lowering result, including queryability state and recheck requirements |
| common subexpression analysis | required | active | identify duplicate or reusable expression structure for reuse or elimination passes |
| common subexpression elimination | bounded | active where invoked | remove duplicated expression work without changing semantic output |
| runtime payload plan annotation | required | active | inject chosen runtime plan metadata into emitted payloads after selection |
| materialized-view substitution | fail-closed | surface exists but rewrite returns no replacement plan today | no query may claim MV substitution until the rewrite surface produces a validated replacement tree |
| general decorrelation | fail-closed | not admitted | subquery decorrelation is not a generic transform family in the current contract |
| unrestricted join reordering rewrite | fail-closed | not admitted as a rewrite pass | join ordering belongs to planner search, not an unconstrained rewrite layer |
| arbitrary view rule expansion | fail-closed | not admitted | no donor-style rule system or view rewrite engine is implied |

## Transform execution contract

Every admitted transform must declare:

1. transform class name
2. owning pass id
3. input semantic contract id
4. output semantic contract id
5. whether the transform is structure-preserving, access-lowering, or payload-annotation only
6. whether downstream recheck is required for correctness

## Access-family lowering rules

Access-family lowering must produce a lowering result that includes at least:

1. planner family identity
2. family name
3. queryability state
4. requires-recheck flag when exact execution cannot be guaranteed from the access family alone
5. invalid or rejection reason if the requested family cannot legally serve the predicate shape

## Transform refusal rules

A transform must refuse execution when:

1. the semantic contract is not frozen for its entry point
2. the transform would change bound object identity
3. the transform depends on unavailable metadata or unsupported statistics features
4. the transform cannot state whether executor recheck is required
5. the transform is present only as a placeholder or pending migration surface

## Required ordering

1. normalization before predicate matching
2. predicate matching before access-family lowering
3. access-family lowering before planner path enumeration
4. runtime payload annotation only after final plan selection

## Explicit non-guarantees

- no general algebraic optimizer catalog
- no universal decorrelation framework
- no automatic materialized-view substitution guarantee
- no hidden transform family outside the catalog in this file
