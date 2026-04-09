# Section 36 Test Contract

Required evidence for future hardening:
- rewrite-stage coverage evidence
- bounded plan construction or stability evidence
- proof for any claimed cost or statistics dependency
- explicit negative evidence for unsupported planner behaviors where appropriate

Required evidence for Beta 2 optimizer claims:
- planner-front-door unification proof, including canonical API ownership and property-preserving candidate retention
- merge, ordering, exchange, and parallel path search evidence showing property-distinct alternatives are preserved and compared
- cross-family optimizer parity evidence proving shipped families are evaluated as primary classes under family-native or documented fallback metrics
- mixed workload crossover evidence covering ordered, summary, columnstore, text, spatial, vector, and exact fallback competition
- persistent plan-store, baseline, and regression evidence with stable plan identity, regression classification, and baseline or forcing state
- CE governance evidence with version identity, confidence class, fallback reason, and sampled or multivariate usage disclosure
- adaptive processing evidence for adaptive join thresholds, memory grant prediction versus actuals, interleaved-execution branch outcomes, and safe fallback behavior
- parameter-sensitive plan evidence proving regime bucketing, bounded multi-plan cache growth, and deterministic regime selection
- workload-governance and DOP-feedback evidence proving optimizer decisions honor admission and resource-envelope outcomes rather than bypassing them
- distributed-query decomposition evidence proving fragment graph publication,
  motion-class assignment, and deterministic result stitching
- OLTP fast-path evidence proving prepared point-shape admission, cache reuse,
  and contention-aware refusal behavior
- OLAP rewrite evidence proving rollup, projection, and cube matching respect
  coverage, freshness, and HTAP service-envelope constraints
- native federation closure evidence proving remote binding, pushdown-class
  disclosure, statistics freshness, and residual local execution behavior
- plan-store closure evidence proving operator review, forced-plan refusal, and
  managed tuning action traceability
