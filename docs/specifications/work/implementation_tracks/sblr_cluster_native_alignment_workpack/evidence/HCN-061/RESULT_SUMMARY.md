# Result Summary - HCN-061

Status: complete.

Executed:
- Perf/latency subset: 12/12 passed.
- Extended regression subset: 74/74 passed.

Latency highlights:
- `vm_p95_us=697`
- `jit_p95_us=357`
- Benchmark guard (`jit_p95 <= vm_p95*3+1`) satisfied.

Cluster-path gate outcome:
- Replication ordering/watermark and telemetry SLO/shape tests all passed.
