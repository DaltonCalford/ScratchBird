# Phase J — Performance hardening

Scope: B-Tree performance benchmarking and tunables to achieve competitive throughput/latency without regressions.

Benchmarks
- Workloads: point lookup, range scan, insert, delete, maintenance (rebuild/validate).
- Distributions: uniform, skew (hot set), Zipf-like.
- Page sizes: 4K, 8K, 16K, 32K, 64K, 128K.
- CLI: `bench_btree <base> <page_size> <N> <uniform|zipf|skew> <point|range|insert|delete> [fillfactor] [prefetch_pages] [split_policy]`.

Tunables
- `fillfactor` (default 0.7)
- `split_policy` (even | left | right) controls left/right page occupancy after split.
- `prefetch_horizon_pages` best-effort OS prefetch for leaf reads in scans.
- `enable_key_prefix_compare` (on) and `enable_leaf_prefix_compression` (future).

Micro-optimizations
- Comparator uses `memcmp` + size check for branch-reduced short-key paths.
- Split selection respects tunables to avoid pathological fullness or churn.
- Prefetch hints on leaf scans to improve sequential throughput.

Exit criteria
- Record benchmark timings for each page size and distribution.
- Establish baseline target ranges for insert QPS and point latency (p50/p99) and guard via CI perf-smoke (to be added).
- No regressions under CI gates relative to the last green baseline.
