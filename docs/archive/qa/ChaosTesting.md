### Fault Matrix
- Disk I/O failure: simulated via `SCRATCHBIRD_FAULT_INJECT` macros, see `tests/chaos/`.
- WAL corruption: to be added under `tests/chaos/`.
- Process crash: targeted tests that exit abruptly and validate recovery on next start.
- Memory pressure: allocate pressure in tests and validate behavior.

### Procedures
- Build with `-DSCRATCHBIRD_FAULT_INJECT=ON`.
- Run chaos tests offline (embedded), no network server.

### Recovery Targets
- Restart < 30s; recovery < 5 min (simulated scale); query availability gap < 10s.


## Related
- [Gates](CI_Gates.md)
- [Hardware and Environment](PerformanceMethodology.md)
- [Progress](Progress.md)
- [Static Analysis](SecurityHardening.md)
- [Scope](TestPlan.md)
