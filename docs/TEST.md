# ScratchBird Test Guide

## Core Regression

Run all registered tests:

```bash
ctest --test-dir build --output-on-failure
```

## Required Public Beta Gate

Hard gate script:

```bash
bash tests/conformance/public_beta/run_required_public_beta_gate.sh
```

Equivalent CTest target:

```bash
ctest --test-dir build -R '^ConformancePublicBetaRequiredGate$' --output-on-failure
```

## v3 Native Inet Conformance

```bash
bash tests/conformance/v3_native_inet/run_v3_native_inet_ctest.sh
```

## Compatibility/Emulation Suites

Run per-surface compatibility scripts:

```bash
bash tests/compatibility/scratchbird/scripts/run_scratchbird_native_ctest.sh
bash tests/compatibility/postgresql/scripts/run_postgresql_ctest.sh
bash tests/compatibility/mysql/scripts/run_mysql_ctest.sh
bash tests/compatibility/firebird/scripts/run_firebird_ctest.sh
```

## Test Policy for Public Beta

A beta release candidate must have a passing required gate run covering:

- wire protocol correctness
- transaction semantics
- security enforcement
- end-to-end SQL correctness
- modal/NoSQL verification
- cluster infrastructure verification
