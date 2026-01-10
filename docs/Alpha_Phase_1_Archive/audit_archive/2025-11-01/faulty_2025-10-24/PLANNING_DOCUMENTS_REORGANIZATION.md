# Planning Documents Reorganization Recommendations
**Date**: October 24, 2025
**Purpose**: Recommend which planning documents to move to implemented/ subdirectory
**Methodology**: Based on comprehensive code audit findings

---

## Summary

**Total Planning Documents**: 41 files
- Currently in /docs/planning: 41 files
- Currently in /docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented: 6 files
- **Recommended to MOVE**: 4 files
- **Recommended to KEEP**: 37 files

---

## Documents to MOVE to /docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/

### 1. SPRINT0_BUG_FIX_COMPLETE.md ✅ **MOVE IMMEDIATELY**

**Reason**: Fully verified as complete in code audit

**Evidence**:
- HeapPage::overwriteTuple() exists: src/core/heap_page.cpp:925-1051
- StorageEngine::updateTuple() cross-page fix exists: src/core/storage_engine.cpp:880-1034
- Unit test exists: tests/unit/test_storage_engine_mga_crosspage.cpp (326 lines)
- All code matches specification exactly

**Status in Document**: "✅ COMPLETE"
**Actual Status**: ✅ COMPLETE (verified)
**Confidence**: HIGH

**Action**: `mv /home/dcalford/CliWork/ScratchBird/docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/SPRINT0_BUG_FIX_COMPLETE.md /home/dcalford/CliWork/ScratchBird/docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/`

---

### 2. SPRINT1_FOUNDATION_COMPLETE.md ✅ **MOVE IMMEDIATELY**

**Reason**: Fully verified as complete in code audit

**Evidence**:
- PageManager::extendTablespace() exists: src/core/page_manager.cpp:1353-1601
- allocatePageInTablespace() with autoextend hook exists: src/core/page_manager.cpp:551-734
- Integration test exists: tests/unit/test_tablespace_autoextend.cpp (386 lines)
- All features match specification

**Status in Document**: "✅ COMPLETE"
**Actual Status**: ✅ COMPLETE (verified)
**Confidence**: HIGH

**Action**: `mv /home/dcalford/CliWork/ScratchBird/docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/SPRINT1_FOUNDATION_COMPLETE.md /home/dcalford/CliWork/ScratchBird/docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/`

---

### 3. SPRINT3_SUMMARY.md ✅ **MOVE IMMEDIATELY**

**Reason**: Design/architecture sprint (no code implementation required)

**Evidence**:
- Architecture document exists: SPRINT3_ONLINE_MIGRATION_ARCHITECTURE.md
- Sprint 3 was design-only (8-10 hours of documentation work)
- Document status: "✅ COMPLETE"
- All deliverables present

**Status in Document**: "✅ COMPLETE"
**Actual Status**: ✅ COMPLETE (design sprint)
**Confidence**: HIGH

**Action**: `mv /home/dcalford/CliWork/ScratchBird/docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/SPRINT3_SUMMARY.md /home/dcalford/CliWork/ScratchBird/docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/`

---

### 4. SPRINT3_ONLINE_MIGRATION_ARCHITECTURE.md ✅ **MOVE IMMEDIATELY**

**Reason**: Architecture document (deliverable complete)

**Evidence**:
- Comprehensive architecture document exists (~15,000 words)
- Contains all required sections (13 parts)
- Referenced by Sprint 4 & 5 planning documents
- No code implementation required for architecture doc

**Status in Document**: "✅ COMPLETE"
**Actual Status**: ✅ COMPLETE (document deliverable)
**Confidence**: HIGH

**Action**: `mv /home/dcalford/CliWork/ScratchBird/docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/SPRINT3_ONLINE_MIGRATION_ARCHITECTURE.md /home/dcalford/CliWork/ScratchBird/docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/`

---

## Documents to KEEP in /docs/archive/2026-01-09/planning/ (NOT complete)

### Sprint 2 Documents ⚠️ **KEEP - Needs Verification**

**Files**:
- SPRINT2_SUMMARY.md
- SPRINT2_IMPLEMENTATION_PROGRESS.md
- SPRINT2_INDEX_TOAST_ANALYSIS.md

**Reason**: Index TID update methods not fully verified

**Status in Documents**: "✅ COMPLETE"
**Actual Status**: ⚠️ PARTIAL - Index implementation files exist but specific methods not found
**Confidence**: MEDIUM

**Evidence**:
- All 6 index files exist (hnsw_index.cpp, gin_index.cpp, brin_index.cpp, etc.)
- Generic `updateTIDsAfterMigration` found (12 occurrences)
- But specific method signatures not found:
  - `HNSWIndex::updateTIDsAfterMigration` - NOT FOUND
  - `GINIndex::updateTIDsAfterMigration` - NOT FOUND
  - `BRINIndex::updateBlockRangesAfterMigration` - NOT FOUND

