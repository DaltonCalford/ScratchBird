# Sprint 3: ONLINE Migration Architecture - Summary

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Document Status**: ✅ COMPLETE
**Version**: 1.0
**Date**: October 21, 2025
**Sprint Goal**: Complete architecture design for ONLINE tablespace migration
**Total Effort**: 8-10 hours (design and documentation)

---

## Executive Summary

Sprint 3 has been **FULLY COMPLETED**. A comprehensive architecture design document for ONLINE tablespace migration has been created, providing detailed specifications for all components needed to implement concurrent migration with minimal downtime.

**Design Completed**:
- ✅ Migration state tracking (catalog schema, state machine, API)
- ✅ Dual-source visibility model (TID resolution, bloom filters, caching)
- ✅ Write routing strategy (INSERT/UPDATE/DELETE during migration)
- ✅ Incremental page copy algorithm (background thread, batching)
- ✅ Catch-up phase and convergence detection
- ✅ Atomic swap protocol (< 100ms downtime target)
- ✅ Cleanup and deferred deallocation
- ✅ Error handling and rollback strategies
- ✅ Performance targets and scalability analysis
- ✅ Risk assessment and mitigation
- ✅ Implementation roadmap for Sprints 4-6

---

## What Was Accomplished

### 1. Architecture Document Created ✅

**Document**: `docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/SPRINT3_ONLINE_MIGRATION_ARCHITECTURE.md`

**Contents** (~15,000 words, comprehensive):
- Part 1: Migration State Tracking
- Part 2: Dual-Source Visibility Model
- Part 3: Write Routing Strategy
- Part 4: Incremental Page Copy
- Part 5: Catch-Up Phase and Convergence
- Part 6: Atomic Swap (Final Cutover)
- Part 7: Cleanup Phase
- Part 8: Error Handling and Rollback
- Part 9: Performance Characteristics
- Part 10: Testing Strategy
- Part 11: Risk Assessment
- Part 12: Implementation Roadmap
- Part 13: Success Criteria

---

### 2. Key Architectural Decisions

#### Decision 1: MGA-Native Dual-Source Visibility ✅

**Problem**: How to handle reads during migration when tuples exist in two tablespaces?

**Solution**:
- Leverage existing MGA infrastructure (TIP, snapshots, version chains)
- No new concurrency layer needed
- TID Resolution Service with bloom filter for fast lookup
- Query-level cache for repeated TID resolutions

**Benefits**:
- Minimal changes to existing heap fetch code
- Uses proven MGA visibility rules
- < 5% overhead target achievable

#### Decision 2: Target-Tablespace Write Routing ✅

**Problem**: Where should new INSERTs go during migration?

**Solution**:
- New INSERTs → target tablespace (xmin >= migration_xid)
- UPDATEs → same tablespace as old tuple (avoids cross-tablespace version chains)
- DELETEs → mark tuple in current location

**Benefits**:
- Preserves TID stability (MGA principle)
- No complex cross-tablespace version chains
- Simpler implementation

#### Decision 3: Convergence-Based Catch-Up ✅

**Problem**: High write load may prevent migration from completing.

**Solution**:
- Monitor dirty page rate vs. copy rate
- Convergence condition: `dirty_rate < copy_rate * 0.5`
- Fail gracefully after 100 iterations if not converging
- Optional: Brief write pause (< 1 second) to force convergence

**Benefits**:
- Automatic adaptation to write load
- Clear failure mode for impossible migrations
- User can choose OFFLINE migration if needed

#### Decision 4: Atomic Catalog Swap ✅

**Problem**: How to atomically switch table to target tablespace?

**Solution**:
- Acquire exclusive lock (< 100ms target)
- Copy final dirty pages (should be < 100)
- Single transaction:
  - Update TableInfo.tablespace_id
  - Batch update all index TIDs (use Sprint 2 code)
  - Commit
- Release lock

**Benefits**:
- Zero data loss
- Minimal downtime
- Leverages existing index TID update code

---

### 3. Performance Targets Established

| Metric | Target | Acceptable |
|--------|--------|------------|
| Query overhead (non-migrating) | 0% | < 1% |
| Query overhead (migrating) | < 5% | < 10% |
| Swap downtime | < 50ms | < 100ms |
| Migration throughput | 1000+ pages/sec | 500+ pages/sec |
| Memory overhead | < 1% of table size | < 5% |
| Convergence iterations | < 10 | < 100 |

**Scalability**:
- Small tables (< 1000 pages): < 1 second
- Medium tables (1K-100K pages): 1 sec - 2 min
- Large tables (100K-10M pages): 2 min - 3 hours
- Very large tables (> 10M pages): > 3 hours

---

### 4. Risk Assessment Completed

**High Risks**:
1. Dual-source visibility bugs → Extensive testing, feature flag
2. Swap phase timeout → Optimize batch updates, limit dirty pages
3. Non-convergence → Detection and graceful failure

**Medium Risks**:
4. Memory overhead → Monitor and tune bloom filter
5. Performance regression → Benchmarking and optimization

**Low Risks**:
6. Dirty page tracking errors → Test coverage and verification

**Mitigation**: All risks have clear mitigation strategies and contingency plans.

---

### 5. Implementation Roadmap Created

**Sprint 4: Core Infrastructure** (30-37 hours)
- Task 5.4.1: State Management (8-10 hours)
- Task 5.4.2: Dual-Source Visibility (12-15 hours)
- Task 5.4.3: Write Routing (10-12 hours)

