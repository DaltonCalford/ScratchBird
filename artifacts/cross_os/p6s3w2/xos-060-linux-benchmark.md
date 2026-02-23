# XOS-060 Linux Benchmark vs Baseline
Last-Modified: 2026-02-22

## Method
- Benchmark harness:
  - `scripts/cross_os/benchmark_portability.sh`
- Workload:
  - `ServiceControllerListenerBootstrapTest.NormalizeConfigPathsAndValidateUtf8NormalizesKeyRuntimePaths`
  - `MySQLParserTest.CreateTableBasic`
  - `PostgreSQLParserTest.SimpleSelect`
  - `FirebirdParserTest.SelectSimple`

## Evidence
- Command log:
  - `artifacts/cross_os/p6s3w2/xos-060-benchmark-command-log.txt`
  - `artifacts/cross_os/p6s3w2/xos-060-benchmark-rerun-command-log.txt`
- Baseline JSON:
  - `artifacts/cross_os/p6s3w2/xos-060-linux-benchmark-baseline.json`
- Candidate JSON:
  - `artifacts/cross_os/p6s3w2/xos-060-linux-benchmark.json`

## Current Result (9-run rerun)
- Baseline median: `258 ms`
- Candidate median: `260 ms`
- Regression: `0.78%`
- Threshold status (`<= 5%`): **MET**
