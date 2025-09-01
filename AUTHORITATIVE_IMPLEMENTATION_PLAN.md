# ScratchBird Authoritative Implementation Plan
## THIS DOCUMENT SUPERSEDES ALL OTHER PLANNING DOCUMENTS

### Document Authority
- **Version**: 1.0.0
- **Created**: 2024-09-01
- **Status**: AUTHORITATIVE - This is the SINGLE SOURCE OF TRUTH
- **Supersedes**: All other phase plans, implementation plans, and roadmaps

### ⚠️ CRITICAL NOTICE
Any implementation MUST follow this plan. All other planning documents are DEPRECATED:
- ~~PHASED_IMPLEMENTATION_PLAN.md~~ - OBSOLETE (60 phases)
- ~~COMPLETE_PHASED_IMPLEMENTATION.md~~ - OBSOLETE (27 phases)
- ~~Individual Phase_XX files~~ - REFERENCE ONLY

---

## Alpha Phase (Current Focus)

### Alpha 1.01 - Foundation ✅ CURRENT TARGET
**Goal**: Create and open database files with proper structure

**Deliverables**:
```c
// Exact executable behavior required:
$ scratchbird --version
ScratchBird v0.1.0-alpha.1.01

$ scratchbird create database test.db --page-size=16384
Database created successfully: test.db (16KB pages)

$ scratchbird open test.db
ScratchBird> .info
Database: test.db
Page Size: 16384
Pages: 2
UUID: 018b9f3a-7d4e-7f3a-9c5d-1234567890ab (v7)
```

### System Schemas (Purpose and Scope)

The following base schemas exist to organize core functionality and future features:
- [root]: top-level namespace; owner of base system structures
- [sys]: system catalog objects (schemas, tables, columns, indexes)
- [sec]: security objects (users, roles, grants) — metadata only in Alpha
- [agents]: reserved for agent automation metadata/logs (future)
- [app]: application-owned default schema placeholder
- [remote]: mount points for foreign schemas (future)
- [users]: per-user namespaces (future)
- [roles]: role-based namespaces (future)

### System Tables (Minimal Alpha Set)

Alpha must create and minimally populate:
- sys.schemas(schema_uuid, parent_uuid, path, name, level, created_time, owner_uuid, object_count)
- sys.tables(table_uuid, schema_uuid, table_name, full_path, column_count, row_count, data_pages, index_pages, created_time)
- sys.columns(column_uuid, table_uuid, column_name, column_position, data_type, max_length, is_nullable, is_primary_key, default_value)

Planned (defined later phases):
- sys.indexes (index_uuid, table_uuid, name, is_unique, type)
- sys.procedures (proc_uuid, schema_uuid, name, language)
- sys.version_control_log (object_uuid, version, change_time, author, summary)

**Technical Requirements**:
- Page sizes: **8192, 16384, 32768 ONLY** (64K/128K deferred to Beta)
- UUID: **v7 ONLY** (no v4 support)
- Checksum: **CRC32C (Castagnoli)** with little-endian storage
- File format: See `/workspace/references/technical_specifications/ON_DISK_FORMAT.md`

**Success Criteria**:
1. Database file created with valid header (Page 0)
2. System catalog initialized (Page 1)
3. Checksum validation on all reads
4. File lock prevents concurrent access

**Test Requirements**:
```cpp
TEST(Alpha101, CreateDatabase) {
    ASSERT_EQ(create_database("test.db", 16384), SB_OK);
    ASSERT_TRUE(file_exists("test.db"));
    ASSERT_EQ(get_file_size("test.db"), 16384 * 2);  // 2 pages minimum
}

TEST(Alpha101, ValidateHeader) {
    create_database("test.db", 16384);
    PageHeader header = read_page_header("test.db", 0);
    ASSERT_EQ(header.magic, 0x53425244);  // 'SBRD'
    ASSERT_EQ(header.version, 1);
    ASSERT_EQ(header.page_type, PAGE_TYPE_DATABASE_HEADER);
    ASSERT_TRUE(validate_checksum(&header));
}
```

---

### Alpha 1.02 - Page Management
**Goal**: Read, write, and manage pages

**Deliverables**:
- Page allocation bitmap
- Free space tracking
- Page read/write with validation
- Basic buffer pool (single-threaded)

**Technical Requirements**:
- Buffer pool size: 32 pages minimum
- Eviction: Simple LRU
- Dirty page tracking
- Write-ahead logging: NOT YET (deferred)

---

### Alpha 1.03 - Storage Engine
**Goal**: Store and retrieve tuples

**Deliverables**:
- Heap page format
- Tuple insertion
- Sequential scan
- Visibility rules (basic)

---

### Alpha 1.04 - Transaction Foundation
**Goal**: Basic ACID transactions

**Deliverables**:
- Transaction ID generation (64-bit)
- Transaction Inventory Pages (TIP)
- MVCC visibility (single connection)
- Commit/Rollback

---

### Alpha 1.05 - SQL Parser
**Goal**: Parse basic SQL statements

**Deliverables**:
- CREATE TABLE
- INSERT (single row)
- SELECT (no joins)
- Basic expressions

**Parser Mode**: Traditional with reserved words (context-aware deferred)

---

## Beta Phase (Future)

### Beta 2.01 - Extended Storage
- 64KB and 128KB page sizes
- Compression
- TOAST/LOB support

### Beta 2.02 - Concurrency
- Multi-threaded buffer pool
- Lock manager
- Deadlock detection

### Beta 2.03 - Advanced SQL
- Joins
- Subqueries
- Window functions
- CTEs

---

## Release Criteria

### Alpha Complete When:
- [ ] All Alpha 1.0x milestones complete
- [ ] Test coverage > 80%
- [ ] Documentation complete
- [ ] No critical bugs
- [ ] Performance: 100 TPS minimum

### Beta Complete When:
- [ ] All Beta 2.0x milestones complete
- [ ] PostgreSQL wire protocol 80% compatible
- [ ] Concurrent users: 10+
- [ ] Performance: 1000 TPS minimum

---

## Implementation Rules

### MUST Follow:
1. Implement phases in order (no skipping)
2. Each phase must pass all tests before proceeding
3. Document all deviations in CHANGELOG.md
4. Update METRICS.md weekly

### MUST NOT:
1. Implement Beta features in Alpha
2. Use UUID v4 (v7 only)
3. Support 64K/128K pages in Alpha
4. Add distributed features before Release 3.0

---

## Quick Reference

### Current Phase: Alpha 1.01
### Next Milestone: Database file creation
### Blocking Issues: See CRITICAL_REMEDIATION_PLAN.md
### Test Command: `ctest --test-dir build -R Alpha101`

---

## Appendix: Deprecated Plans

The following documents are kept for historical reference only:
- `/workspace/ProjectPlan/archive/` - Old planning documents
- Individual `Phase_XX.md` files - Superseded by this document

When in doubt, THIS DOCUMENT is the authority.