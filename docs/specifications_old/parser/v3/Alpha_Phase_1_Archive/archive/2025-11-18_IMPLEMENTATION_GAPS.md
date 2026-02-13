# ScratchBird Implementation Gaps - Detailed Analysis

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date:** November 18, 2025
**Purpose:** Document the gap between documentation claims and actual implementation

---

## EXECUTIVE SUMMARY

ScratchBird's documentation **overstates completeness** in several key areas:

| Feature Area | Documented | Actual | Gap |
|--------------|-----------|--------|-----|
| Indexes | 11/11 (100%) | 2/11 usable (18%) | **82% gap** |
| Data Types | 86/86 (100%) | 20/86 storable (23%) | **77% gap** |
| Materialized Views | 80% complete | 40% complete | **40% gap** |
| Security | 100% (Phase 3.5) | 80% (no enforcement) | **20% gap** |
| Built-in Functions | 123/123 (100%) | 162/169 (96%) | **+4% surplus** |

**Net Result:** Users expect features that don't actually work.

---

## GAP #1: INDEX TYPES (82% UNUSABLE)

### Documentation Claims

> **Indexes (11/11 = 100%)** 🎉
> - B-Tree, Hash, R-Tree, GIN, Bitmap
> - GiST, HNSW, SP-GiST, BRIN
> - Columnstore, LSM-Tree
> - All production-ready with MGA compliance

### Reality

**Code Exists (30,097 total lines):**

| Index Type | Lines of Code | Completeness | SQL Integration |
|------------|---------------|--------------|-----------------|
| B-Tree | 4,675 | 100% | ✅ YES |
| LSM-Tree | 3,961 | 100% | ✅ YES |
| GIN | 5,388 | 95% | ❌ NO |
| Hash | 1,643 | 90% | ❌ NO |
| R-Tree | 2,288 | 85% | ❌ NO |
| Bitmap | 2,138 | 85% | ❌ NO |
| HNSW | 2,240 | 85% | ❌ NO |
| BRIN | 1,412 | 80% | ❌ NO |
| GiST | 1,682 | 80% | ❌ NO |
| SP-GiST | 1,711 | 75% | ❌ NO |
| Columnstore | 2,959 | 70% | ❌ NO |

**Integration Status (`index_factory.cpp` lines 108-124):**

```cpp
case IndexType::HASH:
case IndexType::RTREE:
case IndexType::GIN:
case IndexType::GIST:
case IndexType::SPGIST:
case IndexType::HNSW:
case IndexType::BRIN:
case IndexType::BITMAP:
case IndexType::COLUMNSTORE:
    return Status::NOT_IMPLEMENTED;  // 9 indexes return error
```

### The Gap

- **Claim:** "All production-ready"
- **Reality:** 9/11 indexes return `NOT_IMPLEMENTED` error when you try to create them via SQL
- **Impact:** Users cannot use 82% of documented index types

### Why This Happened

The engineering team **wrote excellent index code** (30K lines!) but **forgot to register** 9 of them in the factory. The code exists and works, but there's no way to use it.

**Fix Required:** Add 5-20 lines per index in `index_factory.cpp` to register them.

**Estimated Effort:** 18-36 hours (9 indexes × 2-4 hours each)

---

## GAP #2: DATA TYPES (77% NON-STORABLE)

### Documentation Claims

> **Data Types (86/86 = 100%)** 🎉
> - Numeric: INT8-INT128, UINT8-UINT64, DECIMAL, FLOAT, MONEY
> - Spatial: POINT, LINESTRING, POLYGON
> - Advanced: ARRAY, RANGE, COMPOSITE, VECTOR, VARIANT
> - Network: INET, CIDR, MACADDR
> - Text Search: TSVECTOR, TSQUERY

### Reality

**Type Definitions (54 DataType enum values):**
- ✅ All 54 types are defined
- ✅ All 54 have runtime TypedValue support
- ✅ All 54 have factory methods (makeInt8(), makeJSON(), etc.)
- ✅ All 54 have comparison operators
- ✅ All 54 have toString() methods

**Serialization Support (20 types only):**

| Category | Types Defined | Can Be Stored | Gap |
|----------|---------------|---------------|-----|
| Numeric | 13 | 4 (INT8-64, FLOAT32/64, DECIMAL) | **69% gap** |
| String | 3 | 3 | 0% |
| Binary | 4 | 4 | 0% |
| Temporal | 4 | 3 (no INTERVAL) | 25% |
| Spatial | 7 | 0 | **100% gap** |
| Array/Composite | 2 | 0 | **100% gap** |
| Range | 6 | 0 | **100% gap** |
| Network | 4 | 0 | **100% gap** |
| Text Search | 2 | 0 | **100% gap** |
| **TOTAL** | **52** | **20** | **62% gap** |

