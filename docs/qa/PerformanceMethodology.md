### Hardware and Environment
- CI runner: ubuntu-latest. Local: document CPU, RAM, storage.

### Runs
- Warm single run + 2 measured runs, report p95 from 3 runs.
- Scale: microbenchmarks; TPC-H style queries (scale 1) placeholder.

### Metrics
- `query_execution_time_p95_ms`, `tps`, `peak_mem_mb`, `disk_io_ops`, `cpu_pct`.

### Baselines and Gates
- Baselines in `resource/perf_baselines/perf_baselines.json`.
- Gates: p95 +10% fail; TPS −5% warn/−10% fail; peak_mem +20% fail.

### Process
- Update baselines via dedicated PR with title “Perf: Refresh baselines [date/hash]”.


## Related
- [Gates](CI_Gates.md)
- [Fault Matrix](ChaosTesting.md)
- [Progress](Progress.md)
- [Static Analysis](SecurityHardening.md)
- [Scope](TestPlan.md)
