# Post-Alpha Stub Implementation & Testing Tracker

**Created:** February 6, 2026  
**Purpose:** Track remaining stub implementations and comprehensive page size testing  
**Editable:** Yes - remove tasks as needed

---

## 📋 How to Use This Document

1. **Each section has a checkbox** `[ ]` - check when complete
2. **Priority levels:** P0 (Critical), P1 (High), P2 (Medium), P3 (Low)
3. **Estimated effort:** Days/Weeks indicated for planning
4. **Delete sections** you don't want to implement
5. **Add notes** in the Notes column

---

## PART 1: STUB IMPLEMENTATION TRACKER

### 1.1 Parser DDL Stubs 📝

**Status:** Non-critical | **Total Effort:** 2-3 weeks

| # | Task | File | Line | Priority | Effort | Status | Notes |
|---|------|------|------|----------|--------|--------|-------|
| [ ] | 1.1.1 | Complete Firebird ALTER INDEX implementation | `firebird_parser.cpp` | 2747 | P2 | 2-3 days | Parse works, needs execution | |
| [ ] | 1.1.2 | Separate MySQL DDL to dedicated module | `mysql_parser.cpp` | 3977 | P3 | 1 week | Code reorganization | Currently inline |
| [ ] | 1.1.3 | Implement PostgreSQL advanced CREATE statements | `pg_parser_ddl.cpp` | 1351 | P3 | 1-2 weeks | CREATE RULE, POLICY, etc. | Core CREATE works |

**Rationale for inclusion:** Parser DDL stubs represent organizational debt. Core DDL works but needs cleanup.

---

### 1.2 FDW Adapter Stubs 🌐

**Status:** Low-Medium Impact | **Total Effort:** 1-2 weeks

| # | Task | File | Line | Priority | Effort | Status | Notes |
|---|------|------|------|----------|--------|--------|-------|
| [ ] | 1.2.1 | Implement PostgreSQL FDW MD5 authentication | `fdw/postgresql_adapter.cpp` | 897 | P2 | 3-4 days | FDW connections only | Use SCRAM workaround |
| [ ] | 1.2.2 | Document PostgreSQL FDW auth limitations | `fdw/postgresql_adapter.cpp` | 900 | P3 | 1 day | GSSAPI, Kerberos | Document don't implement |
| [ ] | 1.2.3 | Document MySQL cursor limitations | `fdw/mysql_adapter.cpp` | 630-642 | P3 | 1 day | MySQL architectural limit | Not ScratchBird limitation |

**Rationale for inclusion:** FDW stubs affect heterogeneous database integration. MySQL cursors are not implementable (MySQL limitation).

---

### 1.3 Index Factory Stubs 📊

**Status:** Low Impact | **Total Effort:** 1-2 weeks

| # | Task | File | Line | Priority | Effort | Status | Notes |
|---|------|------|------|----------|--------|--------|-------|
| [ ] | 1.3.1 | Enable LSM indexes in non-primary tablespaces (create) | `index_factory.cpp` | 255 | P3 | 1-2 weeks | Works in primary | Primary tablespace workaround |
| [ ] | 1.3.2 | Enable LSM indexes in non-primary tablespaces (open) | `index_factory.cpp` | 808 | P3 | 1 week | Same as above | Same fix as 1.3.1 |

**Rationale for inclusion:** LSM indexes work but limited to primary tablespace. Affects advanced storage layouts.

---

### 1.4 Optional Library Stubs 📦

**Status:** Graceful degradation | **Total Effort:** Documentation only

| # | Task | File | Line | Priority | Effort | Status | Notes |
|---|------|------|------|----------|--------|--------|-------|
| [ ] | 1.4.1 | Document GEOS requirement for spatial ops | `geos_wrapper.cpp` | 559 | P3 | 1 day | Install libgeos-dev | Optional library |
| [ ] | 1.4.2 | Document PROJ requirement for transforms | `proj_wrapper.h` | 220 | P3 | 1 day | Install libproj-dev | Optional library |
| [ ] | 1.4.3 | Document LZ4 compression options | `compression_lz4.cpp` | 187 | P3 | 1 day | Install liblz4-dev | Graceful fallback |

