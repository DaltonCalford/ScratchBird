# ScratchBird Project Context

**Last Updated**: 2025-10-30 (Phase 3 Task 14.1 Complete - Full-Text Search Core Types)
**Version**: Alpha 1.4.0
**Status**: Educational/Development (**Phase 3 Started - Text Search Types** 🎉)

> **PURPOSE**: This file provides essential context for AI assistants working on ScratchBird.
> Read this file at session start and after every context compaction.
> **CRITICAL**: Always check /src/sblr/ and /src/parser/ directories - they contain core functionality!

---

## 1. Project Architecture & Design Goals

### **CRITICAL**: 3-Layer Embedded Architecture

ScratchBird is an **embedded database engine** designed as a library for other projects, NOT a standalone database server.

```
┌─────────────────────────────────────────────────────────┐
│ Layer 3: Client Applications                            │
│  - sb_isql (CLI tool for testing)                       │
│  - Future: Custom applications using the engine         │
└─────────────────────────────────────────────────────────┘
                         ↓ uses
┌─────────────────────────────────────────────────────────┐
│ Layer 2: Parser Engines (Dialect-Specific Libraries)    │
│  - libsb_parser_scratchbird.so (Alpha: ScratchBird SQL) │
│  - Future: libsb_parser_firebird.so                     │
│  - Future: libsb_parser_postgres.so                     │
│  - Future: libsb_parser_mysql.so, etc.                  │
│                                                          │
│  Responsibilities:                                       │
│  • SQL → AST parsing (dialect-specific)                 │
│  • AST → SBLR bytecode generation                       │
│  • Result formatting (SBLR → dialect format)            │
│  • Statement caching (per dialect)                      │
│  • Stateless: one-way translation only                  │
└─────────────────────────────────────────────────────────┘
                         ↓ SBLR bytecode
┌─────────────────────────────────────────────────────────┐
│ Layer 1: Database Engine (libscratchbird.so)            │
│  - SBLR bytecode interpreter/executor                   │
│  - Storage engine (buffer pool, pages, heap)            │
│  - Transaction manager (Firebird MGA)                   │
│  - Index engines (6 types: B-tree, Hash, GIN, etc.)     │
│  - Type system (all types from FB/MySQL/PG/MSSQL)       │
│  - Catalog manager (system tables, metadata)            │
│  - All APIs public for direct use                       │
│                                                          │
│  Key: Universal backend via SBLR bytecode               │
└─────────────────────────────────────────────────────────┘
```

### SBLR: The Universal Interface

**SBLR (ScratchBird Binary Language Runner)** is based on Firebird's BLR with extensions.

- **Purpose**: Dialect-agnostic bytecode for database operations
- **Design**: Stack-based interpreter with opcode instructions
- **Location**: `/src/sblr/` and `/include/scratchbird/sblr/`
- **Key Files**:
  - `opcodes.h` - Opcode definitions (188 lines)
  - `executor.h/.cpp` - Bytecode interpreter (3,108 lines)
  - `bytecode_generator.h/.cpp` - AST → SBLR compiler (1,162 lines)

**CRITICAL**: Parsers translate SQL → SBLR (one-way), engine only understands SBLR.

### Parser Engine Design

- **Separate Libraries**: Each SQL dialect has its own parser library
- **Stateless**: Connect, send SBLR, get results, disconnect
- **Version Aware**: Parser checks engine version compatibility at init
- **Swappable**: Engine upgrades transparent to parser (if API compatible)
- **Caching**:
  - Parser caches SQL → SBLR translations (per dialect)
  - Engine caches SBLR execution plans (universal)

### Alpha Completion Criteria (9 Priorities)

**Priority 1**: Data Type Completeness
- Support ALL types from FirebirdSQL, MySQL/MariaDB, PostgreSQL, MSSQL
- Either directly or via domain types
- **Location to check**: `/src/core/types.cpp`, `/include/scratchbird/core/types.h`

**Priority 2**: Index Type Completeness
- Support ALL index types from the 4 major databases
- **Current**: 6 types exist (B-tree, Hash, GIN, HNSW, BRIN, Bitmap)
- **Check against**: Full index type lists from FB/MySQL/PG/MSSQL

**Priority 3**: Data Manipulation Completeness
- Casting, math functions, string functions, temporal extraction
- All low-level operations the 4 major databases perform
- **Location**: `/src/sblr/executor.cpp` (function implementations)

