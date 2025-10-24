# Documentation Reorganization Plan

**Date**: 2025-10-23
**Status**: In Progress
**Purpose**: Clean up documentation structure after Sprints 0-6 (Tablespace + MGA compliance work)

---

## Executive Summary

Over the past sprints (Phase 1 through Phase 6, Sprint 0 MGA bug fix, and MGA catalog compliance work), documentation has accumulated in the root `/docs/` directory. This plan reorganizes documentation into the proper structure, identifies missing documentation, and prepares for Phase 7 (Advanced Features).

---

## Current Issues

### Problem 1: Root `/docs/` Directory Cluttered with Status Files

The following STATUS_* and phase-specific files are in `/docs/` root but should be in `/docs/status/`:

```
STATUS_PHASE1_TASK1_1_DATA_STRUCTURES.md
STATUS_PHASE1_TASK1_1.md
STATUS_PHASE1_TASK1_2_5_TID_ANALYSIS.md
STATUS_PHASE1_TASK1_4.md
STATUS_PHASE1_TASK1_5.md
STATUS_PHASE1_TASK1_6.md
STATUS_PHASE2_TASK2_1.md
STATUS_PHASE2_TASK2_2.md
STATUS_PHASE2_TASK2_4.md
STATUS_PHASE2_TASK2_5.md
STATUS_PHASE2_TASK2_6.md
STATUS_PHASE3_AUTOEXTEND.md
STATUS_PHASE3_TASK3_1_ARCHITECTURAL_DECISION.md
STATUS_PHASE3_TASK3_2_ARCHITECTURAL_ANALYSIS.md
STATUS_PHASE4A_TASK4A_1_BRIN_INDEX.md
STATUS_PHASE4A_TASK4A_2_HNSW_INDEX.md
STATUS_PHASE6_ATTACH_DETACH_COMPLETE.md
STATUS_PHASE6_ATTACH_DETACH_PARTIAL.md
STATUS_SPRINT0_MGA_BUG_FIX.md
STATUS_SPRINT4_ONLINE_MIGRATION_INFRASTRUCTURE.md
STATUS_SPRINT5_EXECUTION_ENGINE.md
STATUS_SPRINT6_ONLINE_MIGRATION_POLISH.md
```

### Problem 2: Miscellaneous Phase/Completion Files in Root

These summary/completion files should be in `/docs/status/`:

```
PHASE1_TASK1_2_ARCHITECTURAL_NOTE.md
PHASE2_GC_COMPLETION_SUMMARY.md
PHASE4_NEW_INDEX_TYPES_DEPENDENCY_ANALYSIS.md
MIGRATION_SUMMARY_2025_10_03.md
DOCUMENTATION_UPDATE_SUMMARY.md
```

### Problem 3: Analysis/Review Files in Root

These should be in `/docs/planning/` or `/docs/audit/`:

```
MGA_COMPLIANCE_REVIEW_TABLESPACE.md → /docs/audit/
INDEX_MGA_ALPHA_READINESS_SUMMARY.md → /docs/audit/
MGA_ALPHA_STATUS.md → /docs/status/
PAGE_SIZE_PERFORMANCE_CONSIDERATIONS.md → /docs/design/
```

### Problem 4: Session Summary in Root

```
SESSION_SUMMARY_2025_10_23_MGA_CATALOG_COMPLIANCE.md → /docs/status/sessions/
```

### Problem 5: Guide Files Scattered

The following guide files should be consolidated in `/docs/guides/`:

**Currently in /docs/ root:**
```
CI_CD_GUIDE.md → Already should be here (OK)
CONCURRENCY_PATTERNS.md → /docs/guides/
ERROR_HANDLING_GUIDE.md → /docs/guides/
LOCKING_PROTOCOL.md → /docs/guides/
RESOURCE_MANAGEMENT.md → /docs/guides/
```

**Already in /docs/guides/:**
```
PHASE_1_5_COMPLETION_GUIDE.md
PHASE_1_5_FINAL_STEPS.md
PHASE_1_5_MIGRATION_GUIDE.md
```

---

## Reorganization Actions

### Action 1: Move STATUS_* Files to `/docs/status/tablespace/`

Create new subdirectory: `/docs/status/tablespace/` (for tablespace-specific status docs)

**Move these files:**

