# ScratchBird Test Guide

> Section 35 boundary note: the maintained test surfaces here provide bounded
> durability, recovery, and reliability evidence. They are not by themselves a
> universal certification of WAL-style recovery or seamless failover behavior.
>
> Section 36 boundary note: the maintained test surfaces here also bound
> rewrite and planner claims. They are not by themselves proof of a mature
> multi-phase optimizer, adaptive replanning, or stable cross-version plan
> identity.
>
> Section 37 boundary note: the maintained test surfaces here bound current
> statistics, metadata, and schema behavior only. They do not by themselves
> certify optimizer-grade statistics maturity, global metadata coherence, or
> mature concurrent DDL behavior.

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
