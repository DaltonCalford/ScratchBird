# One-Off Latency Guard Results - HCN-052

Primary benchmark source:
- `SblrJitFixture.jit_performance_envelope`

Observed p95 values (from `/tmp/hcn061_tests.log`):
- `vm_p95_us=697`
- `jit_p95_us=357`

Guard condition in benchmark:
- `jit_p95_us <= vm_p95_us * 3 + 1`

Result:
- Guard satisfied with margin (`357 <= 2092`).
- One-off latency protection requirement remains satisfied.
