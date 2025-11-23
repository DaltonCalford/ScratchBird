# Tablespace Implementation Roadmap - Executive Summary

**Document Status**: EXECUTIVE SUMMARY
**Version**: 1.0
**Date**: October 21, 2025
**Related Documents**:
- [Complete Implementation Roadmap](./TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md)
- [MGA and ONLINE Migration Analysis](./MGA_ONLINE_MIGRATION_ANALYSIS.md)
- [Tablespace Implementation Plan](./TABLESPACE_IMPLEMENTATION_PLAN.md)

---

## Purpose

This document provides an executive summary of the complete tablespace implementation roadmap for ScratchBird ALPHA release, addressing the user's directive:

> "The Alpha is not complete until the engine with all functionality is fully implemented. We are not going to build functionality on partially or incorrectly implemented engine design."

---

## Key Findings

### 1. Firebird MGA vs PostgreSQL MVCC

**Critical Architectural Insight**: ScratchBird uses **Firebird's Multi-Generational Architecture (MGA)**, NOT PostgreSQL's MVCC. This fundamentally changes the implementation approach for ONLINE migration.

**Key Difference**:
- **PostgreSQL**: UPDATEs create new tuple at new location → must update all indexes
- **Firebird MGA**: UPDATEs modify in-place, create back version → index TIDs remain stable

**Impact**: ONLINE migration is SIMPLER and MORE EFFICIENT with MGA than initially thought.

---

### 2. Previous Analysis Was Incorrect

The document `PHASE5_TASK5_4_ONLINE_MIGRATION_ANALYSIS.md` recommended deferring ONLINE migration to post-BETA. This was **incorrect** because:

1. **Wrong concurrency model**: Assumed PostgreSQL MVCC instead of Firebird MGA
2. **Overestimated infrastructure needs**: MGA infrastructure already exists in ScratchBird
3. **Underestimated existing capabilities**: Transaction IDs, TIP, snapshot isolation all present

**Conclusion**: ONLINE migration is FEASIBLE for ALPHA with proper MGA-aware design.

---

### 3. Current Status

**✅ COMPLETE** (~110 hours):
- Phase 0: Research and Specification
- Phase 1: Core Infrastructure (GPID, tablespace files, catalog)
- Phase 1.5: TID Migration to GPID Format
- Phase 2: SQL DDL (CREATE/DROP/ALTER TABLESPACE)
- Phase 3: Autoextend (partial - preallocation complete)
- Phase 4: Migration Infrastructure
- Phase 5: OFFLINE Migration (partial - heap pages, B-Tree, Hash indexes)

**⏸️ INCOMPLETE** (~138-197 hours):
- Phase 3.1: Autoextend completion (12-18 hours)
- Phase 5.1.3: Full TOAST handling (8-12 hours)
- Phase 5.3.2-5.3.6: Other index types (17-24 hours)
- Phase 5.4: ONLINE Migration (66-96 hours) **← MUST IMPLEMENT**
- Phase 6: Attach/Detach (20-30 hours)
- Phase 7: Advanced Features (TBD)

---

## Implementation Roadmap

### Sprint 1: Foundation Completion (20-30 hours)
**Goal**: Complete prerequisites for ONLINE migration

- [ ] Phase 3.1: Autoextend Implementation (12-18 hours)
- [ ] Phase 5.1.3: Full TOAST Handling (8-12 hours)

**Deliverables**:
- Tablespaces auto-extend when full
- TOAST values migrated correctly

---

### Sprint 2: Index Types (17-24 hours - PARALLELIZABLE)
**Goal**: 100% index coverage

- [ ] Phase 5.3.2: Vector/HNSW Index TID Updates (6-8 hours)
- [ ] Phase 5.3.3: GIN Index TID Updates (5-7 hours)
- [ ] Phase 5.3.4: GIST Index TID Updates (4-6 hours)
- [ ] Phase 5.3.5: BRIN Index TID Updates (3-4 hours)
- [ ] Phase 5.3.6: Full-Text Index TID Updates (4-6 hours)

**Deliverables**:
- All index types support tablespace migration
- B-Tree (✅), Hash (✅), Vector, GIN, GIST, BRIN, Full-Text

---

### Sprint 3: ONLINE Migration - Architecture (8-10 hours)
**Goal**: Detailed MGA-aware design

- [ ] Phase 5.4.0: Architecture Design and Specification (8-10 hours)

**Deliverables**:
- Architecture document: `PHASE5_TASK5_4_ONLINE_MIGRATION_MGA_DESIGN.md`
- Migration state catalog schema
- Dual-source visibility model specification
- Write routing strategy
- Risk assessment and mitigation

---

