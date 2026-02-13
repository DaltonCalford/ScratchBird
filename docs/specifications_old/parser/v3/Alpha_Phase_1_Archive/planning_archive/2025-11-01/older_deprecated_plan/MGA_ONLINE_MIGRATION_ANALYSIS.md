# Firebird MGA and ONLINE Migration: Architectural Analysis

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Document Status**: ARCHITECTURAL ANALYSIS
**Version**: 1.0
**Date**: October 21, 2025
**Purpose**: Explain how Firebird's MGA fundamentally changes ONLINE migration design

---

## Executive Summary

The previous analysis document (`PHASE5_TASK5_4_ONLINE_MIGRATION_ANALYSIS.md`) recommended deferring ONLINE migration based on PostgreSQL MVCC assumptions. This was **incorrect** because ScratchBird uses **Firebird's Multi-Generational Architecture (MGA)**, which fundamentally changes the design and implementation.

**Key Insight**: In Firebird MGA, the primary record location is STABLE. This makes ONLINE migration EASIER and MORE EFFICIENT than in PostgreSQL.

---

## The Fundamental Difference

### PostgreSQL MVCC (NOT our model)

**Update Process**:
1. Create NEW tuple at NEW location
2. Mark OLD tuple for cleanup
3. Update ALL indexes to point to new location

**Problem for ONLINE Migration**:
- Every UPDATE creates new physical tuple
- All indexes must be updated (write amplification)
- Migration must track BOTH old and new tuple locations
- Complex index maintenance during migration

**HOT (Heap-Only Tuple) Optimization**:
- Avoids index updates IF:
  - New tuple fits on same page
  - No indexed columns changed
- Still requires complex logic

---

### Firebird MGA (ScratchBird's model)

**Update Process** (from `MGA_IMPLEMENTATION.md` lines 970-1006):

```c
// THE FIREBIRD WAY:
1. Create BACK VERSION (old data) - store elsewhere
2. Modify record IN-PLACE (at original location)
3. Update header to point to back version

// PRIMARY RECORD LOCATION IS STABLE!
```

**Advantages for ONLINE Migration**:
- Primary record location NEVER changes
- Index TIDs point to stable location
- No index updates needed (unless indexed columns change)
- Version chains traversed via back pointers

**Critical Insight**:
> "By moving the *old* version instead of the *new* one, you keep the
> primary record location stable, eliminating the need for index updates
> on non-indexed columns."

---

## How MGA Simplifies ONLINE Migration

### 1. Index Stability (Biggest Advantage)

**Without MGA** (PostgreSQL):
```
Initial state:
  Index TID → Heap Tuple (page 100, slot 5)

After UPDATE:
  Index TID → ??? (old tuple at page 100, new tuple at page 200)
  PROBLEM: Must update index TID to point to page 200

During Migration:
  Index TID → ??? (source tablespace or target tablespace?)
  PROBLEM: Must track dual locations AND update indexes
```

**With MGA** (ScratchBird):
```
Initial state:
  Index TID → Primary Record (page 100, slot 5)

After UPDATE:
  Index TID → Primary Record (page 100, slot 5) [UNCHANGED!]
  Primary Record → Back Version (page 100, slot 10)

During Migration:
  Index TID → Primary Record (still points to SOURCE tablespace)
  After migration, update to TARGET tablespace (ONE-TIME operation)
  Version chains: TARGET (new primary) → SOURCE (back version)
```

**Key Point**: Index TIDs only need to be updated ONCE (during final swap), not continuously during writes.

---

### 2. Dual-Source Visibility

**The Challenge**: During migration, some records are in SOURCE, some in TARGET.

**With MGA, this is EASY** because:

1. **Transaction IDs tell us the truth**:
   ```c
   if (record.xmin < migration_start_xid) {
       // Record created before migration
       location = SOURCE_TABLESPACE;
   } else {
       // Record created during/after migration
       location = TARGET_TABLESPACE;
   }
   ```

2. **Version chains are self-describing**:
   ```c
   struct SBRecordHeader {
       TransactionId   rhd_transaction;     // Tells us WHEN created
       UUID            rhd_back_version;     // Points to previous version
       uint32_t        rhd_flags;            // RHD_CHAIN, RHD_DELTA, etc.
   };
   ```