**Priority 4**: Schema Structure
- Recursive schema support
- Well-defined system tables
- Complete metadata catalog

**Priority 5**: SBLR Complete Implementation
- Full specification documented
- All opcodes implemented
- Supporting API calls complete
- **Status**: Appears substantially complete, needs verification

**Priority 6**: Query Optimization
- Statement caching (engine-level, SBLR-based)
- Statement analysis and query plan generation
- Parallel worker threads for execution

**Priority 7**: ScratchBird SQL Parser
- Complete SQL parser for ScratchBird dialect
- AST → SBLR bytecode generator
- **Location**: `/src/parser/`, `/src/sblr/bytecode_generator.cpp`
- **Status**: Exists, needs verification of completeness

**Priority 8**: sb_isql CLI Application
- Interactive SQL command-line interface
- Uses Layer 2 (parser) → Layer 1 (engine)
- Firebird isql-compatible behavior
- **Status**: Not found yet, needs implementation

**Priority 9**: Complete Documentation
- Technical specifications
- Operational guides
- Developer documentation with code examples
- Full specifications for engine to this point

**Alpha Definition**: When Priority 1-9 are complete, Alpha is finished → First release

### Beta Requirements (Deferred from Alpha)

- ONLINE Migration (Sprint 4 & 5) - needed for concurrency testing
- Networking layer
- WAL (Write-Ahead Logging)
- Replication

---

## 2. Current Project State

### Version & Status
- **Current Version**: Alpha 1.0.7
- **Production Ready**: ❌ NO - Educational/Development only
- **Last Major Update**: October 28, 2025
- **Active Development**: ✅ **Phase 1 COMPLETE** - Ready for Phase 2 planning

### Phase 1 Completion (Oct 28, 2025)
**Status**: ✅ **100% COMPLETE** - All 8 critical tasks delivered

| Task | Implementation | Status |
|------|----------------|--------|
| Query Optimizer | 3,653 lines | ✅ 100% |
| UPDATE/DELETE | 1,183 lines | ✅ 100% |
| JOINs | 3,910 lines | ✅ 100% |
| Aggregation | 1,510 lines | ✅ 100% |
| Sorting/Limiting | 855 lines | ✅ 100% |
| Window Functions | 1,050 lines | ✅ 100% |
| JSON Functions | 500 lines | ✅ 100% |
| Conditional Functions | 320 lines | ✅ 100% |

**Total**: 12,981 lines + 200+ test cases

**Audit Result**: Zero critical blockers, 92 non-blocking TODOs
**Documentation**: `/docs/audit/PHASE_1_COMPLETION_AUDIT.md`

### Critical Statistics
```yaml
Storage Engine:      100% complete (buffer pool, pages, TOAST, compression, MGA)
Transaction System:  100% complete (Firebird MGA, 4 isolation levels)
MVCC/MGA:           100% complete (back versioning, cross-page support)
Concurrency:         100% complete (ConnectionContext, locking, deadlock detection)
Indexing:
  - B-tree:          100% complete (with prefix compression)
  - Hash:            100% complete
  - GIN:             100% complete (with posting list compression)
  - Bitmap:          100% complete (Roaring compression)
  - HNSW:            100% complete (vector similarity search)
  - BRIN:            100% complete (block range indexes)
Type System:         95% complete  (30+ types, UUIDv7, timezones, collations)
Query Processing:    ✅ Phase 1 100% complete (all critical features working)
  - Lexer/Parser:    ✅ Phase 1 complete (JOINs, GROUP BY, ORDER BY, LIMIT, Window, JSON, Conditional)
  - Query Planner:   ✅ 100% complete (Cost model, Statistics, Planning, EXPLAIN)
  - Bytecode Gen:    ✅ Phase 1 complete (All Phase 1 features generate bytecode)
  - Executor:        ✅ Phase 1 complete (CRUD, JOINs, Aggregation, Sorting, Window, JSON, Conditional)
Catalog:             75% complete  (metadata persistence)
Code Quality:        98% complete  (RAII, logging, const-correct, zero FIXME/HACK)
CI/CD:               100% complete (TSAN, ASAN, Helgrind, Valgrind, Clang-Tidy)
JSON Library:        ✅ nlohmann/json v3.11.3 integrated via FetchContent
```

### Tablespace Implementation Status

**Phase 1**: ✅ COMPLETE - Core Infrastructure (33 hours)
- GPID addressing system (64-bit: 16-bit tablespace + 48-bit page)
- Tablespace file management
- Catalog structures (pg_tablespace)

