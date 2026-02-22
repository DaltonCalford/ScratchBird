# AUTH-REL-001 Full Build/Test Report (2026-02-21)

## Clean build
- Command:
  - `rm -rf /home/dcalford/CliWork/ScratchBird/build`
  - `cmake -S /home/dcalford/CliWork/ScratchBird -B /home/dcalford/CliWork/ScratchBird/build -DCMAKE_BUILD_TYPE=Debug`
  - `cmake --build /home/dcalford/CliWork/ScratchBird/build -j8`
- Result: success.

## Full test suite execution
Deterministic full-suite execution used two steps due throughput-sensitive copy benchmark under heavy parallel contention.

1. Parallel full pass excluding copy benchmark
- Command: `ctest --output-on-failure -j14 -E '^COPY_1GB_Test$' --output-junit /home/dcalford/CliWork/local_work/artifacts/auth/p4s3w1/full-ctest-results-no-copy.xml`
- Result: success, exit code 0.
- Notes: `FrontDoorModeBenchmarkTest.DirectVsManagerProxyConnectAuthQueryLatency` skipped by test logic.

2. Isolated copy benchmark
- Command: `ctest -R '^COPY_1GB_Test$' --output-on-failure --output-junit /home/dcalford/CliWork/local_work/artifacts/auth/p4s3w1/copy-1gb-results.xml`
- Result: success (`1/1 passed`, ~190s).

## Flake analysis
- A prior all-in-one parallel run showed transient failures in:
  - `ConfigTest.ClearMethod`
  - `ServiceControllerListenerBootstrapTest.DirectModeLaunchesNativePgMysqlFirebirdListenerMatrix`
  - `Copy1GBTest.Copy_Throughput_Sustained`
- Corrective actions in this cycle:
  - Isolated working directory per `ConfigTest` process (`tests/unit/test_config.cpp`).
  - Wait-for-content synchronization in listener bootstrap matrix test (`tests/unit/test_service_controller_listener_bootstrap.cpp`).
  - Throughput benchmark executed in isolated step as part of full-suite coverage.

## Artifacts
- `artifacts/auth/p4s3w1/full-ctest-results-no-copy.xml`
- `artifacts/auth/p4s3w1/copy-1gb-results.xml`
- `artifacts/auth/p4s3w1/full-ctest-results.xml` (earlier exploratory full runs)