3. **Snapshot isolation already exists**:
   - Queries have snapshot (xmin, xmax, xip array)
   - Visibility checks use Transaction Inventory Pages (TIP)
   - No new visibility logic needed - just check tablespace!

---

### 3. Write Routing During Migration

**The Challenge**: Where do writes go during migration?

**With MGA**:

```c
Status insertTuple(Relation rel, HeapTuple tuple) {
    TableInfo table = getTableInfo(rel->rel_id);

    tablespace_id target;
    if (table.migration_in_progress &&
        currentXID >= table.migration_start_xid) {
        // New inserts go to TARGET
        target = table.target_tablespace_id;
    } else {
        target = table.tablespace_id;
    }

    return heapInsert(target, rel, tuple);
}

Status updateTuple(Relation rel, TID old_tid, HeapTuple new_tuple) {
    // UPDATE creates back version in CURRENT location,
    // new version in SAME location (MGA principle!)

    HeapTuple old_tuple = fetchTuple(old_tid);
    tablespace_id ts = old_tuple->tablespace_id;  // Stay in same TS

    // Create back version
    createBackVersion(ts, old_tuple);

    // Modify in-place
    modifyInPlace(ts, old_tid, new_tuple);

    return STATUS_OK;
}
```

**Key Point**: UPDATEs don't need special routing because they modify in-place!

---

### 4. Version Chain Continuity

**The Beauty of MGA for Migration**:

```
Before Migration (all in SOURCE tablespace):
  Primary Record (TID 100:5) → Back Version (TID 100:10) → Back Version (TID 100:15)

During Migration:
  Target: New Primary (TID 200:5) [migrated copy]
  Source: Old Primary (TID 100:5) → Back Versions (100:10, 100:15)

After Final Swap:
  Index TIDs updated: 100:5 → 200:5 (ONE-TIME operation)
  Version chain: 200:5 (target) → 100:5 (source, now a back version) → 100:10 → 100:15
```

**Implementation**:
```c
// During migration copy
void copyRecordToTarget(TID source_tid, tablespace_id target_ts) {
    HeapTuple source_record = fetchTuple(source_ts, source_tid);

    // Copy to target tablespace
    TID target_tid = allocateTID(target_ts);
    copyData(target_tid, source_record->data);

    // Link version chain: target → source
    SBRecordHeader* target_header = getHeader(target_tid);
    target_header->rhd_transaction = migration_xid;
    target_header->rhd_back_version = source_tid;  // Points back to source!
    target_header->rhd_flags |= RHD_CHAIN;

    // Source record becomes a back version (no modification needed!)
}
```

---

## Comparison: PostgreSQL MVCC vs Firebird MGA for ONLINE Migration

| Feature | PostgreSQL MVCC | Firebird MGA (ScratchBird) |
|---------|-----------------|----------------------------|
| **Primary Record Location** | Changes on UPDATE | STABLE (never changes) |
| **Index Updates on UPDATE** | Always (or HOT if lucky) | Never (unless indexed columns change) |
| **Version Chain Direction** | Oldest-to-Newest (O2N) | Newest-to-Oldest (N2O) |
| **Migration Complexity** | HIGH (dual tuple tracking) | MEDIUM (dual tablespace, but stable TIDs) |
| **Index Migration** | Must update continuously | ONE-TIME update at final swap |
| **Write Routing Complexity** | HIGH (new tuple location?) | LOW (UPDATEs modify in-place) |
| **Visibility Checks** | Complex (tuple chain traversal) | Simple (check xmin vs migration_xid) |
| **Version Chain Continuity** | Broken (old chain in source, new in target) | Preserved (target → source chain) |
| **Estimated Effort (ONLINE)** | 80-120 hours | **60-80 hours** |

---

## Why Previous Analysis Was Wrong

### Mistake #1: Assumed PostgreSQL MVCC

The previous analysis (`PHASE5_TASK5_4_ONLINE_MIGRATION_ANALYSIS.md`) said:

> **Task 5.4.2: Concurrent Write Support (12-18 hours)**
> - Write Routing: New writes must go to target tablespace
> - Update Propagation: UPDATE on a migrated tuple must update target tablespace

