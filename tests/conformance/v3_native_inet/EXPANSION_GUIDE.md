# v3 Inet Expansion Guide

This suite now executes `01..18` pass cases and explicit deprecated-alias fail cases over the inet listener path.

## Current expansion strategy

1. Keep parser-surface coverage broad in `sql/*.sql` so new parser regressions are caught quickly.
2. Track runtime/semantic gaps in `config/v3_statement_gap_report.csv`.
3. Translate upstream NoSQL regression ideas into canonical v3 scripts tracked in `config/nosql_regression_translation_candidates.csv`.
4. Promote each translated case into:
   - `sql/<case>.sql`
   - `expected/<case>.expected`
   - list entry in `config/v3_native_inet_list.txt`
5. Only convert a gap from `gap/partial` to `covered` when deterministic assertion behavior is present and reproducible.

## Immediate next high-value additions

1. Add a dedicated command-mode assertion runner for deterministic row-count/value validation after setup scripts.
2. Re-enable strict assertions for `01`, `03`, `04`, `06`, `07`, and security metadata cases once the command-mode verifier is in place.
3. Implement canonical runtime closures for `SEARCH DSL` and `REDIS` surfaces, then add pass-case scripts replacing current gap entries.
4. Reintroduce `MERGE` conformance once storage corruption defect is resolved.