### Sprint 4: ONLINE Migration - Core (30-37 hours)
**Goal**: Concurrent reads/writes during migration

- [ ] Phase 5.4.1: Migration State Management (8-10 hours)
- [ ] Phase 5.4.2: Dual-Source Visibility Layer (12-15 hours)
- [ ] Phase 5.4.3: Write Routing (10-12 hours)

**Deliverables**:
- Migration state tracked in catalog
- TID Resolver Service (determines SOURCE vs TARGET tablespace)
- Queries work correctly during migration
- INSERTs/UPDATEs routed to correct tablespace

---

### Sprint 5: ONLINE Migration - Copy and Swap (26-33 hours)
**Goal**: End-to-end ONLINE migration

- [ ] Phase 5.4.4: Incremental Page Copy (8-10 hours)
- [ ] Phase 5.4.5: Catch-Up Phase (6-8 hours)
- [ ] Phase 5.4.6: Final Swap (8-10 hours)
- [ ] Phase 5.4.7: Cleanup (4-5 hours)

**Deliverables**:
- Background thread copies pages incrementally
- Dirty page tracking and re-copy
- Atomic catalog + index TID update (< 100ms downtime)
- Source page deallocation after safe

---

### Sprint 6: ONLINE Migration - Polish (12-16 hours)
**Goal**: Production-ready

- [ ] Phase 5.4.8: Error Handling and Rollback (6-8 hours)
- [ ] Phase 5.4.9: Integration Testing (6-8 hours)

**Deliverables**:
- Rollback works for all migration phases
- User can cancel migration
- Comprehensive test coverage

---

### Sprint 7: Attach/Detach (20-30 hours)
**Goal**: Tablespace portability

- [ ] Phase 6.1: ATTACH TABLESPACE (10-15 hours)
- [ ] Phase 6.2: DETACH TABLESPACE (10-15 hours)

**Deliverables**:
- Can attach tablespace file from another database
- Can detach tablespace (with data migration if needed)
- Validation and compatibility checks

---

### Sprint 8: Advanced Features (TBD)
**Goal**: Phase 7 features (to be scoped)

Potential features:
- Tablespace quotas
- Tablespace compression
- Tablespace encryption
- Tablespace replication
- Tablespace partitioning
- Tablespace statistics

**NOTE**: Scope to be determined based on ALPHA requirements.

---

## Effort Summary

| Phase | Estimated Hours | Priority | Status |
|-------|----------------|----------|--------|
| **Completed** | ~110 | - | ✅ DONE |
| **Sprint 1** (Foundation) | 20-30 | HIGH | ⏸️ NOT STARTED |
| **Sprint 2** (Index Types) | 17-24 | MEDIUM | ⏸️ NOT STARTED |
| **Sprint 3** (Architecture) | 8-10 | CRITICAL | ⏸️ NOT STARTED |
| **Sprint 4** (ONLINE Core) | 30-37 | HIGH | ⏸️ NOT STARTED |
| **Sprint 5** (Copy/Swap) | 26-33 | HIGH | ⏸️ NOT STARTED |
| **Sprint 6** (Polish) | 12-16 | HIGH | ⏸️ NOT STARTED |
| **Sprint 7** (Attach/Detach) | 20-30 | HIGH | ⏸️ NOT STARTED |
| **Sprint 8** (Advanced) | TBD | MEDIUM | ⏸️ NOT STARTED |
| **TOTAL** | 133-190 hours | - | - |

**Timeline Estimates**:
- **1 developer**: 17-24 weeks
- **2 developers** (parallel index types + ONLINE): 12-16 weeks
- **3+ developers**: 8-12 weeks

---

## Critical Path

**Must complete in order**:

1. Sprint 1 (Foundation) → Blocks Sprint 4
2. Sprint 2 (Index Types) → Blocks Sprint 5 (final swap)
3. Sprint 3 (Architecture) → Blocks Sprint 4-6
4. Sprint 4-6 (ONLINE Migration) → Blocks Sprint 7
5. Sprint 7 (Attach/Detach) → Blocks Sprint 8

**Parallel opportunities**:
- Sprint 1 (Autoextend + TOAST) - can split between 2 developers
- Sprint 2 (5 index types) - can assign 1 per developer (5-way parallelism)
- Sprint 4 (State + Visibility + Routing) - can partially parallelize (2-3 developers)

---

## Risk Assessment

### HIGH RISK: Dual-Source Visibility (Sprint 4)

**Risk**: Complex changes to core query path may introduce bugs

**Mitigation**:
- Comprehensive testing before/after
- Performance benchmarking (< 5% overhead target)
- Feature flag to disable if issues found
- Code review by multiple developers

### MEDIUM RISK: Catch-Up Convergence (Sprint 5)