```bash
/docs/STATUS_PHASE1_TASK1_1_DATA_STRUCTURES.md       → /docs/status/tablespace/PHASE1_TASK1_1_DATA_STRUCTURES.md
/docs/STATUS_PHASE1_TASK1_1.md                        → /docs/status/tablespace/PHASE1_TASK1_1.md
/docs/STATUS_PHASE1_TASK1_2_5_TID_ANALYSIS.md        → /docs/status/tablespace/PHASE1_TASK1_2_5_TID_ANALYSIS.md
/docs/STATUS_PHASE1_TASK1_4.md                        → /docs/status/tablespace/PHASE1_TASK1_4.md
/docs/STATUS_PHASE1_TASK1_5.md                        → /docs/status/tablespace/PHASE1_TASK1_5.md
/docs/STATUS_PHASE1_TASK1_6.md                        → /docs/status/tablespace/PHASE1_TASK1_6.md
/docs/STATUS_PHASE2_TASK2_1.md                        → /docs/status/tablespace/PHASE2_TASK2_1.md
/docs/STATUS_PHASE2_TASK2_2.md                        → /docs/status/tablespace/PHASE2_TASK2_2.md
/docs/STATUS_PHASE2_TASK2_4.md                        → /docs/status/tablespace/PHASE2_TASK2_4.md
/docs/STATUS_PHASE2_TASK2_5.md                        → /docs/status/tablespace/PHASE2_TASK2_5.md
/docs/STATUS_PHASE2_TASK2_6.md                        → /docs/status/tablespace/PHASE2_TASK2_6.md
/docs/STATUS_PHASE3_AUTOEXTEND.md                     → /docs/status/tablespace/PHASE3_AUTOEXTEND.md
/docs/STATUS_PHASE3_TASK3_1_ARCHITECTURAL_DECISION.md → /docs/status/tablespace/PHASE3_TASK3_1_ARCHITECTURAL_DECISION.md
/docs/STATUS_PHASE3_TASK3_2_ARCHITECTURAL_ANALYSIS.md → /docs/status/tablespace/PHASE3_TASK3_2_ARCHITECTURAL_ANALYSIS.md
/docs/STATUS_PHASE4A_TASK4A_1_BRIN_INDEX.md           → /docs/status/tablespace/PHASE4A_TASK4A_1_BRIN_INDEX.md
/docs/STATUS_PHASE4A_TASK4A_2_HNSW_INDEX.md           → /docs/status/tablespace/PHASE4A_TASK4A_2_HNSW_INDEX.md
/docs/STATUS_PHASE6_ATTACH_DETACH_COMPLETE.md         → /docs/status/tablespace/PHASE6_ATTACH_DETACH_COMPLETE.md
/docs/STATUS_PHASE6_ATTACH_DETACH_PARTIAL.md          → /docs/status/tablespace/PHASE6_ATTACH_DETACH_PARTIAL.md
```

### Action 2: Move Sprint Status Files to `/docs/status/sprints/`

Create new subdirectory: `/docs/status/sprints/` (for sprint summaries)

**Move these files:**

```bash
/docs/STATUS_SPRINT0_MGA_BUG_FIX.md                      → /docs/status/sprints/SPRINT0_MGA_BUG_FIX.md
/docs/STATUS_SPRINT4_ONLINE_MIGRATION_INFRASTRUCTURE.md  → /docs/status/sprints/SPRINT4_ONLINE_MIGRATION_INFRASTRUCTURE.md
/docs/STATUS_SPRINT5_EXECUTION_ENGINE.md                 → /docs/status/sprints/SPRINT5_EXECUTION_ENGINE.md
/docs/STATUS_SPRINT6_ONLINE_MIGRATION_POLISH.md          → /docs/status/sprints/SPRINT6_ONLINE_MIGRATION_POLISH.md
```

### Action 3: Move Phase Summary Files to `/docs/status/`

**Move these files:**

```bash
/docs/PHASE1_TASK1_2_ARCHITECTURAL_NOTE.md     → /docs/status/PHASE1_TASK1_2_ARCHITECTURAL_NOTE.md
/docs/PHASE2_GC_COMPLETION_SUMMARY.md          → /docs/status/PHASE2_GC_COMPLETION_SUMMARY.md
/docs/PHASE4_NEW_INDEX_TYPES_DEPENDENCY_ANALYSIS.md → /docs/status/PHASE4_NEW_INDEX_TYPES_DEPENDENCY_ANALYSIS.md
/docs/MIGRATION_SUMMARY_2025_10_03.md          → /docs/status/MIGRATION_SUMMARY_2025_10_03.md
/docs/DOCUMENTATION_UPDATE_SUMMARY.md          → /docs/status/DOCUMENTATION_UPDATE_SUMMARY.md
```

