# ScratchBird Project Context

**Last Updated**: 2025-10-14
**Version**: Alpha 1.2
**Status**: Educational/Development (NOT Production Ready)

> **PURPOSE**: This file provides essential context for AI assistants working on ScratchBird.
> Read this file at session start and after every context compaction.
> Update this file when major milestones are completed or priorities change.

---

## 1. Current Project State

### Version & Status
- **Current Version**: Alpha 1.2
- **Production Ready**: ❌ NO - Educational/Development only
- **Last Major Update**: October 14, 2025
- **Active Development Phase**: ALPHA-003 (GIN Index) - Phase 6 Complete

### Critical Statistics
```yaml
Storage Engine:      95% complete  (buffer pool, pages, TOAST, compression)
Transaction System:  98% complete  (Firebird-style MGA, 4 isolation levels, sweep)
Concurrency:         95% complete  (ConnectionContext, locking, deadlock detection)
Indexing:
  - B-tree:          100% complete (2,256 lines)
  - Hash:            100% complete (2,254 lines)
  - GIN:             98% complete  (Phase 6 done)
  - Bitmap:          100% complete (Roaring compression)
Type System:         95% complete  (30+ types, UUIDv7, timezones, collations)
Query Processing:    72% complete  (lexer, parser, AST, semantic, bytecode, executor)
Catalog:             75% complete  (metadata persistence)
Code Quality:        90% complete  (RAII, logging, const-correct)
```

---

## 2. Known Critical Issues

**From Comprehensive Audit (Oct 14, 2025)**: 23 Critical, 41 Major, 62 Minor issues identified.

### Top 7 Critical Issues (Must Fix Before Beta)

1. ~~**CRC32C Implementation Incorrect**~~ ✅ VERIFIED CORRECT (`src/core/crc32c.cpp:26-34`, `ondisk.h:70-77`)
   - ~~Doesn't exclude checksum field from calculation per spec~~
   - **AUDIT ERROR**: Implementation was already correct, verified with comprehensive tests
   - Status: COMPLETE (2025-10-14) - Added test suite `/tests/unit/test_crc32c_comprehensive.cpp`

2. ~~**Missing Atomic XID Allocation**~~ ✅ FIXED (`src/core/transaction_manager.cpp:257`)
   - ~~Race condition: multiple transactions can get same XID~~
   - **FIXED**: Changed to `std::atomic<uint64_t>` with `fetch_add(1, memory_order_seq_cst)`
   - Status: COMPLETE (2025-10-14) - Verified with concurrency tests

3. **Buffer Pool LRU Race Condition** (`src/core/buffer_pool.cpp:450-457`)
   - `updateLru()` modifies shared data without lock
   - Impact: LRU list corruption, crashes in evictPage()

4. **Heap Page Memory Leak** (`src/core/heap_page.cpp:624-835`)
   - `findVisibleVersion()` doesn't unpin pages on error paths
   - Impact: Buffer pool exhaustion

5. **Missing fsync After Commits** (`src/core/transaction_manager.cpp:96`)
   - Committed transactions can be lost on crash
   - Impact: ACID durability violated

6. **Integer Overflow** (`src/core/page_manager.cpp:245-249`)
   - Bitmap extension calculates before checking overflow
   - Impact: Undefined behavior, heap corruption

7. **Tuple Size Validation Missing** (`src/core/heap_page.cpp:109-236`)
   - No maximum size check in `insertTuple()`
   - Impact: Buffer overflow, heap corruption

**Full Details**: `/docs/audit/COMPREHENSIVE_AUDIT_REPORT.md`

---

## 3. Current Priorities

### Immediate Tasks (This Week)
1. ✅ Complete comprehensive audit → DONE (Oct 14)
2. ✅ Fix critical issue #1: CRC32C implementation → VERIFIED CORRECT (Oct 14)
3. ✅ Fix critical issue #2: Atomic XID allocation → COMPLETE (Oct 14)
4. 🔄 Fix critical issue #3: Buffer Pool LRU Race Condition → NEXT
5. 🔄 Continue critical issue fixes (2/23 complete, 21 remaining)

### Next Milestone: Alpha 1.3 (Target: End of October)
- Fix all 23 CRITICAL issues from audit
- Complete remaining type system components (JSONB, XML, VECTOR)
- Add comprehensive test coverage for transaction manager
- Implement hint bits optimization

### Beta Requirements (Target: Q1 2026)
- ❌ Write-Ahead Logging (WAL) - crash recovery
- ❌ Network layer - multi-client support
- ❌ Query optimizer foundations
- ❌ Advanced SQL (JOINs, subqueries)
- ✅ All critical issues resolved

