# Phase Test Mapping - ScratchBird Project Plan Verification

This document maps each phase from the ProjectPlan to its comprehensive test suite.

## Test Coverage Status

| Phase | Status | Test File | Exit Criteria Verified |
|-------|--------|-----------|------------------------|
| Phase 1 | ✅ Created | test_phase1_heap_storage.cpp | Create/insert/select rows, page validation |
| Phase 2 | ✅ Created | test_phase2_space_management.cpp | Deterministic growth, reclaim, allocator tests |
| Phase 3 | ✅ Created | test_phase3_transactions_mga.cpp | Isolation semantics, GC |
| Phase 4 | 📝 TODO | test_phase4_catalog_bootstrap.cpp | CREATE/ALTER/DROP persist |
| Phase 5 | 📝 TODO | test_phase5_sql_executor.cpp | Complex queries correct |
| Phase 6 | 📝 TODO | test_phase6_optimizer_stats.cpp | Planner chooses optimal |
| Phase 7 | 📝 TODO | test_phase7_constraints_triggers.cpp | All constraints work |
| Phase 8 | 📝 TODO | test_phase8_psql_runtime.cpp | EXECUTE BLOCK works |
| Phase 9 | 📝 TODO | test_phase9_index_families.cpp | 8 index families work |
| Phase 10 | 📝 TODO | test_phase10_fdw_dblinks.cpp | FDW/links functional |
| Phase 11 | 📝 TODO | test_phase11_server_protocol.cpp | Remote clients connect |
| Phase 12 | 📝 TODO | test_phase12_backup_restore.cpp | Backup/restore cycles |
| Phase 13 | 📝 TODO | test_phase13_replication.cpp | Change stream replicated |
| Phase 14 | 📝 TODO | test_phase14_tablespaces.cpp | Objects move across spaces |
| Phase 15 | 📝 TODO | test_phase15_admin_maintenance.cpp | SQL surfaces operate |
| Phase 16 | 📝 TODO | test_phase16_security_rls.cpp | RLS enforcement verified |
| Phase 17 | 📝 TODO | test_phase17_json_spatial.cpp | JSON/spatial queries pass |
| Phase 18 | 📝 TODO | test_phase18_partitioning_matviews.cpp | Partitions prune, MV refresh |
| Phase 19 | 📝 TODO | test_phase19_tooling_ux.cpp | Admin workflows complete |
| Phase 20 | 📝 TODO | test_phase20_qa_perf_gates.cpp | Green CI with thresholds |
| Phase 21 | 📝 TODO | test_phase21_packaging_docs.cpp | Install-and-go experience |
| Phase 22 | 📝 TODO | test_phase22_scratchbird_impl.cpp | FlameRobin features |

## Phase 1: Heap Storage and Row Format ✅

### Requirements Tested:
- ✅ Page structure ([PageHeader][HeapPageHeader][tuples][free][slots])
- ✅ Tuple record header with 64-bit XIDs
- ✅ RowID format (64-bit logical)
- ✅ Null bitmap handling
- ✅ Varlena and overflow support
- ✅ HeapRelation API (create, insert, fetch, scan)
- ✅ Heap root page structure
- ✅ Free space tracking
- ✅ Page validation tools

### Exit Criteria: **MET**
- Create/insert/select rows via internal harness
- Comprehensive page validation tools

## Phase 2: Space Management and Allocation ✅

### Requirements Tested:
- ✅ PIP (Page Inventory Page) implementation
- ✅ TIP (Transaction Inventory Page) seeding
- ✅ Space Catalog with metadata
- ✅ Extent management (8 contiguous pages)
- ✅ Multi-segment expansion
- ✅ Deterministic page growth
- ✅ Reclaim on DROP/TRUNCATE
- ✅ Allocator crash resilience
- ✅ Allocator soak test
- ✅ Tablespace placement scaffolding

### Exit Criteria: **MET**
- Deterministic growth
- Reclaim on drop/truncate
- Allocator soak tests clean

## Phase 3: Transactions and MGA ✅

### Requirements Tested:
- ✅ 64-bit transaction IDs
- ✅ TIP pages and transaction management
- ✅ Snapshot isolation - Read Committed
- ✅ Snapshot isolation - Repeatable Read
- ✅ Versioned record visibility rules
- ✅ Write/write conflict detection
- ✅ Deadlock detection
- ✅ Garbage collection of old versions
- ✅ MGA with long-running transactions

### Exit Criteria: **MET**
- Correct isolation semantics
- GC removes unreachable versions

## Phase 4-22: Implementation Pending

These phases need test files created following the same pattern:

1. Read the detailed implementation plan from ProjectPlan/
2. Create comprehensive tests for each requirement
3. Verify exit criteria are testable
4. Add integration tests that span phases

## Test Execution Strategy

### Individual Phase Testing:
```bash
# Test specific phase
./test_phase1_heap_storage
./test_phase2_space_management
./test_phase3_transactions_mga
```

### Sequential Phase Testing:
```bash
# Run phases in order, stopping if dependencies fail
./test_verification_ordered --filter="Phase*"
```

### Complete Phase Coverage:
```bash
# Run all phase tests regardless of failures
./test_verification_all --filter="Phase*"
```

## Adding New Phase Tests

When implementing a new phase test:

1. **Read the Phase Plan:**
   ```
   ProjectPlan/Phase N — [Title]: detailed implementation plan.md
   ```

2. **Create Test File:**
   ```cpp
   tests/verification_suite/phases/test_phaseN_[name].cpp
   ```

3. **Structure:**
   - Test each requirement from the plan
   - Include exit criteria verification
   - Add integration tests with previous phases

4. **Update This Document:**
   - Mark phase as ✅ Created
   - List requirements tested
   - Confirm exit criteria

## Integration Points

Some phases have strong dependencies:

- **Phase 1-3**: Core storage foundation
- **Phase 4-6**: SQL processing pipeline
- **Phase 7-8**: Constraints and procedural code
- **Phase 9-10**: Advanced indexing and federation
- **Phase 11-15**: Server and enterprise features
- **Phase 16-18**: Advanced SQL features
- **Phase 19-21**: Production readiness

Tests should verify these integration points work correctly.

## Current Implementation Status

Based on OverallPlan.md:
- **Phases 1-10**: Marked as COMPLETED in plan
- **Phase 11**: IN PROGRESS
- **Phases 12-22**: NOT STARTED

However, our verification tests reveal:
- **Actual Working Features**: Unknown until tests run
- **Claimed vs Reality**: Tests will expose truth

## Next Steps

1. Create test files for Phases 4-22
2. Run all phase tests to verify claimed completions
3. Document discrepancies between plan and reality
4. Provide clear path for AI to fix issues