# Section 12 Test Contract

Section `12` is implementation-ready only if maintained evidence covers the
current temp-table and planner-spill behaviors it claims.

## Direct audited proof artifacts

- `tests/unit/test_temp_table_semantics.cpp`
  - `TempTableExecutorTest.TempTableOnCommitPreserveRows`
  - `TempTableExecutorTest.TempTableOnCommitDeleteRows`
  - `TempTableExecutorTest.TempTableSessionCleanupDropsMetadata`
  - `TempTableExecutorTest.StartupReopenDropsStaleSessionTempMetadata`
- `tests/unit/test_query_planner_integration.cpp`
  - `QueryPlannerIntegrationTest.HashJoinRuntimePlanTracksMemoryBudgetAndSpillMetadata`
  - `QueryPlannerIntegrationTest.SpillPolicyDisallowRejectsSpilledHashJoin`
  - `QueryPlannerIntegrationTest.ExplainJsonIncludesOperatorMemoryAndSpillMetadata`

## Required certification lanes

### Temporary tables

- temp-table catalog identity and scope enums map to the documented behavior
- parser rejects `ON COMMIT` on non-temp tables
- `ON COMMIT DELETE ROWS` behavior is deterministic
- `ON COMMIT PRESERVE ROWS` behavior is deterministic
- `ON COMMIT DROP` follows the documented current authority boundary
- session-end temp cleanup is deterministic
- startup temp purge is deterministic
- temp pages are marked non-durable and excluded from durable restart handling

### Rollback behavior

- temp DDL and temp DML follow the same transaction and savepoint model as the
  rest of the engine
- no test may assume a temp-specific rollback mechanism outside the documented
  shared MGA transaction lifecycle and the explicit `ON COMMIT` actions

### Planner spill

- spill-policy parsing is deterministic
- operators that would spill are refused when spill is disallowed
- spill metadata in plan or explain output matches the documented fields

## Excluded lanes

The following are not current section `12` certification requirements:
- runtime spill-file identity
- runtime spill-file quotas
- runtime spill cleanup or restart handling
- generalized operator workfile subsystem diagnostics