**Phase 1.5**: ✅ COMPLETE - TID Migration (8 hours)
- All 6 index types migrated to TID struct API
- Heap layer, storage engine, garbage collector updated

**Phase 2**: ✅ COMPLETE - SQL DDL (21 hours)
- CREATE/DROP/ALTER TABLESPACE
- CREATE TABLE ... TABLESPACE

**Phase 3**: ✅ COMPLETE - Autoextend and Growth (16.5 hours)
- Task 3.1: Autoextend Implementation COMPLETE
- Task 3.2: Preallocation COMPLETE

**Phase 4**: ✅ COMPLETE - Migration Infrastructure (9.5 hours)
- Parser, catalog manager infrastructure
- Batch processing, index TID update, executor

**Phase 5**: ✅ COMPLETE - OFFLINE Migration (~32-41 hours)
- All heap page migration complete
- All 6 index types migrated (B-Tree, Hash, HNSW, GIN, BRIN, Bitmap)
- Full TOAST migration complete

**Sprint 4**: ⚠️ PARTIAL (~33% complete) - ONLINE Migration Infrastructure
- Task 5.4.1: Migration State Management ⚠️ NOT VERIFIED (needs catalog_manager.cpp audit)
- Task 5.4.2: Dual-Source Visibility ✅ COMPLETE (TIDResolver: tid_resolver.h/cpp, 557 lines verified)
- Task 5.4.3: Write Routing During Migration ⚠️ PARTIAL (some evidence in storage_engine.cpp, needs verification)

**Sprint 5**: ❌ NOT IMPLEMENTED (0% complete) - ONLINE Migration Execution
- Task 5.4.4: Copying Phase ❌ NOT IMPLEMENTED (migration_worker.h/cpp do not exist)
- Task 5.4.5: Catch-Up Phase ❌ NOT IMPLEMENTED
- Task 5.4.6: Atomic Swap Phase ❌ NOT IMPLEMENTED
- **CRITICAL**: MigrationWorker class and all 5 execution methods are missing
- **Estimated Effort**: 26-33 hours to implement

**Sprint 0**: ✅ COMPLETE - CRITICAL Bug Fix (2-4 hours)
- Cross-page UPDATE MGA compliance fixed
- HeapPage::overwriteTuple() implementation complete
- TID stability verified

**Phase 6**: ✅ COMPLETE - Attach/Detach Operations (15 hours)
- SQL parser integration (ATTACH/DETACH TABLESPACE)
- Bytecode generation and executor handlers
- Catalog manager methods (attachTablespace, detachTablespace)

**MGA Catalog Fixes**: ✅ COMPLETE - Catalog MGA Compliance (3 hours)
- Bug #1 FIXED: ALTER TABLESPACE now updates records in-place (no catalog bloat)
- Bug #2 FIXED: DROP/DETACH TABLESPACE now marks is_valid=0 (persistent deletion)
- Added updateRecordInHeapPage() and deleteRecordFromHeapPage() template methods
- Added compactCatalogHeapPage() and compactCatalog() for garbage collection
- Unit tests implemented (test_catalog_mga_compliance.cpp)

**Documentation Reorganization**: ✅ COMPLETE (2025-10-23)
- Moved all STATUS_* files to /docs/status/tablespace/
- Moved all SPRINT* files to /docs/status/sprints/
- Moved session summaries to /docs/status/sessions/
- Moved guides to /docs/guides/
- Moved audit files to /docs/audit/
- Clean /docs/ root directory (only INDEX.md, CI_CD_GUIDE.md)

**Total Completed**: ~198-223 hours

---

### Phase 2: Critical Features (Post-Phase 1)

**Phase 2 Wave 1**: ✅ COMPLETE - SQL Integration (October 28, 2025)
- Spatial Types (2,095 lines, 44 tests) ✅
- Array Functions (1,300 lines) ✅
- Text Search Functions (1,318 lines) ✅
- **Total**: 4,713 lines, 50 opcodes, 47 functions

**Phase 2 Wave 2**: ✅ COMPLETE - Advanced SQL Features (October 28, 2025)
- CTEs (707 lines) ✅
- Subqueries (725 lines, 4/4 tests passing) ✅
- Triggers (885 lines) ✅
- **Total**: ~2,400 lines, 11 opcodes, 28 tests