---

## 4. Recent Completed Work

### Last 7 Days
- **Oct 14**: Atomic XID allocation fixed with `std::atomic<uint64_t>` (Issue 1.2/23) ✅
- **Oct 14**: CRC32C checksum implementation verified correct (Issue 1.1/23) ✅
- **Oct 14**: Comprehensive audit report (126 issues documented)
- **Oct 13**: ALPHA-003 GIN Index Phase 6 complete (advanced performance features)
- **Oct 13**: ALPHA-002 Bitmap Index complete
- **Oct 12**: ALPHA-001 Type system extensions complete
- **Oct 12**: Phase 4 critical issues resolved (deadlock, RAII, logging)
- **Oct 11**: Phase 3 Firebird transaction model complete
- **Oct 7**: Phase 2 ConnectionContext & always-in-transaction complete

### Last 30 Days
- ✅ Multi-connection support with thread-local storage
- ✅ All four isolation levels (READ UNCOMMITTED through SERIALIZABLE)
- ✅ Sweep mechanism for garbage collection
- ✅ Long transaction monitoring with action policies
- ✅ Deadlock detection with cycle detection & victim selection
- ✅ Memory safety improvements (RAII throughout lock manager)
- ✅ Logging framework integrated (Logger with categories)
- ✅ Code formatting standardized (83 files with clang-format)

---

## 5. Architecture Quick Reference

### Core Design Principles
- **Transaction Model**: Firebird-style MGA (Multi-Generational Architecture)
- **MVCC**: Snapshot isolation, 4 isolation levels
- **Storage**: TOAST for large objects, LZ4 compression
- **Concurrency**: Always-in-transaction model, ConnectionContext for thread-local state
- **Memory Safety**: RAII everywhere, smart pointers, no manual memory management
- **Error Handling**: Status enum + ErrorContext for detailed errors

### Key Components
```
Storage:       buffer_pool.cpp, page_manager.cpp, heap_page.cpp, toast.cpp
Transactions:  transaction_manager.cpp, clog.cpp, proc_array.cpp
Indexes:       btree.cpp, gin_index.cpp, bitmap_index.cpp, hash_index.cpp
Types:         types.cpp, decimal_arithmetic.cpp, jsonb.cpp, xml.cpp
Parser:        lexer.cpp, parser.cpp, ast.cpp, semantic_analyzer.cpp
Executor:      bytecode_generator.cpp, executor.cpp
```

### Transaction Markers (Firebird-style)
- **OIT** (Oldest Interesting Transaction): Oldest uncommitted transaction
- **OAT** (Oldest Active Transaction): Oldest running transaction
- **OST** (Oldest Snapshot Transaction): Oldest snapshot in use
- **NEXT**: Next transaction ID to allocate

### Page Format
- Page sizes: 4KB, 8KB (default), 16KB, 32KB
- 64-byte PageHeader with CRC32C checksum
- 8-byte alignment required for on-disk structures
- TOAST threshold: varies by page size

---

## 6. Essential File Locations

### Always Read These Files
1. **`/README.md`** - Project overview, quick start, current status
2. **`/docs/development/TODO.md`** - Prioritized work items (50KB, critical)
3. **`/docs/status/CURRENT_STATUS.md`** - Detailed implementation status
4. **`/docs/audit/COMPREHENSIVE_AUDIT_REPORT.md`** - All known issues (126 total)

### Before Working On Component
- **Storage**: `/docs/specifications/STORAGE_ENGINE_*.md`, `ON_DISK_FORMAT.md`
- **Transactions**: `/docs/specifications/TRANSACTION_*.md`, `MGA_IMPLEMENTATION.md`
- **Indexes**: `/docs/specifications/LOW_LEVEL_SPECIFICATION_*_INDEX.md`
- **Types**: `/docs/specifications/03_TYPES_AND_DOMAINS.md`
- **SQL**: `/docs/specifications/SCRATCHBIRD_SQL_COMPLETE_BNF.md`

### Planning & Status
- **Plans**: `/docs/planning/ALPHA_*_IMPLEMENTATION_PLAN.md`
- **Status**: `/docs/status/ALPHA_*_COMPLETE.md` or `*_PROGRESS.md`
- **Index**: `/docs/INDEX.md` - navigation hub for all documentation

---

## 7. Coding Standards Summary

### Non-Negotiable Rules
1. **RAII Everywhere** - No manual memory management
2. **Const Correctness** - Mark methods const when they don't modify state
3. **Smart Pointers** - Use `std::unique_ptr`, `std::shared_ptr`
4. **Error Handling** - Return `Status`, use `ErrorContext` for details
5. **Thread Safety** - Use atomic operations for shared state
6. **Lock Documentation** - Document which locks methods require
7. **Test Everything** - Unit tests for all new code