**Sprint 5: Copy and Swap** (26-33 hours)
- Task 5.4.4: Incremental Copy (8-10 hours)
- Task 5.4.5: Catch-Up (6-8 hours)
- Task 5.4.6: Atomic Swap (8-10 hours)
- Task 5.4.7: Cleanup (4-5 hours)

**Sprint 6: Polish** (12-16 hours)
- Task 5.4.8: Error Handling (6-8 hours)
- Task 5.4.9: Integration Testing (6-8 hours)

**Total Implementation Effort**: 68-86 hours (matches roadmap estimate)

---

## Design Highlights

### Catalog Schema Extensions

**New Table**: `pg_table_migrations`
- Tracks active migrations
- Progress monitoring
- Phase tracking
- Statistics

**Extended Structure**: `TableInfo`
```cpp
struct TableInfo {
    // New fields:
    bool migration_in_progress = false;
    ID migration_id;
    uint64_t migration_xid = 0;
    uint16_t migration_target_ts = 0;
    uint8_t migration_phase = 0;
};
```

### TID Resolution Service

**Components**:
- Bloom filter (1% false positive rate)
- Exact TID mapping (for false positives)
- Query-level cache

**Performance**:
- Bloom lookup: ~1-2 ns
- Cache lookup: ~10-20 ns
- Overall overhead: < 5%

### Write Routing

**INSERTs**: → target tablespace (xmin >= migration_xid)

**UPDATEs**: → same tablespace as old tuple
- Preserves TID stability (MGA)
- Avoids cross-tablespace version chains

**DELETEs**: → mark tuple in current location

### Migration Phases

```
INIT → COPYING → CATCH_UP → SWAP → CLEANUP → COMPLETE
```

**COPYING**: Background incremental copy (minutes to hours)

**CATCH_UP**: Re-copy dirty pages until convergence (seconds to minutes)

**SWAP**: Atomic catalog update (< 100ms)

**CLEANUP**: Deferred source page deallocation (background)

---

## Documentation Deliverables

### Architecture Document ✅
- **File**: `SPRINT3_ONLINE_MIGRATION_ARCHITECTURE.md`
- **Size**: ~15,000 words
- **Sections**: 13 parts covering all aspects
- **Status**: Complete and ready for implementation

### Summary Document ✅
- **File**: `SPRINT3_SUMMARY.md` (this document)
- **Purpose**: Executive summary of Sprint 3
- **Status**: Complete

### Updated Roadmap ✅
- **File**: `TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md`
- **Updates**: Sprint 3 marked complete, Sprint 4-6 ready
- **Status**: Updated

---

## Next Steps

### Immediate: Sprint 4 Implementation

**Sprint 4: Core Infrastructure** (30-37 hours)
1. Implement migration state tracking
2. Implement dual-source visibility layer
3. Implement write routing

**Deliverables**:
- Catalog schema changes
- TID Resolution Service
- Modified heap fetch code
- Write routing logic

**Acceptance Criteria**:
- Queries work during migration (correct results)
- Writes routed to correct tablespace
- Performance overhead < 10%

### Future: Sprints 5-6

**Sprint 5**: Incremental copy, catch-up, swap, cleanup

**Sprint 6**: Error handling, rollback, testing

---

## Comparison: Sprints 0-3

| Sprint | Goal | Effort (Actual) | Status |
|--------|------|-----------------|--------|
| **Sprint 0** | Fix critical MVCC→MGA bug | 2.5 hours | ✅ COMPLETE |
| **Sprint 1** | Foundation (Autoextend) | Already done | ✅ COMPLETE |
| **Sprint 2** | Index Types + TOAST | 22-31 hours | ✅ COMPLETE |
| **Sprint 3** | ONLINE Migration Design | 8-10 hours | ✅ **COMPLETE** |
| **Sprint 4** | ONLINE Core Infrastructure | 30-37 hours | ⏸️ Not started |
| **Sprint 5** | ONLINE Copy and Swap | 26-33 hours | ⏸️ Not started |
| **Sprint 6** | ONLINE Polish | 12-16 hours | ⏸️ Not started |

---

## Success Criteria

**Sprint 3 is COMPLETE when**:
- [x] Migration state tracking fully designed
- [x] Dual-source visibility model specified
- [x] Write routing strategy defined
- [x] Incremental copy algorithm designed
- [x] Catch-up and convergence logic specified
- [x] Atomic swap protocol defined
- [x] Error handling and rollback strategies documented
- [x] Performance targets established
- [x] Risk assessment completed
- [x] Implementation roadmap created

**ALL CRITERIA MET** ✅

---

## Conclusion

**Sprint 3 Status**: ✅ **100% COMPLETE**

**Key Achievement**: Comprehensive architecture design for ONLINE tablespace migration, fully aligned with ScratchBird's MGA principles and providing a clear roadmap for implementation across Sprints 4-6.

**Design Quality**:
- Leverages existing MGA infrastructure
- Minimal invasive changes
- Clear performance targets
- Comprehensive risk mitigation
- Detailed implementation guidance

**Readiness**: The architecture is **production-ready** and can be immediately implemented in Sprints 4-6.

**Estimated Implementation Time**: 68-86 hours total (30-37 + 26-33 + 12-16)

**Next Sprint**: Sprint 4 (Core Infrastructure) - State Management, Dual-Source Visibility, Write Routing

---

**Document Version**: 1.0
**Last Updated**: October 21, 2025
**Status**: ✅ COMPLETE
**Next Action**: Begin Sprint 4 implementation or review architecture design
