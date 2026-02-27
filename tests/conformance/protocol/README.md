# Protocol Frame Conformance

This directory contains deterministic frame-shape conformance assets for:
1. `sbwp_native`
2. `postgresql_v3` emulation surface
3. `mysql_8x` emulation surface
4. `firebird_remote` emulation surface

## Scope
1. Header/payload framing invariants.
2. Request/response shape invariants.
3. Negative frame handling contracts.
4. Golden trace checksum index for reproducible audits.

## Layout
1. `fixtures/`: deterministic trace fixtures grouped by protocol.
2. `golden/sbwp/`: native SBWP golden trace metadata used by frame conformance tests.
3. `golden/pg/`: PostgreSQL emulation golden trace metadata used by frame conformance tests.
4. `golden/mysql/`: MySQL emulation golden trace metadata used by frame conformance tests.
5. `golden/firebird/`: Firebird emulation golden trace metadata used by frame conformance tests.
6. `scripts/generate_golden_trace_index.sh`: emits checksum index for all fixtures.
7. `test_sbwp_frame_conformance.cpp`: native SBWP frame-level assertions.
8. `test_pg_frame_conformance.cpp`: PostgreSQL frame-level assertions.
9. `test_mysql_frame_conformance.cpp`: MySQL frame-level assertions.
10. `test_firebird_frame_conformance.cpp`: Firebird frame-level assertions.
11. `test_protocol_frame_conformance.cpp`: cross-lane negative protocol matrix assertions.

## Current Status
1. `A55-010`: fixture inventory exists for SBWP, PostgreSQL, MySQL, and Firebird lanes.
2. `A55-011`: native SBWP frame conformance is executable via `SBWPFrameConformance`.
3. `A55-012`: PostgreSQL frame conformance is executable via `PGFrameConformance`.
4. `A55-013`: MySQL frame conformance is executable via `MySQLFrameConformance`.
5. `A55-014`: Firebird frame conformance is executable via `FirebirdFrameConformance`.
6. `A55-015`: negative protocol conformance matrix is executable via `ProtocolFrameConformance`.