**Rationale for inclusion:** These are documentation tasks, not implementations. Libraries work when installed.

---

### 1.5 Infrastructure Stubs 🔧

**Status:** Connection pool simulation | **Total Effort:** 1-2 weeks

| # | Task | File | Line | Priority | Effort | Status | Notes |
|---|------|------|------|----------|--------|--------|-------|
| [ ] | 1.5.1 | Implement actual connection logic in pool | `pool/connection_pool.cpp` | 194 | P1 | 3-4 days | Currently simulated | Integrate with IPC |
| [ ] | 1.5.2 | Implement actual SQL execution in pool | `pool/connection_pool.cpp` | 208 | P1 | 2-3 days | Currently simulated | Use EngineIPCSessionHandler |
| [ ] | 1.5.3 | Implement parameterized execution in pool | `pool/connection_pool.cpp` | 223 | P1 | 2-3 days | Currently simulated | Same as above |
| [ ] | 1.5.4 | Implement result cache clearing | `pool/connection_pool.cpp` | 662 | P2 | 1 day | Empty implementation | Memory management |
| [ ] | 1.5.5 | Windows daemon service implementation | `server/daemon.cpp` | 662 | P3 | N/A | Use Windows services | Platform-specific |

**Rationale for inclusion:** Connection pool stubs affect metrics and monitoring but not actual connectivity. Windows stubs are by design.

---

## PART 2: PAGE SIZE COMPREHENSIVE TESTING

### 2.1 Supported Page Sizes

ScratchBird supports 5 page sizes:

| Page Size | Bytes | Use Case | Status |
|-----------|-------|----------|--------|
| 8KB | 8192 | Small databases, embedded | Needs full testing |
| 16KB | 16384 | **Default** - balanced | Needs full testing |
| 32KB | 32768 | Medium databases | Needs full testing |
| 64KB | 65536 | Large databases | Needs full testing |
| 128KB | 131072 | Very large databases, DSS | Needs full testing |

**Critical:** All page sizes must be tested across ALL subsystems.

---

### 2.2 Page Size Testing Matrix

**Legend:** `[ ]` = Not tested | `[/]` = Partially tested | `[x]` = Fully tested

#### 2.2.1 Core Storage Engine

| # | Test Category | 8KB | 16KB | 32KB | 64KB | 128KB | Notes |
|---|---------------|-----|------|------|------|-------|-------|
| [ ] | 2.2.1.1 | Database creation and header validation | [ ] | [ ] | [ ] | [ ] | [ ] | All sizes |
| [ ] | 2.2.1.2 | Page header format consistency | [ ] | [ ] | [ ] | [ ] | [ ] | Check offsets |
| [ ] | 2.2.1.3 | Buffer pool allocation/deallocation | [ ] | [ ] | [ ] | [ ] | [ ] | Memory pressure |
| [ ] | 2.2.1.4 | Page read/write correctness | [ ] | [ ] | [ ] | [ ] | [ ] | CRC validation |
| [ ] | 2.2.1.5 | Multi-page tuple storage | [ ] | [ ] | [ ] | [ ] | [ ] | Large rows |
| [ ] | 2.2.1.6 | Page boundary alignment | [ ] | [ ] | [ ] | [ ] | [ ] | Pointer arithmetic |
| [ ] | 2.2.1.7 | Recovery and checkpointing | [ ] | [ ] | [ ] | [ ] | [ ] | Page size in headers |

#### 2.2.2 TOAST (Oversized Attribute Storage)

