# Test Contract

Section `23` is implementation-ready only if maintained evidence covers:
- planner front door and select-planning result formation
- compiler finalize and bytecode validation behavior
- join-ordering, access-path, selectivity, statistics, and runtime-plan payload formation
- plan-cache key formation, reuse, stats, and invalidation
- current memory-budget, spill-planning, and diagnostics reporting surfaces
- catalog-adjacent artifact hash and identifier anchors where this section claims them
- explicit fail-closed behavior for unsupported distributed, compute-capsule, native-compilation, and bulk-load narratives

This section does not require proof of unsupported future-only checklist files.

Section `23` is additionally Beta 2 implementation-ready only if maintained
evidence covers:
- runtime descriptor binding for the Beta 2 datatype families
- serializer round-trip for `BFLOAT16`, `BIGNUM`, `VERSIONSTAMP`, and
  `MULTIRANGE`
- executor comparison, arithmetic, selector, and setter behavior for the new
  Beta 2 families
- fail-closed unsupported-operation behavior on opaque donor payload wrappers
- plan and explain identity for `LATERAL_APPLY`, `JSON_TABLE_SCAN`,
  `ROWS_FROM_FANOUT`, `PIVOT_TRANSFORM`, `UNPIVOT_TRANSFORM`,
  `ROW_PATTERN_MATCH`, `TEMPORAL_BIND`, `ARRAY_JOIN_EXPAND`,
  `PREWHERE_FILTER`, `QUALIFY_FILTER`, `LIMIT_BY`, and
  `MULTI_MODEL_COMMAND_DISPATCH`
- generic window-function execution that preserves the admitted donor window
  families without collapsing them to `ROW_NUMBER`
- ordered-set aggregate execution that preserves direct arguments and
  `WITHIN GROUP` ordering
- deterministic plan-key participation for temporal bindings, session snapshot
  scope, insert-surface flavor, and multi-model verb identity
- SQL/XML function execution, `XML_TABLE_SCAN`, and richer `ROWS_FROM_FANOUT_V2`
  bindings preserve declared row shape and donor-visible explain identity
- SQL/JSON function execution preserves `RETURNING`, `ON EMPTY`, `ON ERROR`,
  wrapper, quote, and unique-key semantics
- aggregate-local separator and local-limit semantics execute inside the
  aggregate path rather than as outer-query rewrites
- insert-source `VALUES(col)` binds to candidate-row slots only and fails
  closed outside duplicate-key conflict-update scope
- parametric-function binding and lambda execution preserve parameter identity,
  body identity, and deterministic plan-key participation
- registry-bound engine error emission preserves `error_ref_uuid`, detail-slot
  payload, cause-chain identity, and operator trace metadata without composing
  client-facing prose inside the engine
- uncataloged internal failures collapse to the reserved internal UUID and do
  not surface raw legacy engine text to parser-visible transport
