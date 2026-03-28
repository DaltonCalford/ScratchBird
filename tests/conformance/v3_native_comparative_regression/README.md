# V3 Native Comparative Regression

This suite runs a frozen donor-derived comparative corpus against:

- original Firebird
- original MySQL
- original PostgreSQL
- ScratchBird native `v3`

The translated `v3` cases are static on disk. The runner only substitutes a
run-local namespace token for object isolation; it does not translate donor SQL
at execution time.

## Purpose

- extend native `v3` coverage with donor-regression-derived issue classes
- prove apples-to-apples donor/native behavior using one normalized result schema
- emit timing and correctness outputs that can be compared in full-run metrics

## Run Standalone

```bash
bash tests/conformance/v3_native_comparative_regression/run_v3_native_comparative_regression_ctest.sh
```

It is also registered in CTest as `ConformanceV3NativeComparativeRegression`.
