# Implementation Notes - HCN-053

Code paths:
- `src/sblr/jit/jit_artifact_store.cpp`
- `tests/unit/test_sblr_jit_rebase.cpp`
- `tests/unit/test_sblr_jit_target_mismatch.cpp`

Key details:
- Compatibility selector disqualifies non-matching target triples before native selection.
- Runtime preserves canonical VM executability under all target-mismatch scenarios.
