# Implementation Notes - HCN-051

Code paths:
- `src/sblr/jit/jit_artifact_store.cpp`
- `include/scratchbird/sblr/jit/jit_reason_codes.h`
- `src/sblr/jit/jit_reason_codes.cpp`
- `tests/unit/test_sblr_jit_artifact_key.cpp`
- `tests/unit/test_sblr_jit_fallback.cpp`
- `tests/unit/test_sblr_jit_policy.cpp`

Key details:
- Candidate artifacts are evaluated in deterministic newest-first order.
- First rejection reason is retained when no valid candidate is found.
- Non-ready artifact states are rejected with explicit reason code (`ARTIFACT_STATE_NOT_READY`).
