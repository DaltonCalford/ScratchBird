# B1-01-004 Evidence Note

## Closure summary

Lane B implementation closure for this package is complete.

This closure pass:
- tightened transaction begin and rollback so an open write-admission fence
  refuses publication before a new XID is consumed or rollback is terminally
  published
- added direct transaction-manager proof for fenced begin and rollback refusal
- advanced the lane-B implementation rows in
  `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv` to `implemented`
- refreshed the canonical section test contracts for sections `03`, `08`, `09`,
  `10`, `11`, `35`, and `42`

## Code and proof anchors

- `src/core/transaction_manager.cpp`
- `tests/unit/test_transaction_manager.cpp`
- `tests/unit/test_executor_transaction_payload.cpp`
- `tests/unit/test_storage_engine.cpp`
- `tests/unit/test_garbage_collector.cpp`
- `tests/unit/test_heap_index_gc_integration.cpp`
- `tests/unit/test_deadlock_detection.cpp`
- `tests/unit/test_virtual_catalogs.cpp`
- `tests/unit/test_toast_gc_contract.cpp`
- `tests/unit/test_toast_tip_visibility.cpp`
- `tests/integration/test_toast_crash_recovery_mga.cpp`

## Verification

Focused rebuild:
- `cmake --build /home/dcalford/CliWork/ScratchBird/build --target scratchbird_tests -j4`

Focused test proof:
- `./build/tests/scratchbird_tests --gtest_filter='TransactionManagerTest.BeginTransactionRefusesOpenWritebackFenceWithoutConsumingXid:TransactionManagerTest.RollbackRefusesOpenWritebackFenceAndKeepsTransactionActive:ExecutorTransactionPayloadTest.CommitFenceRejectsWhileWritebackIncidentIsOpen:ExecutorTransactionPayloadTest.ReopenReloadsWritebackFenceUntilIncidentClears:MgaFailpointReplayTest.CommitPreTipFailpointAbortsInsertedRowAcrossUncleanRestart'`

Result:
- all five targeted tests passed on March 30, 2026

## Residual non-blockers

- section `04` page-size evidence and section `06` bootstrap corruption-matrix
  closure remain B1-01-005 gate work rather than lane-B implementation blockers
- this ticket did not run the broader gate or benchmark suite