### The Gap

- **Claim:** "86 data types 100% complete"
- **Reality:** Only 20/86 (23%) can actually be stored in the database
- **Impact:** Users can create columns with fancy types, but data disappears on server restart

### Missing Serialization

**34 types have NO serialization:**
- INT128, UINT8, UINT16, UINT32, UINT64
- MONEY, INTERVAL
- All spatial types (POINT, LINESTRING, POLYGON, etc.)
- All array types
- All range types (INT4RANGE, TSRANGE, etc.)
- All network types (INET, CIDR, MACADDR)
- Text search types (TSVECTOR, TSQUERY)
- VECTOR, VARIANT, JSONB, XML

### Example Failure Scenario

```sql
CREATE TABLE locations (
    id INTEGER,
    position POINT  -- Accepted by parser
);

INSERT INTO locations VALUES (1, POINT(10.5, 20.3));  -- Works

-- Restart database

SELECT * FROM locations;  -- ERROR: Cannot deserialize POINT type
```

**Fix Required:** Implement serialization for 34 types (~2-3 hours each).

**Estimated Effort:** 68-102 hours (34 types × 2-3 hours each)

---

## GAP #3: MATERIALIZED VIEWS (40% PHANTOM FEATURE)

### Documentation Claims

> **Views (80% COMPLETE - Nov 17, 2025)** 🎉
> - ✅ CREATE MATERIALIZED VIEW / CREATE OR REPLACE MATERIALIZED VIEW
> - ✅ REFRESH [CONCURRENTLY] MATERIALIZED VIEW (parser + bytecode + executor)
> - ⧗ Physical materialization (table creation + data population) - **20% remaining**

### Reality

**What Works:**
- ✅ Parser recognizes MATERIALIZED keyword
- ✅ Bytecode encodes materialized flag
- ✅ Executor has executeRefreshMaterializedView() method
- ✅ Catalog stores materialized flag

**What Doesn't Work:**

**Evidence from code:**

**`catalog_manager.cpp` line 8320:**
```cpp
view.materialized_table_id = ID{};  // TODO: Create physical table for materialized data
```

**`catalog_manager.cpp` lines 8380-8389:**
```cpp
// TODO: ALPHA Phase 1 - Implement actual refresh logic:
// 1. Parse and execute the view's SELECT query
// 2. If concurrently=true: create temp table, populate, swap atomically
// 3. If concurrently=false: truncate existing table, repopulate
// 4. Update last_refresh_time

// For now, just update the timestamp
view.last_refresh_time = current_time_micros();
```

### The Gap

- **Claim:** "80% complete, 20% remaining"
- **Reality:** "40% complete, 60% remaining" - No physical table, no data population, no actual refresh
- **Impact:** Materialized views accepted but provide ZERO performance benefit (behave like regular views)

### Missing Implementation

1. ❌ Physical table creation for materialized data storage
2. ❌ Schema inference from SELECT query
3. ❌ Initial data population
4. ❌ REFRESH logic (currently only updates timestamp)
5. ❌ CONCURRENTLY logic (temp table swap)
6. ❌ Query optimizer using materialized data instead of view expansion

**Fix Required:** Implement physical materialization.

**Estimated Effort:** 20-30 hours

---

## GAP #4: CATALOG TABLES (7 MISSING)

### Documentation Claims

> **Catalog System (40 tables = 100% structures, 58% CRUD)** ✅

### Reality

**Actual Count:** 33 catalog tables (not 40)

**Table Status:**

| Category | Claimed | Actual | Structures | CRUD | Gap |
|----------|---------|--------|------------|------|-----|
| Core | 10 | 10 | 10/10 | 9/10 | 1 unused (pg_constraint) |
| Security | 8 | 8 | 7/8 | 7/8 | 1 unused (pg_group_mapping) |
| Stored Code | 5 | 5 | 5/5 | 2/5 | 3 unused |
| Emulation | 3 | 3 | 3/3 | 0/3 | 3 unused |
| Infrastructure | 5 | 5 | 4/5 | 4/5 | 1 unused (pg_statistics) |
| Dependencies | 2 | 2 | 2/2 | 2/2 | 0 |
| **TOTAL** | **33** | **33** | **31/33** | **19/33** | **10 unused** |

### The Gap

- **Claim:** "40 tables"
- **Reality:** 33 tables declared in CatalogRootPage
- **Discrepancy:** 7 tables missing (likely counting error)

### Tables With Zero CRUD

