# ScratchBird Project Context

**Last Updated**: 2025-10-21 (Sprint 4 & 5: ONLINE Migration Complete)
**Version**: Alpha 1.0.3 (Tablespace Infrastructure + ONLINE Migration)
**Status**: Educational/Development (Production-Ready Core)

> **PURPOSE**: This file provides essential context for AI assistants working on ScratchBird.
> Read this file at session start and after every context compaction.

---

## 1. Current Project State

### Version & Status
- **Current Version**: Alpha 1.0.3
- **Production Ready**: ❌ NO - Educational/Development only
- **Last Major Update**: October 21, 2025
- **Active Development**: Tablespace implementation (Sprint 4 & 5 COMPLETE)

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
Query Processing:    72% complete  (lexer, parser, AST, semantic, bytecode, executor)
Catalog:             75% complete  (metadata persistence)
Code Quality:        98% complete  (RAII, logging, const-correct)
CI/CD:               100% complete (TSAN, ASAN, Helgrind, Valgrind, Clang-Tidy)
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

**Phase 3**: ✅ PARTIAL - Autoextend (4.5 hours)
- Task 3.2: Preallocation COMPLETE
- Task 3.1: Autoextend (12-18 hours) - REMAINING

**Phase 4**: ✅ COMPLETE - Migration Infrastructure (9.5 hours)
- Parser, catalog manager infrastructure
- Batch processing, index TID update, executor

**Phase 5**: ✅ COMPLETE - OFFLINE Migration (~32-41 hours)
- All heap page migration complete
- All 5 index types migrated (B-Tree, Hash, HNSW, GIN, BRIN)
- Full TOAST migration complete

**Sprint 4**: ✅ COMPLETE - ONLINE Migration Infrastructure (9.5 hours)
- Task 5.4.1: Migration State Management
- Task 5.4.2: Dual-Source Visibility (TIDResolver with Bloom filters)
- Task 5.4.3: Write Routing During Migration

**Sprint 5**: ✅ COMPLETE - ONLINE Migration Execution (4 hours)
- Task 5.4.4: Copying Phase
- Task 5.4.5: Catch-Up Phase (iterative dirty page re-copy)
- Task 5.4.6: Atomic Swap Phase (index updates + catalog swap)

**Total Completed**: ~162-185 hours

---

## 2. Known Issues

### Critical Bug (BLOCKS ALPHA)
**Cross-Page UPDATE uses MVCC instead of MGA** (2-4 hours - **CRITICAL**)
- **Location**: `src/core/storage_engine.cpp:729-762`
- **Problem**: Creates NEW tuple at NEW location (PostgreSQL MVCC) instead of creating BACK version and modifying PRIMARY in-place (Firebird MGA)
- **Impact**: Index TIDs become invalid, version chains break
- **Priority**: 0 (MUST FIX FIRST)
- **Documentation**: `docs/planning/MVCC_VS_MGA_CODE_REVIEW.md`

### Minor Issues
- GIST Index TID Updates not implemented (4-6 hours) - No GIST implementation found
- Known circular dependency in tid_resolver.h (documented, does not block BETA)

---

## 3. Current Priorities

1. **FIX CRITICAL BUG**: Cross-page UPDATE MGA compliance (2-4 hours)
2. **Complete Phase 3**: Autoextend implementation (12-18 hours)
3. **Phase 6**: Attach/Detach Operations (20-30 hours)
4. **Phase 7**: Advanced Features (50-66 hours)

**Total Remaining for ALPHA**: ~82-124 hours

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
```

### Status Documents
```
docs/STATUS_SPRINT4_ONLINE_MIGRATION_INFRASTRUCTURE.md   (Sprint 4 details)
docs/STATUS_SPRINT5_EXECUTION_ENGINE.md                  (Sprint 5 details)
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

- **2025-10-21**: Sprint 4 & 5 COMPLETE (ONLINE migration infrastructure + execution)
- **2025-10-20**: Phase 1.5 COMPLETE (TID Migration to GPID format)
- **2025-10-17**: All 21 Alpha issues resolved
- **2025-10-16**: MGA Phases 1-4 complete
- **2025-10-14**: Phase 1 audit complete (23 critical issues resolved)