### Action 4: Move MGA/Audit Files to `/docs/audit/`

**Move these files:**

```bash
/docs/MGA_COMPLIANCE_REVIEW_TABLESPACE.md      → /docs/audit/MGA_COMPLIANCE_REVIEW_TABLESPACE.md
/docs/INDEX_MGA_ALPHA_READINESS_SUMMARY.md     → /docs/audit/INDEX_MGA_ALPHA_READINESS_SUMMARY.md
/docs/MGA_ALPHA_STATUS.md                      → /docs/status/MGA_ALPHA_STATUS.md
```

### Action 5: Move Design/Analysis File

**Move this file:**

```bash
/docs/PAGE_SIZE_PERFORMANCE_CONSIDERATIONS.md  → /docs/design/PAGE_SIZE_PERFORMANCE_CONSIDERATIONS.md
```

### Action 6: Move Session Summary to `/docs/status/sessions/`

Create new subdirectory: `/docs/status/sessions/` (for session summaries)

**Move this file:**

```bash
/docs/SESSION_SUMMARY_2025_10_23_MGA_CATALOG_COMPLIANCE.md → /docs/status/sessions/SESSION_SUMMARY_2025_10_23_MGA_CATALOG_COMPLIANCE.md
```

### Action 7: Move Guide Files to `/docs/guides/`

**Move these files:**

```bash
/docs/CONCURRENCY_PATTERNS.md  → /docs/guides/CONCURRENCY_PATTERNS.md
/docs/ERROR_HANDLING_GUIDE.md  → /docs/guides/ERROR_HANDLING_GUIDE.md
/docs/LOCKING_PROTOCOL.md      → /docs/guides/LOCKING_PROTOCOL.md
/docs/RESOURCE_MANAGEMENT.md   → /docs/guides/RESOURCE_MANAGEMENT.md
```

**Keep in root** (these are top-level guides):
```
/docs/CI_CD_GUIDE.md
/docs/INDEX.md
```

---

## Final Directory Structure

After reorganization:

```
/docs/
├── CI_CD_GUIDE.md                     # Top-level CI/CD guide
├── INDEX.md                           # Documentation index
│
├── archive/                           # Historical documentation (pre-Phase 1)
├── audit/                             # Compliance audits and reviews
│   ├── MGA_COMPLIANCE_REVIEW_TABLESPACE.md (NEW LOCATION)
│   ├── INDEX_MGA_ALPHA_READINESS_SUMMARY.md (NEW LOCATION)
│   └── ...existing audit files...
│
├── change_requests/                   # Change request tracking
├── design/                            # Design documents
│   ├── PAGE_SIZE_PERFORMANCE_CONSIDERATIONS.md (NEW LOCATION)
│   └── ...existing design files...
│
├── development/                       # Development processes and guides
│   ├── DOCUMENTATION_REORGANIZATION_PLAN.md (THIS FILE)
│   └── ...existing development files...
│
├── guides/                            # User guides and how-tos
│   ├── CONCURRENCY_PATTERNS.md (NEW LOCATION)
│   ├── ERROR_HANDLING_GUIDE.md (NEW LOCATION)
│   ├── LOCKING_PROTOCOL.md (NEW LOCATION)
│   ├── RESOURCE_MANAGEMENT.md (NEW LOCATION)
│   ├── PHASE_1_5_COMPLETION_GUIDE.md
│   ├── PHASE_1_5_FINAL_STEPS.md
│   └── PHASE_1_5_MIGRATION_GUIDE.md
│
├── issues/                            # Issue tracking
├── planning/                          # Implementation planning
│   └── ...existing planning files...
│
├── reference/                         # Reference materials
├── specifications/                    # SQL and engine specifications
│
├── status/                            # Status reports
│   ├── CURRENT_STATUS.md              # Main status document
│   ├── MGA_ALPHA_STATUS.md (NEW LOCATION)
│   ├── PHASE1_TASK1_2_ARCHITECTURAL_NOTE.md (NEW LOCATION)
│   ├── PHASE2_GC_COMPLETION_SUMMARY.md (NEW LOCATION)
│   ├── PHASE4_NEW_INDEX_TYPES_DEPENDENCY_ANALYSIS.md (NEW LOCATION)
│   ├── MIGRATION_SUMMARY_2025_10_03.md (NEW LOCATION)
│   ├── DOCUMENTATION_UPDATE_SUMMARY.md (NEW LOCATION)
│   │
│   ├── sessions/                      # Session summaries (NEW)
│   │   └── SESSION_SUMMARY_2025_10_23_MGA_CATALOG_COMPLIANCE.md
│   │
│   ├── sprints/                       # Sprint status reports (NEW)
│   │   ├── SPRINT0_MGA_BUG_FIX.md
│   │   ├── SPRINT4_ONLINE_MIGRATION_INFRASTRUCTURE.md
│   │   ├── SPRINT5_EXECUTION_ENGINE.md
│   │   └── SPRINT6_ONLINE_MIGRATION_POLISH.md
│   │
│   ├── tablespace/                    # Tablespace implementation status (NEW)
│   │   ├── PHASE1_TASK1_1_DATA_STRUCTURES.md
│   │   ├── PHASE1_TASK1_1.md
│   │   ├── PHASE1_TASK1_2_5_TID_ANALYSIS.md
│   │   ├── PHASE1_TASK1_4.md
│   │   ├── PHASE1_TASK1_5.md
│   │   ├── PHASE1_TASK1_6.md
│   │   ├── PHASE2_TASK2_1.md
│   │   ├── PHASE2_TASK2_2.md
│   │   ├── PHASE2_TASK2_4.md
│   │   ├── PHASE2_TASK2_5.md
│   │   ├── PHASE2_TASK2_6.md
│   │   ├── PHASE3_AUTOEXTEND.md
│   │   ├── PHASE3_TASK3_1_ARCHITECTURAL_DECISION.md
│   │   ├── PHASE3_TASK3_2_ARCHITECTURAL_ANALYSIS.md
│   │   ├── PHASE4A_TASK4A_1_BRIN_INDEX.md
│   │   ├── PHASE4A_TASK4A_2_HNSW_INDEX.md
│   │   ├── PHASE6_ATTACH_DETACH_COMPLETE.md
│   │   └── PHASE6_ATTACH_DETACH_PARTIAL.md
│   │
│   └── ...existing type implementation status files...
│
└── testing/                           # Test plans and results
```