**Recommendation**: **KEEP until manual inspection verifies methods exist**

**Next Steps**:
1. Manually read src/core/hnsw_index.cpp for TID update logic
2. Manually read src/core/gin_index.cpp for TID update logic
3. Manually read src/core/brin_index.cpp for block range update logic
4. If methods exist (different names), update documentation and MOVE to implemented/
5. If methods missing, update SPRINT2 status to PARTIAL and keep in planning/

---

### Sprint 4 Documents ⚠️ **KEEP - Partially Implemented**

**Files**:
- SPRINT4_IMPLEMENTATION_PLAN.md
- SPRINT4_SUMMARY.md

**Reason**: Only TIDResolver implemented, other tasks unclear

**Status in Documents**:
- SPRINT4_SUMMARY.md: "PLAN READY, IMPLEMENTATION PENDING"
- PROJECT_CONTEXT.md: "✅ COMPLETE" (CONFLICTING!)

**Actual Status**: ⚠️ PARTIAL
- Task 5.4.2 (TIDResolver): ✅ CONFIRMED IMPLEMENTED (557 lines)
- Task 5.4.1 (State Management): ⚠️ UNKNOWN
- Task 5.4.3 (Write Routing): ⚠️ PARTIAL (some evidence found)

**Confidence**: MEDIUM

**Recommendation**: **KEEP until Tasks 5.4.1 and 5.4.3 are verified**

**Next Steps**:
1. Deep dive audit of catalog_manager.h/cpp for migration state tracking
2. Verify TableInfo struct has migration-related fields
3. Verify dirty page tracking implementation
4. Verify write routing for INSERT/UPDATE/DELETE
5. If all tasks complete, MOVE to implemented/
6. If tasks incomplete, update status to "PARTIAL - TIDResolver only"

---

### Sprint 5 Documents ❌ **KEEP - Not Implemented**

**Files**:
- SPRINT5_IMPLEMENTATION_PLAN.md
- SPRINT5_SUMMARY.md

**Reason**: MigrationWorker does not exist

**Status in Documents**:
- SPRINT5_SUMMARY.md: "PLAN READY, IMPLEMENTATION PENDING"
- PROJECT_CONTEXT.md: "✅ COMPLETE" (CONFLICTING!)

**Actual Status**: ❌ NOT IMPLEMENTED
- migration_worker.h: ❌ DOES NOT EXIST
- migration_worker.cpp: ❌ DOES NOT EXIST
- All 5 execution methods: ❌ NOT FOUND

**Confidence**: HIGH (definitive absence)

**Recommendation**: **KEEP as planning documents (not implemented)**

**Next Steps**:
1. Update PROJECT_CONTEXT.md to change Sprint 5 from "COMPLETE" to "NOT STARTED"
2. Implement Sprint 5 if required for Alpha
3. After implementation, MOVE to implemented/

---

### Phase 4 Task Documents ⚠️ **KEEP - Status Uncertain**

**Files** (6 total):
- PHASE4_TASK4_1_1_PARSER_TEST.md
- PHASE4_TASK4_1_2_CATALOG_STUB.md
- PHASE4_TASK4_1_3_PROGRESS_TRACKING.md
- PHASE4_TASK4_1_4_BATCH_PROCESSING.md
- PHASE4_TASK4_1_5_INDEX_TID_UPDATE.md
- PHASE4_TASK4_1_6_EXECUTOR.md

**Reason**: Not verified in this audit

**Recommendation**: **KEEP until individual verification**

**Next Steps**: Per-task audit to verify implementation status

---

### Phase 5 Task Documents ⚠️ **KEEP - Status Uncertain**

**Files** (12 total):
- PHASE5_FULL_IMPLEMENTATION_PLAN.md
- PHASE5_1_HEAP_PAGE_MIGRATION.md
- PHASE5_TASK5_1_1_HEAP_PAGE_ENUMERATION.md
- PHASE5_TASK5_1_2_PAGE_COPYING_TID_REMAPPING.md
- PHASE5_TASK5_1_3_TOAST_HANDLING.md
- PHASE5_TASK5_1_4_TRANSACTION_ROLLBACK.md
- PHASE5_TASK5_2_BTREE_TID_UPDATES.md
- PHASE5_TASK5_3_OTHER_INDEX_TID_UPDATES.md
- PHASE5_TASK5_4_ONLINE_MIGRATION_ANALYSIS.md

**Reason**: OFFLINE migration tasks - not verified in this audit

**Recommendation**: **KEEP until individual verification**