**Phase 2 Wave 3**: ✅ COMPLETE - Spatial/GIS Integration (October 30, 2025)
- R-tree Indexes (2,690 lines) ✅ - Storage + catalog integration
- R-tree Query Planner (548 lines) ✅ - Cost-based spatial optimization
- Spatial Functions (1,800 lines) ✅ - 24 functions via GEOS
- SRID Support (1,608 lines) ✅ - 4 functions, PROJ integration
- Multi-Geometry Infrastructure (1,370 lines) ✅ - Classes complete, SQL deferred to Phase 3
- **Total**: ~9,276 lines, 32 opcodes, 28 operational GIS functions
- **Status**: ~90% PostGIS parity for Phase 2 use cases

**Phase 2 Wave 4**: ✅ COMPLETE - PSQL Stored Procedures (October 30, 2025)
- PSQL language implementation (variables, control flow, exceptions)
- Functions and procedures with parameter support
- Full integration with executor and bytecode generator
- **Status**: Firebird-compatible procedural language operational

**Phase 2 Total**: ~16,389+ lines across 4 waves
**Phase 2 Progress**: ~85% complete (spatial, triggers, procedures, CTEs, subqueries, arrays, text search)

---

**⚠️ IMPORTANT - Code Audit Findings (October 24, 2025)**:
A comprehensive code audit has been completed. See `/docs/audit/` for detailed reports:
- **CRITICAL_DISCREPANCIES_SUMMARY.md** - 3 critical gaps identified (Sprint 4/5, Query Processing)
- **COMPREHENSIVE_CODE_AUDIT_2025-10-24.md** - Full verification with file:line references
- **ALPHA_COMPLETENESS_ASSESSMENT.md** - Alpha NOT READY (~40-45% gap)
- **README.md** in /docs/audit/ - Start here for navigation

**Key Audit Findings**:
- ✅ Core storage engine verified complete (~34,000 lines)
- ❌ Query Processing 0% (claimed 72%) - all 6 files missing
- ❌ Sprint 5 0% (claimed 100%) - MigrationWorker does not exist
- ⚠️ Sprint 4 ~33% (claimed 100%) - Only TIDResolver verified

---

## 2. Known Issues

### Minor Issues
- GIST Index TID Updates not implemented (4-6 hours) - No GIST implementation found
- Known circular dependency in tid_resolver.h (documented, does not block BETA)

---

## 3. Current Priorities

1. ✅ ~~**Phase 6**: Attach/Detach Operations~~ **COMPLETE**
2. ✅ ~~**MGA Catalog Compliance**~~ **COMPLETE**
3. ✅ ~~**Documentation Reorganization**~~ **COMPLETE**
4. 🎯 **Context Variables & Row Identity** (64-94 hours) - IN DESIGN
   - Context Variables Core (20-28 hours) - HIGH PRIORITY
     * Direct context variables (CURRENT_USER, CURRENT_TRANSACTION, etc.)
     * RDB$GET_CONTEXT() and RDB$SET_CONTEXT() functions
     * PSQL Execution Context (CALLING_PROCEDURE, CALL_STACK, etc.)
   - Row Identity: sdb$key (8-10 hours) - HIGH PRIORITY
   - Row Identity: rdb$row_uuid (12-16 hours) - HIGH PRIORITY
   - Transaction Visibility: rdb$xact_id (8-12 hours) - HIGH PRIORITY
   - TRANSFER Command (16-24 hours) - MEDIUM PRIORITY
5. **Phase 7**: Advanced Features (50-66 hours) - NEXT
   - Statistics & Monitoring (12-16 hours) - MUST HAVE
   - Backup/Restore (12-16 hours) - MUST HAVE
   - Compression (12-16 hours) - SHOULD HAVE
   - Encryption (14-18 hours) - SHOULD HAVE

**Total Remaining for ALPHA**: ~114-160 hours (Context Variables + Phase 7)

---

## 4. Architecture Quick Reference

### Core Design Principles
- **Transaction Model**: Firebird MGA (Multi-Generational Architecture)
  - In-place modification with back versions
  - Stable TIDs (indexes never updated unless indexed column changes)
  - Newest-to-Oldest (N2O) version chains
- **MVCC**: Snapshot isolation, 4 isolation levels
- **Storage**: TOAST for large objects, LZ4 compression
- **Concurrency**: Always-in-transaction model, ConnectionContext for thread-local state
- **Memory Safety**: RAII everywhere, smart pointers

