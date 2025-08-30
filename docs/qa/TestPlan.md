### Scope
This plan covers unit, integration, system, performance, security, fuzzing, and chaos testing for the embedded engine.

### Suites
- Unit: `tests/` existing + additions under `tests/unit/`.
- Integration: `tests/integration/` database lifecycle, schema, data loads.
- System: `tests/system/` multi-connection and concurrency smoke.
- Performance: `tests/perf/` microbenchmarks and TPC-H style queries.
- Security: static scans, sanitizers, and input validation tests.
- Fuzzing: `tests/fuzz/` libFuzzer targets for parser, expr, codecs.
- Chaos: `tests/chaos/` with `SCRATCHBIRD_FAULT_INJECT`.

### Determinism
- Fixed seeds, bounded data sizes, temp directories under `/tmp` with PID suffixes.

### Execution
- CTest orchestrates unit/integration/system.
- Perf suite via `tests/perf/run_perf.py`.
- Coverage via `-DSCRATCHBIRD_COVERAGE=ON` and `ninja coverage`.


## Related
- [Gates](CI_Gates.md)
- [Fault Matrix](ChaosTesting.md)
- [Hardware and Environment](PerformanceMethodology.md)
- [Related](Progress.md)
- [Static Analysis](SecurityHardening.md)