| # | Test Category | 8KB | 16KB | 32KB | 64KB | 128KB | Notes |
|---|---------------|-----|------|------|------|-------|-------|
| [ ] | 2.2.2.1 | TOAST threshold calculation | [ ] | [ ] | [ ] | [ ] | [ ] | page_size/32 |
| [ ] | 2.2.2.2 | TOAST chunk size calculation | [ ] | [ ] | [ ] | [ ] | [ ] | page_size/4 |
| [ ] | 2.2.2.3 | Large VARCHAR TOASTing | [ ] | [ ] | [ ] | [ ] | [ ] | > threshold |
| [ ] | 2.2.2.4 | BYTEA TOASTing | [ ] | [ ] | [ ] | [ ] | [ ] | Binary data |
| [ ] | 2.2.2.5 | JSON/JSONB TOASTing | [ ] | [ ] | [ ] | [ ] | [ ] | Large documents |
| [ ] | 2.2.2.6 | TOAST compression with LZ4 | [ ] | [ ] | [ ] | [ ] | [ ] | If enabled |
| [ ] | 2.2.2.7 | TOAST fetch and reconstruction | [ ] | [ ] | [ ] | [ ] | [ ] | Chunk ordering |
| [ ] | 2.2.2.8 | Inline vs out-of-line decision | [ ] | [ ] | [ ] | [ ] | [ ] | Boundary cases |

#### 2.2.3 Index Structures

| # | Test Category | 8KB | 16KB | 32KB | 64KB | 128KB | Notes |
|---|---------------|-----|------|------|------|-------|-------|
| [ ] | 2.2.3.1 | B-Tree index page layout | [ ] | [ ] | [ ] | [ ] | [ ] | Entry capacity |
| [ ] | 2.2.3.2 | B-Tree page splits | [ ] | [ ] | [ ] | [ ] | [ ] | Fill factor |
| [ ] | 2.2.3.3 | B-Tree page merges | [ ] | [ ] | [ ] | [ ] | [ ] | Underflow |
| [ ] | 2.2.3.4 | GIN index posting lists | [ ] | [ ] | [ ] | [ ] | [ ] | Large postings |
| [ ] | 2.2.3.5 | GiST index page layout | [ ] | [ ] | [ ] | [ ] | [ ] | Tree structure |
| [ ] | 2.2.3.6 | Hash index bucket sizes | [ ] | [ ] | [ ] | [ ] | [ ] | Bucket chains |
| [ ] | 2.2.3.7 | BRIN index block ranges | [ ] | [ ] | [ ] | [ ] | [ ] | Range summaries |
| [ ] | 2.2.3.8 | LSM-Tree block sizes | [ ] | [ ] | [ ] | [ ] | [ ] | SSTable blocks |
| [ ] | 2.2.3.9 | Columnstore page sizes | [ ] | [ ] | [ ] | [ ] | [ ] | Segment sizes |

#### 2.2.4 Heap Storage

| # | Test Category | 8KB | 16KB | 32KB | 64KB | 128KB | Notes |
|---|---------------|-----|------|------|------|-------|-------|
| [ ] | 2.2.4.1 | Heap page header (PageHeaderData) | [ ] | [ ] | [ ] | [ ] | [ ] | Size varies |
| [ ] | 2.2.4.2 | Line pointer array (ItemIdData) | [ ] | [ ] | [ ] | [ ] | [ ] | Max tuples/page |
| [ ] | 2.2.4.3 | Free space management (pd_lower/pd_upper) | [ ] | [ ] | [ ] | [ ] | [ ] | Fragmentation |
| [ ] | 2.2.4.4 | Tuple insertion and visibility | [ ] | [ ] | [ ] | [ ] | [ ] | MVCC headers |
| [ ] | 2.2.4.5 | HOT (Heap Only Tuple) updates | [ ] | [ ] | [ ] | [ ] | [ ] | Chain pointers |
| [ ] | 2.2.4.6 | Page compaction (defragmentation) | [ ] | [ ] | [ ] | [ ] | [ ] | Reordering |
| [ ] | 2.2.4.7 | Dead tuple cleanup (VACUUM) | [ ] | [ ] | [ ] | [ ] | [ ] | Space reclamation |

