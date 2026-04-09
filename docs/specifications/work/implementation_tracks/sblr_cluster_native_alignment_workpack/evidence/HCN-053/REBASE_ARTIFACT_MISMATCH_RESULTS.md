# Rebase Artifact Mismatch Results - HCN-053

Coverage:
- `SblrJitFixture.jit_rebase_cross_target_rebase_keeps_canonical_sblr_executable`

Outcome:
- Artifact compiled for original target is disqualified after target rebase.
- Execution deterministically falls back to VM path.
- Canonical SQL/SBLR result remains correct after rebase mismatch.