---

## Missing Documentation Identified

### 1. **Phase 7 Planning Document** (HIGH PRIORITY)

**File**: `/docs/planning/PHASE7_IMPLEMENTATION_PLAN.md`

**Content**: Detailed implementation plan for Phase 7 (Advanced Features):
- Tablespace space management and SHRINK operations
- Tablespace rebalancing
- Tablespace quotas
- pg_tablespace system views
- Performance monitoring and statistics
- Administrative commands

**Estimated Effort**: 50-66 hours (from PROJECT_CONTEXT.md)

### 2. **MGA Catalog Compliance Test Report** (MEDIUM PRIORITY)

**File**: `/docs/testing/MGA_CATALOG_COMPLIANCE_TEST_REPORT.md`

**Content**: Test results for the unit tests in `test_catalog_mga_compliance.cpp`
- Test execution results
- Code coverage analysis
- Performance impact measurements
- Future test recommendations

### 3. **Tablespace Implementation Complete Summary** (HIGH PRIORITY)

**File**: `/docs/status/TABLESPACE_IMPLEMENTATION_COMPLETE.md`

**Content**: Comprehensive summary of Phases 1-6:
- Phase 1: Core Infrastructure (33 hours) ✅
- Phase 1.5: TID Migration (8 hours) ✅
- Phase 2: SQL DDL (21 hours) ✅
- Phase 3: Autoextend/Growth (16.5 hours) ✅
- Phase 4: Migration Infrastructure (9.5 hours) ✅
- Phase 5: OFFLINE Migration (32-41 hours) ✅
- Sprint 4: ONLINE Migration Infrastructure (9.5 hours) ✅
- Sprint 5: ONLINE Migration Execution (4 hours) ✅
- Sprint 0: MGA Bug Fix (2-4 hours) ✅
- Phase 6: Attach/Detach (15 hours) ✅
- MGA Catalog Compliance (3 hours) ✅

**Total**: ~198-223 hours

### 4. **Next Sprint Preparation Document** (HIGH PRIORITY)

**File**: `/docs/planning/SPRINT7_PHASE7_PREPARATION.md`

**Content**:
- Sprint 7 goals (Phase 7 implementation)
- Task breakdown
- Dependencies
- Testing strategy
- Documentation requirements
- Success criteria

### 5. **Catalog Manager API Reference** (MEDIUM PRIORITY)

**File**: `/docs/reference/CATALOG_MANAGER_API.md`