### Naming Conventions
- Classes: `PascalCase` (BufferPool)
- Functions: `camelCase` (pinPage)
- Variables: `snake_case` (frame_index)
- Constants: `UPPER_SNAKE_CASE` (MAX_PAGE_SIZE)
- Private members: trailing underscore (mutex_)

### Before Committing
- Run clang-format on changed files
- Run relevant tests: `ctest --output-on-failure`
- Check for memory leaks in debug builds
- Verify no new compiler warnings

**Full Standards**: `/docs/development/CODING_STANDARDS.md`

---

## 8. Common Pitfalls (From Audit)

### Critical Mistakes to Avoid
1. ❌ **Don't trust comments** - Verify implementation matches specification
2. ❌ **Check for race conditions** - This is a multi-threaded database
3. ❌ **Validate all inputs** - Especially tuple sizes, page numbers, array indices
4. ❌ **Handle error paths** - Clean up resources on ALL error returns (use RAII)
5. ❌ **Use atomic operations** - For shared counters like XID allocation, statistics
6. ❌ **fsync for durability** - Don't assume OS will flush to disk
7. ❌ **Lock ordering** - Maintain consistent order to prevent deadlocks
8. ❌ **Alignment requirements** - 8-byte alignment for on-disk structures

### Code Review Checklist
- [ ] Memory leaks checked (RAII used?)
- [ ] Race conditions considered (atomic ops? locks held?)
- [ ] Error paths handle cleanup (RAII guards? unpin pages?)
- [ ] Input validation (bounds checks? null checks?)
- [ ] Spec compliance (matches documentation?)
- [ ] Tests written (unit + integration?)
- [ ] Logging added (appropriate level and category?)
- [ ] Comments accurate (reflect actual behavior?)

---

## 9. Quick Commands

### Build
```bash
mkdir build && cd build
cmake .. && make
```

### Test
```bash
# All tests
ctest --output-on-failure

# Specific test
ctest -R "test_name"

# With verbose output
ctest -V -R "test_name"
```

### Debug
```bash
# Memory leak detection
valgrind --leak-check=full ./build/test_name

# Thread safety
valgrind --tool=helgrind ./build/test_name
```