### Key Components
```
Storage:       buffer_pool.cpp, page_manager.cpp, heap_page.cpp, toast.cpp
Transactions:  transaction_manager.cpp, clog.cpp, proc_array.cpp
Indexes:       btree.cpp, gin_index.cpp, bitmap_index.cpp, hash_index.cpp, hnsw_index.cpp, brin_index.cpp
Types:         types.cpp, decimal_arithmetic.cpp, jsonb.cpp, xml.cpp
Parser:        lexer.cpp, parser.cpp, ast.cpp, semantic_analyzer.cpp
Executor:      executor.cpp, bytecode_generator.cpp
Catalog:       catalog_manager.cpp
```

### ONLINE Migration Architecture (Sprint 4 & 5)

**State Machine**: INIT → COPYING → CATCH_UP → READY_FOR_SWAP → SWAP → CLEANUP → COMPLETE

**Key Components**:
- **TIDResolver**: Three-tier lookup (Bloom filter → exact mapping → query cache)
  - Bloom filter: ~1-2ns lookup, 1% false positive rate
  - Exact TID mapping: source TID → target TID
  - Query cache: Per-query caching for repeated lookups
- **Dirty Page Bitmap**: Tracks pages modified during COPYING phase
- **Write Routing**: INSERTs → target tablespace, UPDATEs/DELETEs → original location
- **Dual-Source Visibility**: Queries read from both source and target during migration

**Performance Target**: < 5% query overhead during migration

**Files**:
- `include/scratchbird/core/tid_resolver.h` (251 lines)
- `src/core/tid_resolver.cpp` (308 lines)
- `include/scratchbird/core/catalog_manager.h` (migration methods)
- `src/core/catalog_manager.cpp` (execution phases, ~475 lines)

---

## 5. Essential File Locations

### Specifications
```
docs/specifications/TABLESPACE_SPECIFICATION.md           (~1,700 lines)
docs/specifications/MGA_IMPLEMENTATION.md                 (MGA core spec)
docs/specifications/TRANSACTION_MGA_CORE.md               (transaction isolation)
```

### Planning Documents
```
docs/planning/TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md  (comprehensive roadmap)
docs/planning/TABLESPACE_IMPLEMENTATION_PLAN.md              (~1,400 lines)
docs/planning/MVCC_VS_MGA_CODE_REVIEW.md                     (critical bug analysis)
docs/planning/SPRINT7_PHASE7_PREPARATION.md                  (Phase 7 prep)
docs/planning/PHASE7_COMPLETE_SCOPE.md                       (Phase 7 full scope)
docs/planning/ALPHA_CONTEXT_VARIABLES_DESIGN.md              (Context variables v2.0 - Firebird Ch 12 + PSQL context)
docs/planning/ALPHA_CONTEXT_VARIABLES_V2_SUMMARY.md          (v2.0 update summary - NEW)
docs/planning/ALPHA_ROW_IDENTITY_ENHANCED.md                 (Row identity & TRANSFER v2.1)
docs/planning/ALPHA_ROW_IDENTITY_AND_TRANSACTION_VISIBILITY.md  (Original design v1.0)
```

### Status Documents
```
docs/status/TABLESPACE_IMPLEMENTATION_COMPLETE.md        (Phases 1-6 complete summary - NEW)
docs/status/sprints/SPRINT0_MGA_BUG_FIX.md               (Sprint 0 details)
docs/status/sprints/SPRINT4_ONLINE_MIGRATION_INFRASTRUCTURE.md  (Sprint 4 details)
docs/status/sprints/SPRINT5_EXECUTION_ENGINE.md          (Sprint 5 details)
docs/status/sessions/SESSION_SUMMARY_2025_10_23_MGA_CATALOG_COMPLIANCE.md
```

### Audit Documents
```
docs/audit/MGA_COMPLIANCE_REVIEW_TABLESPACE.md           (MGA compliance review)
docs/audit/INDEX_MGA_ALPHA_READINESS_SUMMARY.md          (Index MGA compliance)
```

### Guides
```
docs/guides/PHASE_1_5_MIGRATION_GUIDE.md                 (TID migration guide)
docs/guides/ERROR_HANDLING_GUIDE.md
docs/guides/CONCURRENCY_PATTERNS.md
docs/guides/RESOURCE_MANAGEMENT.md
docs/guides/LOCKING_PROTOCOL.md
```

---

## 6. Coding Standards Summary

