### Building
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Coverage
```bash
cmake -S . -B build-cov -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSCRATCHBIRD_COVERAGE=ON
cmake --build build-cov -j
ctest --test-dir build-cov --output-on-failure
cmake --build build-cov --target coverage # HTML at build-cov/coverage
```

### Sanitizers
```bash
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSCRATCHBIRD_ASAN=ON -DSCRATCHBIRD_UBSAN=ON
cmake --build build-asan -j && ctest --test-dir build-asan --output-on-failure
```

### Perf suite
```bash
cmake -S . -B build-perf -G Ninja -DCMAKE_BUILD_TYPE=Release -DSCRATCHBIRD_PERF=ON
cmake --build build-perf -j
python3 tests/perf/run_perf.py --build build-perf --out perf_current.json
python3 tests/perf/run_perf.py --build build-perf --out perf_current.json --baseline resource/perf_baselines/perf_baselines.json
```

### Fuzzing (short run)
```bash
cmake -S . -B build-fuzz -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSCRATCHBIRD_FUZZ=ON
cmake --build build-fuzz -j
ctest --test-dir build-fuzz -R fuzz_sql_parser_smoke --output-on-failure
```

### Chaos tests
```bash
cmake -S . -B build-chaos -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSCRATCHBIRD_FAULT_INJECT=ON
cmake --build build-chaos -j
```

