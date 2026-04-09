# Latency Percentile Report - HCN-061

Source logs:
- `/tmp/hcn061_tests.log`
- `/tmp/hcn_full_regression.log`

Observed benchmark values:
- Perf subset:
  - `vm_p95_us=697`
  - `jit_p95_us=357`
- Extended regression subset:
  - `vm_p95_us=748`
  - `jit_p95_us=400`

Gate decision:
- Latency guard remains satisfied in both runs (`jit_p95 <= vm_p95*3+1`).
- No latency-gate failure observed.