**Risk**: High write load may prevent migration convergence

**Mitigation**:
- Convergence detection (fail gracefully)
- Option: Brief write pause (< 1 second)
- Document limitation

### MEDIUM RISK: Atomic Swap (Sprint 5)

**Risk**: Swap logic errors could corrupt data

**Mitigation**:
- Transaction-based swap (all-or-nothing)
- Extensive testing under load
- Backup/restore validation

---

## Success Criteria for ALPHA

**Tablespace functionality is COMPLETE when**:

- [ ] ✅ All infrastructure complete (Phases 0-2)
- [ ] ✅ Autoextend works (Phase 3.1)
- [ ] ✅ OFFLINE migration for all data types (Phase 5.1.3 - TOAST)
- [ ] ✅ All index types (Phase 5.2, 5.3.1-5.3.6 - 100% coverage)
- [ ] ✅ ONLINE migration works (Phase 5.4)
- [ ] ✅ Attach/Detach works (Phase 6)
- [ ] ✅ All tests pass
- [ ] ✅ Documentation complete
- [ ] ✅ Zero critical bugs
- [ ] ✅ Performance acceptable (< 5% overhead)

---

## Key Research Materials

### Required Reading (Already Available)

1. **`docs/specifications/MGA_IMPLEMENTATION.md`**:
   - ScratchBird's MGA architecture
   - Record versioning with UUIDs
   - Version chain management
   - Garbage collection

2. **`docs/specifications/TRANSACTION_MGA_CORE.md`**:
   - 64-bit transaction IDs
   - Transaction Inventory Pages (TIP)
   - Snapshot isolation
   - Visibility rules

3. **`docs/planning/TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md`**:
   - Detailed task breakdown
   - Subtask specifications
   - File-level changes
   - Acceptance criteria

4. **`docs/planning/MGA_ONLINE_MIGRATION_ANALYSIS.md`**:
   - PostgreSQL vs Firebird comparison
   - Why MGA simplifies ONLINE migration
   - Architecture patterns
   - Implementation checklist

### External Research (Web)

1. **Firebird Documentation**:
   - Multi-file databases: https://firebirdsql.org/refdocs/langrefupd21-ddl-database.html
   - Sweep process: https://www.firebirdsql.org/file/documentation/html/en/firebirddocs/gfix/firebird-gfix.html
   - ONLINE operations: https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref40/fblangref40-ddl.html

2. **MGA Architecture**:
   - Record versioning: http://www.firebirdfaq.org/faq44/
   - Version chains: https://www.ibexpert.net/ibe/pmwiki.php?n=Doc.Multi-generationalArchitectureMGAAndRecordVersioning
   - Garbage collection: https://www.ibexpert.net/ibe/pmwiki.php?n=Doc.GarbageCollection

3. **PostgreSQL (for comparison)**:
   - pg_repack: https://github.com/reorg/pg_repack (learn what NOT to do)
   - MVCC overview: https://www.postgresql.org/docs/current/mvcc.html

---

## Next Steps

### Immediate Actions

1. **Review this roadmap** with stakeholders
2. **Scope Phase 7** (if needed for ALPHA)
3. **Assign developers** to Sprints 1-2 (can parallelize)
4. **Start Sprint 1**: Autoextend + TOAST (20-30 hours)
5. **Schedule architecture review** for Sprint 3 (Task 5.4.0)

### Long-Term

1. Complete Sprints 1-7 sequentially (with parallelization where possible)
2. Continuous integration testing throughout
3. Performance benchmarking at each sprint completion
4. Documentation updates in parallel with implementation

---

## Conclusion

**ONLINE tablespace migration is FEASIBLE for ScratchBird ALPHA** because:

1. ✅ **MGA infrastructure already exists** (no need to build from scratch)
2. ✅ **Stable TIDs simplify migration** (index updates only at final swap)
3. ✅ **Transaction IDs enable dual-source visibility** (simple xmin checks)
4. ✅ **In-place UPDATEs simplify write routing** (no dual tuple tracking)

**Total remaining effort**: 133-190 hours (feasible for ALPHA timeline)

**Recommended approach**:
- **1-2 developers**: 12-24 weeks (sequential + some parallelization)
- **3+ developers**: 8-12 weeks (aggressive parallelization)

**Key to success**:
- Proper architecture design BEFORE implementation (Sprint 3)
- Leverage existing MGA infrastructure (don't reinvent)
- Comprehensive testing at each sprint
- Follow Firebird patterns, not PostgreSQL patterns

---

**Document Version**: 1.0
**Last Updated**: October 21, 2025
**Status**: EXECUTIVE SUMMARY - Ready for Review
**Next Action**: Stakeholder review and Sprint 1 kickoff