### Format Code
```bash
find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

---

## 10. When Context Is Lost (Post-Compaction Recovery)

### Immediately Re-Read
1. This file (`/PROJECT_CONTEXT.md`)
2. `/docs/development/TODO.md` - See what's prioritized
3. `/docs/audit/COMPREHENSIVE_AUDIT_REPORT.md` - Know the critical issues

### Ask User
- "What were we working on before compaction?"
- "Are we still on the same task, or should we switch?"

### Then Load
- Relevant specification for the component being worked on
- Recent status file for that component
- Implementation plan if starting new work

### Verify Understanding
Before resuming work, confirm:
- Which file(s) are we modifying?
- What is the goal of the current task?
- Are there any related critical issues from the audit?
- Do we need to write tests?

---

## 11. Project Milestones

### ✅ Completed Phases
- **Phase 1**: Storage engine foundations (Alpha 1.0)
- **Phase 2**: ConnectionContext & always-in-transaction (Oct 7)
- **Phase 3**: Firebird transaction model (Oct 7-11)
- **Phase 4**: Critical issues resolution (Oct 12)
- **ALPHA-001**: Type system extensions (Oct 12)
- **ALPHA-002**: Bitmap index (Oct 13)
- **ALPHA-003**: GIN index Phases 1-6 (Oct 13)

### 🔄 In Progress
- **Critical Issues**: Fixing 23 critical bugs from audit
- **Type System**: Complete JSONB, XML, VECTOR implementations
- **Query Optimizer**: Foundations

### ⏳ Upcoming
- **Alpha 1.3**: All critical issues resolved (end of October)
- **Alpha 1.4**: Query optimizer, advanced SQL features
- **Beta 1.0**: WAL, network layer, crash recovery (Q1 2026)
- **Release 1.0**: Production-ready (Q2-Q3 2026)

---

## 12. Key Metrics

### Code Size
- **Total Source Lines**: ~15,000+ (54 .cpp files)
- **Largest Components**:
  - transaction_manager.cpp: ~1,200 lines
  - btree.cpp: ~1,600 lines
  - heap_page.cpp: ~1,000 lines
  - gin_index.cpp: ~800 lines

### Test Coverage
- **Unit Tests**: ~30 tests (need more)
- **Integration Tests**: ~10 tests (need more)
- **Coverage Target**: 80%+ (current ~40%)

### Documentation
- **Total Doc Files**: 96 specification files + ~50 other docs
- **Key Docs**: 4 (this file, TODO, CURRENT_STATUS, audit report)
- **Documentation Quality**: High (detailed specs for all components)

---

## 13. Important Configuration

### Build Configuration
- **CMake Version**: 3.10+
- **C++ Standard**: C++17
- **Compiler**: GCC 7+ or Clang 7+
- **Build Type**: Debug (default) or Release

### Database Configuration
Located in `src/core/config.h`:
```cpp
DEFAULT_PAGE_SIZE = 8192          // 8KB pages
DEFAULT_BUFFER_POOL_SIZE = 1000   // 1000 pages (8MB)
MAX_VERSION_CHAIN_LENGTH = 1000   // Version chain limit
MAX_CONNECTIONS = 100             // Concurrent connections
TOAST_THRESHOLD = 1024            // 1KB threshold for TOAST
```

### Logging Configuration
Categories: STORAGE, TRANSACTION, INDEX, PARSER, EXECUTOR, CATALOG, LOCK
Levels: DEBUG, INFO, WARNING, ERROR, FATAL

---

## 14. Emergency Reference

### Build Broken?
1. `rm -rf build && mkdir build && cd build && cmake .. && make`
2. Check `/docs/development/BUILD_FIX_TODO_LIST.md`
3. Verify all dependencies installed

### Tests Failing?
1. `ctest --output-on-failure` for details
2. Check recent git commits for changes
3. Review test expectations vs actual behavior

### Segfault / Crash?
1. Build in debug: `cmake -DCMAKE_BUILD_TYPE=Debug ..`
2. Run with gdb: `gdb --args ./test_name`
3. Check for null pointer dereferences
4. Verify buffer pool pins/unpins are balanced

### Deadlock?
1. Check lock ordering in code
2. Review `/docs/specifications/TRANSACTION_LOCK_MANAGER.md`
3. Enable deadlock detection logging
4. Verify consistent lock acquisition order

### Memory Leak?
1. `valgrind --leak-check=full ./test_name`
2. Check for missing RAII guards
3. Verify all pinned pages are unpinned
4. Check error paths for cleanup

---

## 15. Contact & Resources

### External Documentation
- PostgreSQL internals (MVCC reference)
- Firebird architecture (transaction model reference)
- SQLite source code (simple reference implementation)

### Specification Compliance
ScratchBird aims to be compatible with:
- Firebird transaction semantics
- PostgreSQL MVCC visibility rules
- Standard SQL (where practical)

### Known Limitations
- No WAL (crash recovery incomplete)
- No network protocol (local only)
- Limited SQL (no JOINs, subqueries yet)
- Single database file only
- No user authentication
- No query optimizer
- No replication

---

## 16. Update History

| Date | Version | Major Changes |
|------|---------|---------------|
| 2025-10-14 | 1.0 | Initial creation with comprehensive audit results |

---

## 17. Quick Status Check

**To quickly assess project status, check these metrics:**

```bash
# Files modified today
git log --since="1 day ago" --name-only --pretty=format: | sort -u

# Recent commits
git log --oneline -10

# Current branch status
git status

# Test pass rate
cd build && ctest 2>&1 | grep "tests passed"
```

**Expected outputs as of Oct 14, 2025:**
- Most recent work: Comprehensive audit
- Tests passing: ~30/40 (75%)
- Active branch: main
- Known failures: Some integration tests need updates for recent changes

---

## 18. Final Reminders

### For AI Assistants

**DO:**
- ✅ Read this file at every session start
- ✅ Re-read after context compaction
- ✅ Check TODO.md for current priorities
- ✅ Verify against specifications before implementing
- ✅ Write tests for new code
- ✅ Use RAII for all resource management
- ✅ Handle all error paths properly
- ✅ Ask user to clarify if uncertain

**DON'T:**
- ❌ Trust comments over implementation
- ❌ Skip input validation
- ❌ Forget to clean up on error paths
- ❌ Use manual memory management
- ❌ Assume thread safety without verification
- ❌ Commit code without running tests
- ❌ Ignore compiler warnings
- ❌ Make changes without understanding context

### For Humans

This file is your **single source of truth** for project status. Update it when:
- Major milestones are completed
- Priorities change
- Critical issues are discovered or resolved
- Architecture decisions are made
- Build/test infrastructure changes

Keep it concise, current, and comprehensive.

---

**END OF PROJECT CONTEXT**

*Next update due: When critical issues begin being fixed, or by October 21, 2025*