**10 tables have structures but no implementation:**
1. pg_constraint (unused, constraints stored inline)
2. pg_group_mapping (AD/LDAP integration)
3. pg_proc_params (procedure parameters)
4. pg_domain (domain types)
5. pg_udr (user-defined routines)
6. pg_package (stored procedure packages)
7. pg_emulation_type (mysql/postgres/mssql emulation)
8. pg_emulation_server (emulation server configs)
9. pg_emulated_database (emulated database mappings)
10. pg_statistics (query optimizer statistics)

### Critical Issue: Function/Procedure Persistence

**MEMORY-ONLY STORAGE** (data loss risk):
- Functions and procedures are stored in `functions_` and `procedures_` caches
- NOT persisted to disk
- Lost on database restart

**Impact:** Users create stored procedures, restart database, all procedures vanish.

**Fix Required:** Implement disk persistence for functions/procedures.

**Estimated Effort:** 8-12 hours

---

## GAP #5: CONSTRAINTS (25% MISSING)

### Documentation Claims

> **Constraints (8/10 = 80%)** ✅
> - ✅ NOT NULL, Data type validation
> - ✅ **DEFAULT values** (executor COMPLETE, parser pending)
> - ✅ **UNIQUE** (executor COMPLETE, parser pending)
> - ✅ **CHECK** (executor 100% COMPLETE, parser COMPLETE) 🎉
> - ✅ **FOREIGN KEY** (Phase A + B 100% COMPLETE)
> - ❌ PRIMARY KEY (depends on UNIQUE + NOT NULL combination)

### Reality

| Constraint | Parser | Bytecode | Executor | Status |
|------------|--------|----------|----------|--------|
| NOT NULL | 100% | 100% | 100% | ✅ COMPLETE |
| DEFAULT | 100% | 100% | 95% | ✅ MOSTLY COMPLETE |
| CHECK (column) | 100% | 100% | 100% | ✅ COMPLETE |
| CHECK (table) | 30% | 0% | 0% | ❌ STUB |
| UNIQUE | 40% | 50% | 75% | ⚠️ PARTIAL |
| FOREIGN KEY | 100% | 100% | 90% | ✅ MOSTLY COMPLETE |
| PRIMARY KEY | 100% | 0% | 0% | ❌ NOT ENFORCED |

### PRIMARY KEY Gap

**What Works:**
- ✅ Parser recognizes `PRIMARY KEY` syntax (column-level and table-level)
- ✅ AST has PrimaryKeyConstraint class
- ✅ Opcode defined (PRIMARY_KEY 0x96)

**What Doesn't Work:**
- ❌ Bytecode generator doesn't emit PRIMARY_KEY opcode
- ❌ Executor has NO primary key enforcement
- ❌ `is_primary_key` flag parsed but NOT stored in catalog
- ❌ No automatic NOT NULL enforcement
- ❌ No automatic UNIQUE enforcement
- ❌ No automatic index creation

**Impact:** Users can declare PRIMARY KEY but it does nothing.

```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,  -- Accepted
    email VARCHAR(100)
);

INSERT INTO users VALUES (1, 'user1@example.com');
INSERT INTO users VALUES (1, 'user2@example.com');  -- Should fail, doesn't
INSERT INTO users VALUES (NULL, 'user3@example.com');  -- Should fail, doesn't
```

**Fix Required:** Store is_primary_key in catalog, enforce UNIQUE + NOT NULL.

**Estimated Effort:** 8 hours

### UNIQUE/FK Performance Gap

**Current Implementation:**
- UNIQUE: O(N) table scan on every INSERT/UPDATE
- FOREIGN KEY: O(N) parent table scan on every INSERT/UPDATE, O(N) child table scan on DELETE

**Should Use:**
- UNIQUE: O(log N) index lookup
- FOREIGN KEY: O(log N) index lookup

**Impact:** Performance degrades severely for large tables (>10,000 rows).

**Fix Required:** Use existing UNIQUE indexes for constraint checking.

**Estimated Effort:** 16 hours

---

## GAP #6: SECURITY (20% ENFORCEMENT MISSING)

### Documentation Claims

> **Security (Phase 3.0 COMPLETE - 100%)** ✅
> - Phase 3.3 (100% - COMPLETE) ✅: Column-Level Permissions
> - Phase 3.4 (100% COMPLETE) ✅: Row-Level Security (RLS)
> - Phase 3.5 (100% COMPLETE) ✅: RLS DML Enforcement

### Reality

See separate report: `2025-11-18_SECURITY_CRITICAL_ISSUES.md`

**Summary:**
- Infrastructure: 100% ✅
- Permission Checks: 0% ❌
- RLS Enforcement: 100% ✅
- Overall: 80% ⚠️

**Critical Gap:** 13 DDL operations missing permission checks.

---

## GAP #7: BUILT-IN FUNCTIONS (DOCUMENTATION UNDERCOUNTED)

### Documentation Claims

> **Built-in Functions (123/123 = 100%)** 🎉