**Content**: API documentation for CatalogManager public methods:
- Tablespace operations (create, alter, drop, attach, detach)
- Schema/table/column operations
- Index operations
- Migration operations
- Garbage collection operations
- Error handling patterns

### 6. **Migration Best Practices Guide** (LOW PRIORITY)

**File**: `/docs/guides/MIGRATION_BEST_PRACTICES.md`

**Content**: Best practices for using OFFLINE and ONLINE migration:
- When to use OFFLINE vs ONLINE
- Performance tuning
- Monitoring during migration
- Rollback procedures
- Common pitfalls

---

## Documentation to Update

### 1. **PROJECT_CONTEXT.md** (ROOT)

**Updates needed:**
- Update "Last Updated" date to 2025-10-23
- Update "Current Priorities" section to reflect Phase 7 as next
- Add reference to new documentation structure
- Update file locations in "Essential File Locations" section

### 2. **docs/INDEX.md**

**Updates needed:**
- Add new sections for `/status/sessions/`, `/status/sprints/`, `/status/tablespace/`
- Update file paths for moved documents
- Add links to missing documentation (once created)

### 3. **docs/status/CURRENT_STATUS.md**

**Updates needed:**
- Mark all Tablespace phases (1-6) as COMPLETE
- Mark MGA Catalog Compliance as COMPLETE
- Set Phase 7 as NEXT
- Update total hours completed
- Update percentage complete statistics

### 4. **docs/planning/TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md**

**Updates needed:**
- Mark Phase 6 as COMPLETE
- Mark MGA Catalog Compliance as COMPLETE
- Update Phase 7 status to IN_PROGRESS (once Sprint 7 starts)
- Add links to completion summaries

---

## Implementation Checklist

- [ ] **Step 1**: Create new subdirectories
  - [ ] `/docs/status/sessions/`
  - [ ] `/docs/status/sprints/`
  - [ ] `/docs/status/tablespace/`

- [ ] **Step 2**: Move STATUS_* files to `/docs/status/tablespace/`
  - [ ] Remove `STATUS_` prefix from filenames during move

- [ ] **Step 3**: Move sprint status files to `/docs/status/sprints/`
  - [ ] Remove `STATUS_` prefix from filenames during move

- [ ] **Step 4**: Move phase summary files to `/docs/status/`

- [ ] **Step 5**: Move MGA/audit files to `/docs/audit/` and `/docs/status/`

- [ ] **Step 6**: Move design file to `/docs/design/`

- [ ] **Step 7**: Move session summary to `/docs/status/sessions/`

- [ ] **Step 8**: Move guide files to `/docs/guides/`

- [ ] **Step 9**: Verify all moves completed successfully

- [ ] **Step 10**: Create missing documentation
  - [ ] `/docs/planning/PHASE7_IMPLEMENTATION_PLAN.md`
  - [ ] `/docs/planning/SPRINT7_PHASE7_PREPARATION.md`
  - [ ] `/docs/status/TABLESPACE_IMPLEMENTATION_COMPLETE.md`
  - [ ] `/docs/testing/MGA_CATALOG_COMPLIANCE_TEST_REPORT.md`
  - [ ] `/docs/reference/CATALOG_MANAGER_API.md`
  - [ ] `/docs/guides/MIGRATION_BEST_PRACTICES.md`

- [ ] **Step 11**: Update existing documentation
  - [ ] `/PROJECT_CONTEXT.md`
  - [ ] `/docs/INDEX.md`
  - [ ] `/docs/status/CURRENT_STATUS.md`
  - [ ] `/docs/planning/TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md`

- [ ] **Step 12**: Verify all internal links still work

- [ ] **Step 13**: Create summary of reorganization work

---

## Success Criteria

1. ✅ All documentation files in correct directories
2. ✅ All STATUS_* files moved to appropriate `/status/` subdirectories
3. ✅ No clutter in `/docs/` root directory (only INDEX.md, CI_CD_GUIDE.md)
4. ✅ All missing high-priority documentation created
5. ✅ All core documentation updated with new file locations
6. ✅ All internal links verified working
7. ✅ PROJECT_CONTEXT.md reflects current state (Phase 6 + MGA compliance COMPLETE, Phase 7 NEXT)

---

## Notes

- This reorganization does NOT affect code files, only documentation
- Archive directories (`/docs/archive/`) are left untouched
- Existing `/docs/status/` files (ALPHA_* and PHASE_*) are NOT moved
- CI/CD, build, and test infrastructure remains unchanged
