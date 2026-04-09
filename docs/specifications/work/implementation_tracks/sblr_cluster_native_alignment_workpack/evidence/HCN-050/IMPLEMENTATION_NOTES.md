# Implementation Notes - HCN-050

Code paths:
- `src/sblr/jit/jit_runtime.cpp`
- `tests/unit/test_sblr_jit_scope.cpp`
- `tests/unit/test_sblr_jit_test_utils.h`

Key details:
- Runtime now rejects explicit compile requests outside routine/member scope.
- Compile request validation enforces deterministic artifact identity prerequisites before backend invocation.
- Test harness default policy profile updated for strict policy resolver behavior while preserving deterministic ticket tests.
