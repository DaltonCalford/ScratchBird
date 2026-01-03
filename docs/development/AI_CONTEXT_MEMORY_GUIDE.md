# AI Context Memory Guide for ScratchBird

**Purpose**: This document defines what directories and files an AI assistant should keep in memory between context compactions to maintain effective continuity when working on the ScratchBird database engine.

**Last Updated**: October 14, 2025

---

## 1. Critical Context Files (Always Load First)

These files provide the foundational understanding of the project:

### 1.1 Project Overview
- **`/README.md`** - Current status, version, recent achievements, known limitations
- **`/docs/INDEX.md`** - Navigation hub for all documentation

### 1.2 Current State Tracking
- **`/docs/status/CURRENT_STATUS.md`** - Most recent implementation status (Oct 12, 2025)
- **`/docs/status/ALPHA_003_PROGRESS.md`** - Active Alpha 003 (GIN Index) progress
- **`/docs/development/TODO.md`** - Prioritized work items and blockers (50KB, updated Oct 13)

### 1.3 Recent Audit Reports
- **`/docs/audit/COMPREHENSIVE_AUDIT_REPORT.md`** - Full codebase audit (Oct 14, 2025)
  - 23 critical issues identified
  - 41 major issues
  - 62 minor issues
  - Production readiness assessment

### 1.4 Implementation Plans
- **`/docs/planning/ALPHA_1_2_IMPLEMENTATION_PLAN.md`** - Current roadmap (Oct 11, 2025)
- **`/docs/planning/ALPHA_003_IMPLEMENTATION_PLAN.md`** - Active GIN index work

---

## 2. Code Organization (Directory Structure)

### 2.1 Source Code Layout
```
/src/
├── core/           # Storage engine, transactions, indexes (44 .cpp files)
│   ├── Storage: buffer_pool.cpp, page_manager.cpp, heap_page.cpp
│   ├── Transactions: transaction_manager.cpp, clog.cpp, proc_array.cpp
│   ├── Indexes: btree.cpp, gin_index.cpp, bitmap_index.cpp, hash_index.cpp
│   ├── Types: types.cpp, decimal_arithmetic.cpp, jsonb.cpp, xml.cpp
│   └── TOAST: toast.cpp, compressed_page_manager.cpp
├── parser/         # SQL parser (7 .cpp files)
│   ├── lexer.cpp, parser.cpp, ast.cpp, semantic_analyzer.cpp
├── sblr/           # Query executor (2 .cpp files)
│   ├── bytecode_generator_v2.cpp, executor.cpp
└── main.cpp        # Entry point
```

### 2.2 Documentation Structure
```
/docs/
├── status/         # What's implemented (completion reports)
├── planning/       # What to implement next (roadmaps)
├── development/    # How to develop (TODO, standards, analysis)
├── audit/          # Code quality audits
├── specifications/ # Technical specs (96+ files)
├── design/         # Architecture decisions
└── archive/        # Historical documents
```

### 2.3 Test Structure
```
/tests/
├── unit/           # Unit tests (test_connection_context.cpp, etc.)
├── integration/    # Integration tests
└── scratchbird_tests.cpp  # Main test suite
```

---

## 3. Key Specification Documents

When working on specific components, load these specs:

### 3.1 Storage Layer
- **`/docs/specifications/ON_DISK_FORMAT.md`** - Page format, checksums, alignment
- **`/docs/specifications/STORAGE_ENGINE_BUFFER_POOL.md`** - Buffer pool management
- **`/docs/specifications/STORAGE_ENGINE_PAGE_MANAGEMENT.md`** - Page allocation
- **`/docs/specifications/HEAP_TOAST_INTEGRATION.md`** - Large object storage

### 3.2 Transaction Management
- **`/docs/specifications/MGA_IMPLEMENTATION.md`** - Multi-generational architecture
- **`/docs/specifications/TRANSACTION_MGA_CORE.md`** - MVCC core
- **`/docs/specifications/TRANSACTION_LOCK_MANAGER.md`** - Locking
- **`/docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`** - Firebird-style transactions

### 3.3 Indexing Systems
- **`/docs/specifications/LOW_LEVEL_SPECIFICATION_B-TREE_INDEX.md`** - B-tree (completed)
- **`/docs/specifications/LOW_LEVEL_SPECIFICATION_GIN_INDEX.md`** - GIN index (in progress)
- **`/docs/specifications/LOW_LEVEL_SPECIFICATION_BITMAP_INDEX.md`** - Bitmap index (completed)
- **`/docs/specifications/LOW_LEVEL_SPECIFICATION_HASH_INDEX.md`** - Hash index (completed)

### 3.4 Type System
- **`/docs/specifications/03_TYPES_AND_DOMAINS.md`** - Type system overview
- **`/docs/specifications/DDL_DOMAINS.md`** - Domain types

### 3.5 Query Processing
- **`/docs/specifications/SCRATCHBIRD_SQL_COMPLETE_BNF.md`** - SQL grammar
- **`/docs/specifications/Appendix_A_SBLR_BYTECODE.md`** - Bytecode format

