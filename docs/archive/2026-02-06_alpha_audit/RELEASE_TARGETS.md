# Release Targets (P0–P3)

This document defines the release ladder for ScratchBird and associated repositories.

## P0 — Alpha (Current)

Goal: Working end-to-end system for developers; correctness over polish.

Features:
1. SBWP v1.1 baseline across core drivers and CLI.
2. TLS enforced; binary-only parameters.
3. Full type matrix encode/decode implemented.
4. Metadata helpers wired to sys.* contract.
5. Conformance harness (type + protocol baseline).
6. Core server ops viable for testing (create/query/transactions).
7. Developer docs/specs prioritized over general docs.

## P1 — Pre-Beta (Initial Binaries + Freeze)

Goal: First official binaries, stable API surface, bug triage.

Features:
1. Binary release for engine + a test app image.
2. Branch freeze for core protocol + driver APIs.
3. Baseline integration validation for key tools (JDBC/ODBC).
4. Stability focus: crash, protocol, and data integrity fixes only.
5. Installation guides validated on Ubuntu 24.04.
6. Compatibility matrices finalized for P0 drivers.

## P2 — Beta

Goal: Feature completeness and ecosystem readiness.

Features:
1. Broader driver/integration coverage (P1/P2 target list).
2. Full metadata contract verification tests.
3. Conformance suite expanded (paging/cancel/timeout).
4. Packaging for core drivers (pip/npm/nuget/etc.).
5. Performance baselines published.
6. Documentation and wiki fully aligned to implementation.

## P3 — Gold / Post-Gold

Goal: Long-term stability, support, and production readiness.

Features:
1. Release candidates + signed binaries.
2. Backward compatibility guarantees and migration guide.
3. Security audit and upgrade policy.
4. Tooling integration polish (BI/ORM).
5. Telemetry/monitoring hooks documented.
6. Support SLAs / issue response workflow (if desired).