This is **correct for PostgreSQL** (where UPDATEs create new tuples), but **unnecessary for MGA** (where UPDATEs modify in-place).

---

### Mistake #2: Overestimated Infrastructure Requirements

The previous analysis said:

> **ONLINE Migration Requires**:
> - Full MVCC implementation (xmin/xmax tracking)
> - WAL integration (crash recovery)
> - Lock manager (page-level locks)
> - Snapshot tracking (oldest active xid)

But ScratchBird ALREADY HAS:
- ✅ Full MGA implementation (`TRANSACTION_MGA_CORE.md`, `MGA_IMPLEMENTATION.md`)
- ✅ 64-bit transaction IDs with TIP (Transaction Inventory Pages)
- ✅ Snapshot isolation (xmin, xmax, xip)
- ✅ Version chain traversal (`sb_get_visible_version`)
- ✅ Garbage collection (sweep process)

**We don't need to build MVCC infrastructure - it already exists!**

---

### Mistake #3: Compared to PostgreSQL pg_repack

The previous analysis said:

> **PostgreSQL ONLINE Migration**:
> - Does NOT support ONLINE tablespace migration natively
> - Users must use external tools (pg_repack, pg_squeeze)
> - These tools are complex (3,000+ lines of C code)

But pg_repack is complex **because it has to work around PostgreSQL's limitations**:
- Must create triggers to track changes
- Must create shadow table
- Must cope with O2N version chains

**ScratchBird doesn't have these limitations!** MGA makes it simpler.

---

## Revised ONLINE Migration Design (MGA-Aware)

### Architecture

```
Phase 1: COPYING
  - Background thread copies pages from SOURCE to TARGET
  - Primary records remain in SOURCE (stable TID)
  - Copied records in TARGET have back pointers to SOURCE

Phase 2: CATCH-UP
  - Re-copy pages modified during COPYING (dirty page tracking)
  - Converge when dirty_page_rate < copy_rate

Phase 3: FINAL SWAP (< 100ms downtime)
  - Update catalog: table.tablespace_id = TARGET
  - Update index TIDs: SOURCE → TARGET (ONE-TIME, uses existing Task 5.2/5.3 logic)
  - Set migration_in_progress = false
  - Commit transaction

Phase 4: CLEANUP
  - Wait for OST > migration_start_xid (old snapshots closed)
  - Deallocate SOURCE pages
  - Remove migration state
```

### Key Simplifications vs PostgreSQL

1. **No dual visibility layer needed**: Transaction IDs tell us which tablespace
2. **No complex write routing**: UPDATEs modify in-place, INSERTs check migration state
3. **No version chain stitching**: Version chains naturally point SOURCE ← TARGET
4. **One-time index update**: At final swap, not continuous

---

## Complexity Comparison

### PostgreSQL ONLINE Migration (pg_repack approach)

```
1. Create shadow table in target tablespace (100 lines)
2. Create triggers on source table to capture changes (200 lines)
3. Copy initial data to shadow table (150 lines)
4. Apply captured changes incrementally (300 lines)
5. Swap table names atomically (100 lines)
6. Drop triggers and old table (50 lines)

TOTAL: ~900 lines + external tool logic
COMPLEXITY: HIGH (trigger management, dual table tracking)
```

### ScratchBird MGA ONLINE Migration

```
1. Mark table as "migration in progress" (50 lines)
2. Background copy pages SOURCE → TARGET (200 lines)
3. TID resolver: check xmin vs migration_xid (100 lines)
4. Write router: INSERTs → TARGET (50 lines)
5. Catch-up: re-copy dirty pages (150 lines)
6. Final swap: update catalog + index TIDs (150 lines)
7. Cleanup: deallocate SOURCE pages (100 lines)

TOTAL: ~800 lines (WITHIN the engine, no external tools)
COMPLEXITY: MEDIUM (leverages existing MGA infrastructure)
```

**Insight**: Similar line count, but MGA version is SIMPLER because:
- No triggers needed (change tracking via xmin)
- No dual table (dual tablespace, but same logical table)
- No version chain stitching (MGA handles it)