---

## 4. Critical Knowledge to Retain

### 4.1 Version Information
```yaml
Current Version: Alpha 1.2
Status: Educational/Development (NOT Production Ready)
Last Major Update: October 12, 2025
Latest Work: GIN Index Phase 6 (Oct 13, 2025)
```

### 4.2 Architecture Decisions
- **Transaction Model**: Firebird-style MGA (Multi-Generational Architecture)
- **MVCC**: Snapshot isolation with 4 isolation levels
- **Storage**: TOAST for large objects, LZ4 compression
- **Indexes**: B-tree (complete), Hash (complete), GIN (in progress), Bitmap (complete)
- **Concurrency**: Always-in-transaction model, ConnectionContext for thread-local state

### 4.3 Known Critical Issues (from audit)
1. CRC32C checksum implementation incorrect (crc32c.cpp:26-34)
2. Missing atomic XID allocation (transaction_manager.cpp:257)
3. Buffer pool LRU race condition (buffer_pool.cpp:450-457)
4. Heap page version chain memory leak (heap_page.cpp:624-835)
5. Missing fsync after critical writes (transaction_manager.cpp:96)
6. Integer overflow in page manager (page_manager.cpp:245-249)
7. Tuple size validation missing (heap_page.cpp:109-236)

### 4.4 Current Development Focus
**Active**: ALPHA-003 GIN Index Implementation (Phase 6 complete)
**Next**: Complete remaining type system components (ALPHA-001)
**Blockers**: None currently (all critical issues documented)

### 4.5 Completion Status
```yaml
Storage Engine: 95%
Transaction Management: 98%
Concurrency: 95%
Indexing:
  - B-tree: 100% (2,256 lines)
  - Hash: 100% (2,254 lines)
  - GIN: 98% (Phase 6 complete)
  - Bitmap: 100%
Type System: 95%
Query Processing: 72%
Catalog: 75%
Code Quality: 90%
```

---

## 5. Context Loading Strategy

### 5.1 On Session Start (Cold Start)
Load in this order:
1. `/README.md` - Get oriented
2. `/docs/status/CURRENT_STATUS.md` - Understand current state
3. `/docs/development/TODO.md` - Know what's prioritized
4. `/docs/audit/COMPREHENSIVE_AUDIT_REPORT.md` - Understand issues
5. `/docs/INDEX.md` - Know where to find things

### 5.2 Before Major Work
If working on specific component, also load:
- Relevant specification from `/docs/specifications/`
- Relevant status report from `/docs/status/`
- Relevant plan from `/docs/planning/`

