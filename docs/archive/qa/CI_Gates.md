### Gates
- Build + unit/integration/system tests must pass.
- Coverage ≥ 80% (fails CI if below).
- Sanitizers (ASan/UBSan) must be clean.
- Perf gates vs baseline (p95 +10% fail, TPS −10% fail, peak_mem +20% fail).
- Security: SBOM scan must have no high/critical.

### How to update baselines
- Use a dedicated PR titled “Perf: Refresh baselines [date/hash]” with artifacts and hardware details.


## Related
- [Fault Matrix](ChaosTesting.md)
- [Hardware and Environment](PerformanceMethodology.md)
- [Progress](Progress.md)
- [Static Analysis](SecurityHardening.md)
- [Scope](TestPlan.md)
