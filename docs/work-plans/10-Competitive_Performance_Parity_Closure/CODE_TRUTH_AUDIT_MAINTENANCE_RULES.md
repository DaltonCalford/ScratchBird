# Code Truth Audit Maintenance Rules

1. Every implementation reference in this package must use `path/file +
   unique_search_key`, never line numbers.
2. When a ticket lands, update:
   - `MASTER_TRACKER.csv`
   - `MASTER_TRACKER.md`
   - `PROCESS_PARITY_TARGETS.csv`
   - `LOAD_TABLE_TARGETS.csv`
   - `DONOR_FAST_PATH_ASSIMILATION_TRACKER.csv`
   - `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`
3. If a benchmark result is replaced, preserve the old artifact root and append
   the new root instead of mutating history.
4. If a donor-fast-path row is waived, the waiver must cite the blocking
   invariant and the package evidence folder.
5. If a process target is met, the evidence entry must point to the exact
   rerun artifact root and comparison summary.
6. If a benchmark runner or artifact format changes, `PP-10-002` must be
   reopened unless equivalent provenance is preserved.