### 5.3 After Context Compaction
Reload these minimal files:
- `/README.md` (project overview)
- `/docs/status/CURRENT_STATUS.md` (what's done)
- `/docs/development/TODO.md` (what's next)
- `/docs/audit/COMPREHENSIVE_AUDIT_REPORT.md` (what's broken)

---

## 6. File Prioritization by Recency

### Recently Updated (Last 7 Days)
```
Oct 14: /docs/audit/COMPREHENSIVE_AUDIT_REPORT.md (NEW)
Oct 13: /docs/development/TODO.md
Oct 13: /docs/status/ALPHA_003_PROGRESS.md
Oct 13: /docs/status/ALPHA_003_GIN_PHASE_6_COMPLETE.md
Oct 12: /docs/status/CURRENT_STATUS.md
Oct 12: /docs/development/CODING_STANDARDS.md
Oct 11: /docs/planning/ALPHA_1_2_IMPLEMENTATION_PLAN.md
```

### Core Stable Documents (Updated Infrequently)
- Specification files (mostly complete)
- Design documents (architectural decisions)
- Archive materials (historical reference)

---

## 7. Quick Reference Queries

### "What's the current status?"
→ Read `/docs/status/CURRENT_STATUS.md`

### "What should I work on next?"
→ Read `/docs/development/TODO.md` (prioritized by CRITICAL/HIGH/MEDIUM)

### "What are the known bugs?"
→ Read `/docs/audit/COMPREHENSIVE_AUDIT_REPORT.md` (23 critical, 41 major, 62 minor)

### "How do I implement X?"
→ Check `/docs/specifications/` for relevant spec
→ Check `/docs/design/` for design decisions

### "What's been completed?"
→ Check `/docs/status/` for completion reports

### "What's the plan for feature Y?"
→ Check `/docs/planning/` for implementation plans

---

## 8. Important Constants and Configuration

### Design Limits
- Page size: 4KB, 8KB, 16KB, 32KB (configurable)
- Default page size: 8192 bytes
- Max tuple size: Page size - headers
- Transaction ID: 64-bit unsigned integer
- Max connections: Configurable via ConnectionContext

### File Locations
- Database files: `*.sdb` (ScratchBird Database)
- Configuration: `src/core/config.h`
- Error codes: Check `Status` enum in code

### Build System
```bash
mkdir build && cd build
cmake .. && make
ctest --output-on-failure
```

---

## 9. Coding Standards Summary

From `/docs/development/CODING_STANDARDS.md`:

### Key Rules
1. **RAII Everywhere** - No manual memory management
2. **Const Correctness** - Mark methods const when appropriate
3. **Smart Pointers** - Use `std::unique_ptr`, `std::shared_ptr`
4. **Error Handling** - Use `Status` enum, `ErrorContext` for details
5. **Logging** - Use `Logger` with categories (STORAGE, TRANSACTION, INDEX, etc.)
6. **Locking** - Document lock requirements, use RAII guards
7. **Thread Safety** - Use atomic operations for shared state
8. **Testing** - Write tests for all new code

### Naming Conventions
- Classes: `PascalCase` (e.g., `BufferPool`)
- Functions: `camelCase` (e.g., `pinPage`)
- Variables: `snake_case` (e.g., `frame_index`)
- Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_PAGE_SIZE`)
- Private members: trailing underscore (e.g., `mutex_`)

---

## 10. AI Assistant Workflow

### Starting a Session
1. Load critical context files (Section 5.1)
2. Ask user: "What would you like to work on?"
3. Load relevant specifications for that component
4. Check TODO.md for any related tasks
5. Check audit report for any related issues

### During Development
1. **Before writing code**: Check spec compliance
2. **During coding**: Follow coding standards
3. **After coding**: Consider test coverage
4. **Before commit**: Run clang-format (if available)

### Context Compaction Recovery
If context is compacted mid-session:
1. Reload Section 5.3 files (minimal set)
2. Ask user to remind you what was being worked on
3. Reload relevant specifications for that work
4. Continue from where you left off

### Handoff Between Sessions
Document in session notes:
- What was completed
- What's in progress
- What's blocked
- Next steps

---

## 11. Common Pitfalls to Remember

### From Audit Report
1. **Don't trust comments** - Verify implementation matches spec
2. **Check for race conditions** - Multi-threaded database
3. **Validate all inputs** - Especially tuple sizes, page numbers
4. **Handle error paths** - Clean up resources on failure
5. **Use atomic operations** - For shared counters (XID, stats)
6. **fsync for durability** - Don't assume OS will flush
7. **Lock ordering** - Prevent deadlocks
8. **Alignment requirements** - 8-byte alignment for on-disk structures

### Code Review Checklist
- [ ] Memory leaks checked (RAII used?)
- [ ] Race conditions considered (atomic ops, locks?)
- [ ] Error paths handle cleanup (RAII guards?)
- [ ] Input validation (bounds, null checks?)
- [ ] Spec compliance (matches documentation?)
- [ ] Tests written (unit + integration?)
- [ ] Logging added (appropriate level?)
- [ ] Comments accurate (reflects actual code?)

---

## 12. Project Milestones Reference

### Completed (Alpha 1.2)
- ✅ Phase 2: ConnectionContext & Always-In-Transaction (Oct 7)
- ✅ Phase 3: Firebird Transaction Model (Oct 7-11)
- ✅ Phase 4: Critical Issues Resolution (Oct 12)
- ✅ ALPHA-001: Type System Extensions (Oct 12)
- ✅ ALPHA-002: Bitmap Index (Oct 13)
- ✅ ALPHA-003: GIN Index Phases 1-6 (Oct 13)

### In Progress
- 🔄 Type system completion (JSONB, XML, VECTOR remaining)
- 🔄 Query optimizer foundations

### Upcoming (Beta Requirements)
- ⏳ Write-Ahead Logging (WAL)
- ⏳ Network layer (multi-client support)
- ⏳ Crash recovery
- ⏳ Advanced SQL features (JOINs, subqueries)

---

## 13. Emergency Reference

### If Build Breaks
1. Check `/docs/development/BUILD_INSTRUCTIONS.md`
2. Check `/docs/development/BUILD_FIX_TODO_LIST.md`
3. Run: `rm -rf build && mkdir build && cd build && cmake .. && make`

### If Tests Fail
1. Run: `ctest --output-on-failure` for details
2. Check recent changes in git history
3. Review test expectations in test files

### If Memory Issues
1. Run with Valgrind: `valgrind --leak-check=full ./test_name`
2. Check for missing RAII guards
3. Review error paths for cleanup

### If Deadlock Occurs
1. Check lock ordering in code
2. Review `/docs/specifications/TRANSACTION_LOCK_MANAGER.md`
3. Ensure consistent lock acquisition order

---

## 14. Summary: Minimum Context Set

**For any session, at minimum keep these in memory:**

1. **Status**: What's done (`/docs/status/CURRENT_STATUS.md`)
2. **TODO**: What's next (`/docs/development/TODO.md`)
3. **Issues**: What's broken (`/docs/audit/COMPREHENSIVE_AUDIT_REPORT.md`)
4. **Structure**: Where things are (`/docs/INDEX.md`)

**Total minimum context: ~100KB across 4 files**

This provides enough context to:
- Understand current state
- Know what to work on
- Avoid known issues
- Navigate to detailed specs when needed

---

## 15. Version History

| Date | Version | Changes |
|------|---------|---------|
| 2025-10-14 | 1.0 | Initial creation based on comprehensive audit |

---

**End of AI Context Memory Guide**