---

## Effort Estimate Revision

### Original Estimate (based on PostgreSQL MVCC)

From `PHASE5_TASK5_4_ONLINE_MIGRATION_ANALYSIS.md`:

```
Task 5.4.1: Concurrent Read Support (8-12 hours)
Task 5.4.2: Concurrent Write Support (12-18 hours)
Task 5.4.3: Catch-Up Phase (10-15 hours)
Task 5.4.4: Final Swap (5-8 hours)
Task 5.4.5: Cleanup (5-7 hours)

TOTAL: 40-60 hours
```

### Revised Estimate (MGA-aware)

```
Task 5.4.0: Architecture Design (8-10 hours) [NEW - critical for MGA approach]
Task 5.4.1: Migration State Management (8-10 hours)
Task 5.4.2: Dual-Source Visibility (12-15 hours) [SIMPLER: use xmin checks]
Task 5.4.3: Write Routing (10-12 hours) [SIMPLER: UPDATEs in-place]
Task 5.4.4: Incremental Copy (8-10 hours)
Task 5.4.5: Catch-Up (6-8 hours)
Task 5.4.6: Final Swap (8-10 hours)
Task 5.4.7: Cleanup (4-5 hours)
Task 5.4.8: Error Handling (6-8 hours) [NEW - important for robustness]
Task 5.4.9: Integration Testing (6-8 hours) [NEW - validation]

TOTAL: 66-96 hours
```