#### 2.2.5 Transaction & MVCC

| # | Test Category | 8KB | 16KB | 32KB | 64KB | 128KB | Notes |
|---|---------------|-----|------|------|------|-------|-------|
| [ ] | 2.2.5.1 | Transaction ID wraparound | [ ] | [ ] | [ ] | [ ] | [ ] | XID boundaries |
| [ ] | 2.2.5.2 | Commit log (CLOG) page alignment | [ ] | [ ] | [ ] | [ ] | [ ] | Per-page tracking |
| [ ] | 2.2.5.3 | Visibility map page size | [ ] | [ ] | [ ] | [ ] | [ ] | All-frozen bits |
| [ ] | 2.2.5.4 | Free space map (FSM) | [ ] | [ ] | [ ] | [ ] | [ ] | Page availability |
| [ ] | 2.2.5.5 | Multi-version chain storage | [ ] | [ ] | [ ] | [ ] | [ ] | Version chains |
| [ ] | 2.2.5.6 | Snapshot serialization | [ ] | [ ] | [ ] | [ ] | [ ] | Subtransaction limits |

#### 2.2.6 Catalog & System Tables

| # | Test Category | 8KB | 16KB | 32KB | 64KB | 128KB | Notes |
|---|---------------|-----|------|------|------|-------|-------|
| [ ] | 2.2.6.1 | System catalog page layout | [ ] | [ ] | [ ] | [ ] | [ ] | pg_class, pg_attribute |
| [ ] | 2.2.6.2 | Index catalog metadata | [ ] | [ ] | [ ] | [ ] | [ ] | pg_index |
| [ ] | 2.2.6.3 | Type catalog storage | [ ] | [ ] | [ ] | [ ] | [ ] | pg_type |
| [ ] | 2.2.6.4 | Constraint catalog | [ ] | [ ] | [ ] | [ ] | [ ] | pg_constraint |

#### 2.2.7 Wire Protocol & IPC

| # | Test Category | 8KB | 16KB | 32KB | 64KB | 128KB | Notes |
|---|---------------|-----|------|------|------|-------|-------|
| [ ] | 2.2.7.1 | SBWP message framing | [ ] | [ ] | [ ] | [ ] | [ ] | Buffer sizes |
| [ ] | 2.2.7.2 | COPY protocol streaming | [ ] | [ ] | [ ] | [ ] | [ ] | Chunk sizes |
| [ ] | 2.2.7.3 | Result set buffering | [ ] | [ ] | [ ] | [ ] | [ ] | Fetch sizes |
| [ ] | 2.2.7.4 | Large result set handling | [ ] | [ ] | [ ] | [ ] | [ ] | Memory pressure |

#### 2.2.8 Backup & Recovery

| # | Test Category | 8KB | 16KB | 32KB | 64KB | 128KB | Notes |
|---|---------------|-----|------|------|------|-------|-------|
| [ ] | 2.2.8.1 | Physical backup page consistency | [ ] | [ ] | [ ] | [ ] | [ ] | Block-level copy |
| [ ] | 2.2.8.2 | Incremental backup deltas | [ ] | [ ] | [ ] | [ ] | [ ] | Page-level diff |
| [ ] | 2.2.8.3 | Point-in-time recovery | [ ] | [ ] | [ ] | [ ] | [ ] | WAL page alignment |
| [ ] | 2.2.8.4 | Restore validation | [ ] | [ ] | [ ] | [ ] | [ ] | Page CRC checks |

#### 2.2.9 Stress & Edge Cases

