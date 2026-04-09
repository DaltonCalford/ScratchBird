# Non-Guarantee and Partial Planner Behavior

## Explicit non-guarantees

- no global optimality guarantee
- no exhaustive donor-equivalent optimizer guarantee
- no unrestricted rule-system rewrite guarantee
- no active materialized-view substitution guarantee while the current MV rewrite surface remains non-producing
- no adaptive or feedback-driven replanning guarantee
- no universal coherent invalidation guarantee across every cache-like surface in the product
- no guarantee that all statements are eligible for reusable plan caching
- no guarantee that statistics are complete, fresh, or equally rich for every relation and operator family
- no guarantee that join search is exhaustive across every possible ordering

## Partial-status rule

Section 36 is now required to be implementation-grade, but it still draws a hard line between:

1. declared rewrite stages and transforms
2. declared planner decision procedures and cache rules
3. unsupported optimizer folklore that remains intentionally excluded

Any implementation ambiguity must be resolved by tightening one of the declared procedures above, not by importing donor behavior by analogy.