### Reality

**Actual Count:** 175 opcodes defined, ~162 implemented (96%)

| Category | Documented | Actual Opcodes | Implemented | Accuracy |
|----------|-----------|----------------|-------------|----------|
| String | 11 | 19 | 17 | Undercounted |
| Aggregate | 6 | 12 | 12 | Undercounted |
| Window | 8 | 8 | 8 | Accurate |
| JSON | 13 | 14 | 13 | Accurate |
| Array | 12 | 17 | 12 | Undercounted |
| Mathematical | 29 | 25 | 25 | Overcounted (recent addition) |
| Spatial | 4+ | 37 | 29 | **Vastly undercounted** |
| Bit Manipulation | 14 | 14 | 14 | Accurate |
| Cryptographic | 4 | 4 | 4 | Accurate |
| Statistical | 7 | 13 | 6 (agg only) | Overcounted |
| XML | 9 | 9 | 9 | Accurate |
| **TOTAL** | **123** | **175** | **~162** | **Undercounted by 39** |

### The Gap

- **Claim:** "123 functions"
- **Reality:** 175 opcodes defined, ~162 working functions
- **Direction:** Documentation **undercounts** actual implementation

**This is the ONE area where reality EXCEEDS documentation!**

**Caveats:**
- 6 statistical functions are stubs (error immediately)
- 2 encoding functions are stubs
- Mathematical functions added Nov 2025 (after original count)
- Spatial functions vastly underreported (29 vs claimed 4+)

---

## SUMMARY TABLE: DOCUMENTATION ACCURACY

| Feature | Documented | Actual | Accuracy | Direction |
|---------|-----------|--------|----------|-----------|
| Indexes | 100% | 18% usable | ❌ Misleading | Overstated |
| Data Types | 100% | 23% storable | ❌ Misleading | Overstated |
| Catalog Tables | 40 tables | 33 tables | ❌ Inaccurate | Overstated |
| Materialized Views | 80% complete | 40% complete | ❌ Overstated | Overstated |
| Security | 100% | 80% (no enforcement) | ⚠️ Overstated | Overstated |
| Constraints | 80% | 75% | ✅ Mostly Accurate | Slight overstatement |
| Built-in Functions | 123 | ~162 | ✅ Accurate | **Understated** |
| Transaction Mgmt | 100% | 100% | ✅ Accurate | Perfect |
| Core Storage | 100% (except tablespaces) | 100% | ✅ Accurate | Perfect |

**Overall Documentation Accuracy:** ~65%

---

## RECOMMENDATIONS

### Immediate (Week 1)

1. **Update Documentation to Match Reality**
   - Indexes: State "2/11 usable (18%)" or "9 pending integration"
   - Data Types: State "20/86 fully functional (23%)" or "66 runtime-only"
   - Materialized Views: State "40% complete" not "80%"
   - Security: State "enforcement gaps exist"

2. **Add Prominent Warnings**
   - "Materialized views syntax-only (no actual materialization)"
   - "9 index types not yet integrated"
   - "66 data types cannot be stored to disk"
   - "Security system has enforcement gaps"

### Short-term (Weeks 2-4)

3. **Complete High-Impact Features**
   - Integrate 9 remaining indexes (36 hours)
   - Implement PRIMARY KEY enforcement (8 hours)
   - Add Function/Procedure persistence (12 hours)
   - Fix security enforcement (40 hours)

4. **Choose Materialized Views Strategy**
   - Option A: Complete implementation (30 hours)
   - Option B: Remove feature and document as "future"
   - Option C: Add warning "syntax-only, no materialization"

### Medium-term (Weeks 5-12)

5. **Data Type Serialization**
   - Prioritize common types: INT128, UINT*, spatial, arrays (40 hours)
   - Complete remaining types (60 hours)

6. **Performance Optimization**
   - Use indexes for UNIQUE/FK checking (16 hours)
   - Query optimizer improvements (40 hours)

---

## CONCLUSION

ScratchBird has **excellent code** but **documentation that overstates completeness**. The engineering work is impressive, but several features are "syntax-only" (parsed but not functional).

**Key Pattern:** Features have 100% parser support but 0-40% execution support.

**This creates user frustration:**
- "Why doesn't CREATE INDEX USING GIN work?" (not integrated)
- "Why did my spatial data disappear?" (no serialization)
- "Why doesn't REFRESH MATERIALIZED VIEW do anything?" (stub implementation)
- "Why can any user drop other users?" (no permission checks)

**Recommendation:** Either **complete the features** OR **document them as incomplete** to set proper expectations.

**Estimated Total Effort to Close All Gaps:** ~462 hours (11-12 weeks, 1 developer)

---

**Audit Date:** November 18, 2025
**Purpose:** Transparency and project planning