| # | Test Category | 8KB | 16KB | 32KB | 64KB | 128KB | Notes |
|---|---------------|-----|------|------|------|-------|-------|
| [ ] | 2.2.9.1 | Maximum tuples per page | [ ] | [ ] | [ ] | [ ] | [ ] | Line pointer limits |
| [ ] | 2.2.9.2 | Single tuple spanning multiple pages | [ ] | [ ] | [ ] | [ ] | [ ] | Without TOAST |
| [ ] | 2.2.9.3 | Page at exactly 100% fill | [ ] | [ ] | [ ] | [ ] | [ ] | Insert failure |
| [ ] | 2.2.9.4 | Rapid page size switching | [ ] | N/A | N/A | N/A | N/A | Separate databases |
| [ ] | 2.2.9.5 | Cross-page-size compatibility | [ ] | N/A | N/A | N/A | N/A | Backup/restore |

---

### 2.3 Page Size Specific Test Cases

#### 2.3.1 TOAST Threshold Calculations

```
Page Size | TOAST Threshold (page/32) | Max Chunk (page/4) | Target (page/16)
----------|---------------------------|--------------------|-----------------
8192      | 256 bytes                 | 2032 bytes         | 512 bytes
16384     | 512 bytes                 | 4080 bytes         | 1024 bytes
32768     | 1024 bytes                 | 8176 bytes         | 2048 bytes
65536     | 2048 bytes                 | 16368 bytes        | 4096 bytes
131072    | 4096 bytes                 | 32752 bytes        | 8192 bytes
```

| # | Test | 8KB | 16KB | 32KB | 64KB | 128KB |
|---|------|-----|------|------|------|-------|
| [ ] | 2.3.1.1 | Data exactly at threshold | [ ] | [ ] | [ ] | [ ] | [ ] |
| [ ] | 2.3.1.2 | Data just below threshold | [ ] | [ ] | [ ] | [ ] | [ ] |
| [ ] | 2.3.1.3 | Data just above threshold | [ ] | [ ] | [ ] | [ ] | [ ] |
| [ ] | 2.3.1.4 | Maximum TOAST chunks | [ ] | [ ] | [ ] | [ ] | [ ] |
| [ ] | 2.3.1.5 | TOAST chunk boundary | [ ] | [ ] | [ ] | [ ] | [ ] |

#### 2.3.2 B-Tree Index Capacity

```
Page Size | ~Max Keys (8-byte) | ~Max Keys (32-byte) | ~Max Keys (128-byte)
----------|--------------------|---------------------|---------------------
8192      | ~500               | ~150                | ~45
16384     | ~1100              | ~350                | ~100
32768     | ~2400              | ~750                | ~220
65536     | ~5000              | ~1600               | ~470
131072    | ~10000             | ~3300               | ~980
```

| # | Test | 8KB | 16KB | 32KB | 64KB | 128KB |
|---|------|-----|------|------|------|-------|
| [ ] | 2.3.2.1 | Index at maximum fill | [ ] | [ ] | [ ] | [ ] | [ ] |
| [ ] | 2.3.2.2 | Page split at boundary | [ ] | [ ] | [ ] | [ ] | [ ] |
| [ ] | 2.3.2.3 | Root page promotion | [ ] | [ ] | [ ] | [ ] | [ ] |

---

### 2.4 Test Implementation Template

For each page size test, create:

```cpp
TEST_F(PageSizeTestSuite, TestName_PageSize8192) {
    TestWithPageSize(8192);
}
TEST_F(PageSizeTestSuite, TestName_PageSize16384) {
    TestWithPageSize(16384);
}
TEST_F(PageSizeTestSuite, TestName_PageSize32768) {
    TestWithPageSize(32768);
}
TEST_F(PageSizeTestSuite, TestName_PageSize65536) {
    TestWithPageSize(65536);
}
TEST_F(PageSizeTestSuite, TestName_PageSize131072) {
    TestWithPageSize(131072);
}
```

---

## PART 3: INTEGRATION & VALIDATION

### 3.1 Cross-Component Testing

