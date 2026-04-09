# Implementation Notes - HCN-052

Code paths:
- `src/sblr/jit/jit_queue.cpp`
- `src/sblr/jit/jit_runtime.cpp`
- `tests/unit/test_sblr_jit_tiering.cpp`
- `tests/unit/test_sblr_jit_policy.cpp`
- `tests/benchmark/test_sblr_jit_performance.cpp`

Key details:
- Request path never blocks on compilation; compilation is queued asynchronously.
- Queue capacity is enforced with deterministic saturation reason code.
- Hint deny actions are mandatory and short-circuit queue/native paths.