**Why higher than original?**
- Added architecture design phase (critical for correctness)
- Added comprehensive error handling (wasn't in original)
- Added integration testing (original deferred to "future")
- BUT: Individual tasks are SIMPLER due to MGA

---

## Required Materials and Research

### For Implementation of ONLINE Migration

**Already Have** (in ScratchBird specs):
1. ✅ `MGA_IMPLEMENTATION.md` - Full MGA architecture
2. ✅ `TRANSACTION_MGA_CORE.md` - Transaction management, TIP, snapshots
3. ✅ `TRANSACTION_LOCK_MANAGER.md` - Lock management (if needed)
4. ✅ Existing codebase - MGA already implemented!

**Need to Study** (from web research):

1. **Firebird ONLINE Operations**:
   - Source: https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref40/fblangref40-ddl.html
   - Section: ALTER TABLE ... ALTER TYPE (online column modification)
   - Key insight: How Firebird handles concurrent reads/writes during schema changes

2. **Firebird Sweep Process**:
   - Source: https://www.firebirdsql.org/file/documentation/html/en/firebirddocs/gfix/firebird-gfix.html
   - Section: Sweeping
   - Key insight: When is it safe to deallocate old versions (OIT/OST logic)

3. **Firebird Multi-File Databases**:
   - Source: https://firebirdsql.org/refdocs/langrefupd21-ddl-database.html
   - Section: ALTER DATABASE ADD FILE
   - Key insight: How Firebird manages multiple storage files (analogous to tablespaces)

**Additional Reading** (helpful but not required):

4. **PostgreSQL pg_repack**:
   - Source: https://github.com/reorg/pg_repack
   - Purpose: Learn what NOT to do (trigger-based approach is wrong for MGA)

5. **Oracle ONLINE Operations**:
   - Source: https://docs.oracle.com/en/database/oracle/oracle-database/19/admin/managing-tables.html
   - Section: Moving Tables Online
   - Key insight: Oracle's approach (similar to Firebird in some ways)

---

## Key Research Questions (TO BE ANSWERED)

### Question 1: How does Firebird handle version chain continuity across files?

**Why important**: During migration, version chains span SOURCE and TARGET tablespaces.

**Research approach**:
- Read Firebird source: `src/jrd/vio.cpp` (version chain traversal)
- Understand: How `rhd_back_version` UUID is resolved across files
- Answer: Does Firebird store file ID in version pointer? Or global UUID?

**ScratchBird decision**:
- Use TID format: (tablespace_id, page_num, slot)
- Version chains: TARGET (ts=2, page=100) → SOURCE (ts=1, page=50)
- Natural cross-tablespace pointers!

---

### Question 2: How does Firebird's sweep decide when old versions are safe to remove?

**Why important**: After migration, SOURCE pages can't be deallocated until all old snapshots are closed.

**Research approach**:
- Read `TRANSACTION_MGA_CORE.md` lines 911-930 (sweep trigger formula)
- Understand: OIT (Oldest Interesting Transaction) vs OST (Oldest Snapshot Transaction)
- Formula: `(OST - OIT) > sweep_interval`

**ScratchBird decision**:
- Use OST (Oldest Snapshot Transaction) to decide cleanup safety
- Cleanup when: `currentOST > migration_start_xid`
- Rationale: All pre-migration snapshots have closed

---

### Question 3: How does Firebird handle concurrent index updates during ONLINE operations?

**Why important**: Final swap updates all index TIDs at once.

**Research approach**:
- Read Firebird source: Index TID update logic
- Understand: Does Firebird lock entire index? Or page-level locks?

**ScratchBird decision** (from existing implementation):
- Use B-Tree `updateTIDsAfterMigration()` pattern (Task 5.2)
- Pin index pages, update TIDs, mark dirty, unpin
- No special locking needed (transaction isolation provides safety)

---

## Implementation Checklist

### Before Starting ONLINE Migration

- [ ] ✅ Review `MGA_IMPLEMENTATION.md` thoroughly
- [ ] ✅ Review `TRANSACTION_MGA_CORE.md` for TIP/snapshot logic
- [ ] ✅ Understand existing version chain traversal code
- [ ] ✅ Complete Task 5.1.3 (TOAST handling)
- [ ] ✅ Complete Tasks 5.3.2-5.3.6 (all index types)
- [ ] ✅ Complete Phase 3.1 (Autoextend)

### During ONLINE Migration Implementation

- [ ] Write architecture document (Task 5.4.0)
- [ ] Get architecture review/approval
- [ ] Implement state management (Task 5.4.1)
- [ ] Implement TID resolver (Task 5.4.2)
- [ ] Test dual-source visibility
- [ ] Implement write routing (Task 5.4.3)
- [ ] Test concurrent INSERTs/UPDATEs
- [ ] Implement incremental copy (Task 5.4.4)
- [ ] Implement catch-up (Task 5.4.5)
- [ ] Implement final swap (Task 5.4.6)
- [ ] Test atomic swap under load
- [ ] Implement cleanup (Task 5.4.7)
- [ ] Implement error handling (Task 5.4.8)
- [ ] Integration testing (Task 5.4.9)

### Testing Validation

- [ ] Unit tests for TID resolver
- [ ] Unit tests for write router
- [ ] Integration test: concurrent SELECTs during migration
- [ ] Integration test: concurrent INSERTs during migration
- [ ] Integration test: concurrent UPDATEs during migration
- [ ] Stress test: 1M row table migration
- [ ] Stress test: high write load during migration
- [ ] Performance test: query latency overhead < 5%
- [ ] Correctness test: compare checksums before/after migration

---

## Conclusion

**ONLINE migration for ScratchBird is MORE FEASIBLE than initially thought** because:

1. **MGA infrastructure already exists** (no need to build MVCC from scratch)
2. **Stable TIDs simplify visibility** (transaction IDs tell us which tablespace)
3. **In-place UPDATEs simplify write routing** (no dual tuple tracking)
4. **One-time index update** (at final swap, not continuous)

**Effort estimate**: 66-96 hours (vs original 40-60 hours, but with better design)

**Risk level**: MEDIUM (vs HIGH for PostgreSQL-style approach)

**Feasibility for ALPHA**: ✅ YES - with proper architecture design and testing

---

**Next Steps**:

1. Complete remaining foundation tasks (Autoextend, TOAST, index types)
2. Write detailed architecture document (Task 5.4.0)
3. Get architecture review from stakeholders
4. Begin implementation with state management (Task 5.4.1)

---

**Document Version**: 1.0
**Last Updated**: October 21, 2025
**Status**: ARCHITECTURAL ANALYSIS COMPLETE
**References**:
- `/docs/specifications/parser/v3/MGA_IMPLEMENTATION.md`
- `/docs/specifications/parser/v3/TRANSACTION_MGA_CORE.md`
- `docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md`