### Error Handling
- Return `Status` enum for all operations
- Use `ErrorContext*` for detailed error messages
- `SET_ERROR_CONTEXT()` macro for errors
- No exceptions in core engine

### Memory Management
- RAII everywhere (smart pointers, lock guards)
- `std::unique_ptr` for ownership
- `std::shared_ptr` for shared ownership
- No manual `new`/`delete`

### Concurrency
- Always acquire locks in consistent order (page → metadata)
- Use `std::lock_guard<std::mutex>` for all locks
- ConnectionContext for thread-local state
- No global mutable state

### Logging
```cpp
LOG_DEBUG(Category::STORAGE, "Message %s", var);
LOG_INFO(Category::TRANSACTION, "Message %d", count);
LOG_WARN(Category::CATALOG, "Message");
LOG_ERROR(Category::BUFFER, "Message");
```

---

## 7. Quick Commands

### Build
```bash
cd build && cmake .. && make -j$(nproc)
```

### Run Tests
```bash
cd build && ctest -V
```

### Clang-Tidy
```bash
cd build && run-clang-tidy -p . ../src
```

### Memory Checks
```bash
# TSAN (thread safety)
cd build && cmake -DCMAKE_BUILD_TYPE=TSan .. && make && ./tests/scratchbird_tests

# ASAN (memory errors)
cd build && cmake -DCMAKE_BUILD_TYPE=ASan .. && make && ./tests/scratchbird_tests

# Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./tests/scratchbird_tests
```

---

## 8. Firebird MGA vs PostgreSQL MVCC

**CRITICAL**: ScratchBird uses Firebird MGA, NOT PostgreSQL MVCC.

### Key Differences

| Aspect | Firebird MGA (ScratchBird) | PostgreSQL MVCC |
|--------|----------------------------|-----------------|
| **Update Strategy** | In-place modification | Append-only |
| **Old Versions** | Back versions (delta or full) | Old tuples left in place |
| **Version Chain** | Newest → Oldest (N2O) | Oldest → Newest (O2N) |
| **Index Updates** | Only if indexed column changes | On every UPDATE |
| **TID Stability** | Stable (never changes) | Changes on UPDATE |

### Implementation Impact
- **UPDATE**: Must create back version and modify primary in-place
- **DELETE**: Mark primary as deleted, back versions remain
- **Indexes**: Store stable TIDs pointing to primary location
- **Visibility**: Traverse back chain from primary to find visible version
- **Garbage Collection**: Sweep removes old back versions

---

## 9. Final Reminders

### For AI Assistants

**DO:**
- ✅ Read this file at every session start
- ✅ Re-read after context compaction
- ✅ Check roadmap for current priorities
- ✅ Verify against specifications before implementing
- ✅ Use Firebird MGA model for UPDATE/DELETE operations
- ✅ Maintain stable TIDs (no TID changes on UPDATE)

**DON'T:**
- ❌ Use PostgreSQL MVCC patterns (append-only updates)
- ❌ Update index TIDs unless indexed column changes
- ❌ Create new tuple locations on UPDATE
- ❌ Implement features without checking specs
- ❌ Skip error handling or logging

### Key Facts
- **64-bit XIDs**: No wraparound issues
- **Stable TIDs**: Index entries never change (unless indexed column modified)
- **Back Versions**: UUID-based pointers for distributed support
- **Always-In-Transaction**: Every operation has a transaction
- **Zero Heap Fragmentation**: Firebird MGA design prevents it

---

## 10. Update History

- **2025-10-28**: Phase 1 Task 7 JSON Functions 70% COMPLETE (Infrastructure + nlohmann/json v3.11.3 integration), Task 6 Window Functions 100% COMPLETE
- **2025-10-24**: Context Variables v2.0 COMPLETE (Firebird Ch 12 + PSQL execution context, RDB$GET/SET_CONTEXT), Row Identity v2.1
- **2025-10-23**: Documentation reorganization COMPLETE, Phase 6 COMPLETE, MGA catalog compliance COMPLETE
- **2025-10-21**: Sprint 4 & 5 COMPLETE (ONLINE migration infrastructure + execution)
- **2025-10-20**: Phase 1.5 COMPLETE (TID Migration to GPID format)
- **2025-10-17**: All 21 Alpha issues resolved
- **2025-10-16**: MGA Phases 1-4 complete
- **2025-10-14**: Phase 1 audit complete (23 critical issues resolved)
