# Code Truth Audit Maintenance Rules

1. Every implementation-status claim in this package must point to a stable
   `implementation_path + unique_search_key`.
2. Use current canonical ScratchBird specs and local code as the first truth
   source.
3. Research may broaden the model, but it may not overwrite current proof
   without explicit canonical ownership.
4. Distributed query, sharding, and OLAP designs must preserve MGA truth and
   keep derivative exchange or commit-log lanes subordinate to local committed
   state.
5. Benchmark or performance claims must not be promoted without a matching gate
   target in section `31`.
6. If a topic turns out to decompose into multiple canonical files, record the
   split in `RISK_DECISION_LOG.md` and update this package's trackers.