**Next Steps**: Per-task audit to verify implementation status

---

### Phase 7 and Future Work Documents ❌ **KEEP - Not Started**

**Files**:
- PHASE7_COMPLETE_SCOPE.md
- SPRINT7_PHASE7_PREPARATION.md

**Reason**: Future work, not implemented

**Recommendation**: **KEEP in planning/**

---

### Context Variables and Row Identity Documents ❌ **KEEP - Not Started**

**Files** (4 total):
- ALPHA_CONTEXT_VARIABLES_DESIGN.md
- ALPHA_CONTEXT_VARIABLES_V2_SUMMARY.md
- ALPHA_ROW_IDENTITY_AND_TRANSACTION_VISIBILITY.md
- ALPHA_ROW_IDENTITY_ENHANCED.md

**Reason**: Future Alpha features, not implemented

**Recommendation**: **KEEP in planning/**

---

### Reference and Analysis Documents ✅ **KEEP - Valuable Reference**

**Files**:
- MVCC_VS_MGA_CODE_REVIEW.md - Critical architectural reference
- MGA_ONLINE_MIGRATION_ANALYSIS.md - Analysis document
- OFFLINE_TABLE_MIGRATION_DESIGN.md - Design reference
- OFFLINE_TABLE_MIGRATION_TODOS.md - Todo tracking
- INDEX_MGA_IMPLEMENTATION_PLAN.md - Implementation plan
- TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md - Roadmap reference
- TABLESPACE_IMPLEMENTATION_PLAN.md - Plan reference
- TABLESPACE_ROADMAP_SUMMARY.md - Summary reference
- SWEEP_INTEGRATION_PLAN.md - Future integration

**Reason**: Valuable reference documents, not "completed work" to archive

**Recommendation**: **KEEP in planning/ as active references**

---

## Move Commands Script

Here are the exact commands to reorganize the verified-complete documents:

```bash
#!/bin/bash
# Move verified-complete planning documents to implemented/ subdirectory
# Generated: 2025-10-24

BASE="/home/dcalford/CliWork/ScratchBird/docs/planning"

# Verify implemented/ directory exists
mkdir -p "$BASE/implemented"

# Move Sprint 0 (verified complete)
mv "$BASE/SPRINT0_BUG_FIX_COMPLETE.md" "$BASE/implemented/"

# Move Sprint 1 (verified complete)
mv "$BASE/SPRINT1_FOUNDATION_COMPLETE.md" "$BASE/implemented/"

# Move Sprint 3 Summary (design complete)
mv "$BASE/SPRINT3_SUMMARY.md" "$BASE/implemented/"

# Move Sprint 3 Architecture (document complete)
mv "$BASE/SPRINT3_ONLINE_MIGRATION_ARCHITECTURE.md" "$BASE/implemented/"

echo "Moved 4 verified-complete documents to implemented/"
echo "Remaining documents in planning/ need verification before moving"
```

**Save as**: `/home/dcalford/CliWork/ScratchBird/docs/audit/reorganize_planning_docs.sh`

---

## Summary Statistics

**Before Reorganization**:
- /docs/planning: 41 files
- /docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented: 6 files

**After Reorganization**:
- /docs/planning: 37 files
- /docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented: 10 files (6 existing + 4 moved)

**Pending Verification** (before can move):
- Sprint 2: 3 files (need index TID method verification)
- Sprint 4: 2 files (need state management and write routing verification)
- Phase 4 tasks: 6 files (need per-task verification)
- Phase 5 tasks: 9 files (need per-task verification)

**Total Pending Verification**: 20 files

---

## Recommendations

### Immediate Actions

1. **Execute move script** for 4 verified-complete documents
2. **Update PROJECT_CONTEXT.md** to reflect accurate status (critical!)
3. **Create tracking document** for 20 pending-verification files

### Short-Term Actions

4. **Verify Sprint 2 index methods**:
   - Read hnsw_index.cpp, gin_index.cpp, brin_index.cpp
   - Document actual method names
   - Move Sprint 2 docs if verified

5. **Verify Sprint 4 tasks**:
   - Audit catalog_manager.h/cpp for state management
   - Verify write routing in storage_engine.cpp
   - Move Sprint 4 docs if verified

### Long-Term Actions

6. **Create systematic verification process**:
   - Automated code-to-docs verification
   - Regular audits (weekly/monthly)
   - Single source of truth for status

7. **Establish clear completion criteria**:
   - Code exists AND compiles
   - Tests exist AND pass
   - Documentation matches implementation
   - All three must be true before marking "COMPLETE"

---

**END OF REORGANIZATION RECOMMENDATIONS**

**Date**: October 24, 2025
**Status**: READY FOR EXECUTION