| # | Integration Test | Page Sizes | Priority |
|---|------------------|------------|----------|
| [ ] | 3.1.1 | End-to-end query execution | All | P0 |
| [ ] | 3.1.2 | Index create/drop/rebuild | All | P0 |
| [ ] | 3.1.3 | Backup and restore cycle | All | P0 |
| [ ] | 3.1.4 | TOAST + compression + encryption | All | P1 |
| [ ] | 3.1.5 | MVCC + page compaction + VACUUM | All | P1 |
| [ ] | 3.1.6 | Large transaction rollback | All | P1 |

### 3.2 Performance Benchmarks

| # | Benchmark | Page Sizes | Metric |
|---|-----------|------------|--------|
| [ ] | 3.2.1 | Sequential read throughput | All | MB/s |
| [ ] | 3.2.2 | Random read IOPS | All | ops/sec |
| [ ] | 3.2.3 | Write throughput | All | MB/s |
| [ ] | 3.2.4 | Index build time | All | seconds |
| [ ] | 3.2.5 | Space efficiency | All | bytes/tuple |

### 3.3 Compatibility Matrix

| Feature | 8KB | 16KB | 32KB | 64KB | 128KB | Notes |
|---------|-----|------|------|------|-------|-------|
| PostgreSQL protocol | [ ] | [ ] | [ ] | [ ] | [ ] | |
| MySQL protocol | [ ] | [ ] | [ ] | [ ] | [ ] | |
| Firebird protocol | [ ] | [ ] | [ ] | [ ] | [ ] | |
| SBWP protocol | [ ] | [ ] | [ ] | [ ] | [ ] | |
| SCRAM auth | [ ] | [ ] | [ ] | [ ] | [ ] | |
| COPY protocol | [ ] | [ ] | [ ] | [ ] | [ ] | |
| SSL/TLS | [ ] | [ ] | [ ] | [ ] | [ ] | |

---

## PART 4: SUMMARY & PLANNING

### 4.1 Effort Summary

| Category | Tasks | Est. Effort | Priority |
|----------|-------|-------------|----------|
| Parser DDL Stubs | 3 | 2-3 weeks | P2-P3 |
| FDW Adapter Stubs | 3 | 1 week | P2-P3 |
| Index Factory Stubs | 2 | 1-2 weeks | P3 |
| Optional Library Docs | 3 | 3 days | P3 |
| Infrastructure Stubs | 4 | 1-2 weeks | P1-P2 |
| Page Size Testing | 150+ tests | 4-6 weeks | P0 |
| **TOTAL** | **165+** | **9-15 weeks** | **Mixed** |

### 4.2 Recommended Phasing

#### Phase 1: Foundation (Weeks 1-2)
- [ ] Connection pool integration (1.5.1-1.5.3)
- [ ] Page size testing - Core Storage (2.2.1)
- [ ] Page size testing - TOAST (2.2.2)

#### Phase 2: Index & Storage (Weeks 3-4)
- [ ] LSM tablespace support (1.3.1-1.3.2)
- [ ] Page size testing - Index Structures (2.2.3)
- [ ] Page size testing - Heap Storage (2.2.4)

#### Phase 3: Parser DDL (Weeks 5-6)
- [ ] Firebird ALTER INDEX (1.1.1)
- [ ] MySQL DDL separation (1.1.2)
- [ ] PostgreSQL CREATE statements (1.1.3)

#### Phase 4: Integration (Weeks 7-8)
- [ ] Cross-component testing (3.1)
- [ ] Performance benchmarks (3.2)
- [ ] Protocol compatibility (3.3)

#### Phase 5: Polish (Weeks 9+)
- [ ] FDW improvements (1.2.x)
- [ ] Documentation (1.4.x)
- [ ] Remaining infrastructure (1.5.4-1.5.5)

---

## Notes for Editing

1. **Remove entire sections** by deleting them
2. **Change priorities** by editing the Priority column
3. **Add new tests** following the numbering scheme
4. **Mark complete** by changing `[ ]` to `[x]`
5. **Add comments** in the Notes column

---

**Document Version:** 1.0  
**Last Updated:** 2026-02-06  
**Next Review:** Upon completion of Phase 1
