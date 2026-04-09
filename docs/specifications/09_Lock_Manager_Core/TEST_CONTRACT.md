# Test Contract

Status: current_authority

Last code-audit date: 2026-03-30

The audited code proves a stronger lock-manager test surface around deadlock detection, read-consistency restart behavior, no-wait update conflict behavior, failpoint-based deadlock handling, and metadata lock visibility.

## Current implementation-backed test surface

| Test or evidence file | Current proven coverage | Anchor |
| --- | --- | --- |
| tests/unit/test_deadlock_detection.cpp | deadlock detection, stats, victim and outcome behavior across lock-manager runtime | [test_deadlock_detection.cpp](tests/unit/test_deadlock_detection.cpp) |
| tests/unit/test_storage_engine.cpp | READ_COMMITTED_READ_CONSISTENCY restart-required outcome and READ_COMMITTED no-wait conflict outcome | [test_storage_engine.cpp:1220](tests/unit/test_storage_engine.cpp:1220) |
| tests/unit/test_mga_failpoint_replay.cpp | deadlock detector stall and victim-selection failpoint coverage | [test_mga_failpoint_replay.cpp:850](tests/unit/test_mga_failpoint_replay.cpp:850) |
| tests/unit/test_virtual_catalogs.cpp | metadata lock exposure, wait-history visibility, timeout reporting | [test_virtual_catalogs.cpp:430](tests/unit/test_virtual_catalogs.cpp:430) |

## Canonical section 09 gate matrix

| Gate area | Required proof | Current scope rule |
| --- | --- | --- |
| deadlock detection | deadlock tests and detector stats | section 09 cannot claim detector correctness without dedicated deadlock test evidence |
| no-wait conflict | storage-engine update conflict tests | section 09 cannot claim NOWAIT behavior without proving immediate fail-closed refusal |
| read-consistency restart | restart-required update conflict tests | section 09 and section 08 must share one vocabulary for restart-required audited paths |
| metadata lock visibility | virtual catalog visibility tests | section 09 cannot claim reporting behavior without proving metadata-lock exposure and wait-history visibility |
| failpoint deadlock handling | failpoint replay tests | section 09 negative-path handling is not complete without detector stall and victim-selection proof |

## Proof obligations

- Prove deadlock detection from dedicated lock-manager tests.
- Prove restart and no-wait divergence from storage-engine tests.
- Prove metadata lock visibility from virtual-catalog tests.
- Add new tests before claiming new isolation or lock-resource capability.
- Keep fairness and starvation claims fail-closed until dedicated tests exist.

## Non-guarantees

- No statement is made here that all future lock-resource families are already test-covered.
- No predicate or range or serializable certification is made here.
- No fairness or starvation certification is made here.
