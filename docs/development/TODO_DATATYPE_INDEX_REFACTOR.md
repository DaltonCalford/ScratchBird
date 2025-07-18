# ScratchBird v0.5 - Datatype & Index Enhancement Roadmap

## Executive Summary

Based on comprehensive analysis of ScratchBird's current capabilities versus modern database requirements, this document outlines a strategic refactor plan to enhance ScratchBird's datatype and indexing systems. The analysis compares ScratchBird (Firebird 6.0.0.929 foundation) against Oracle, SQL Server, MySQL, MariaDB, and PostgreSQL capabilities.

**Current ScratchBird Status:** ✅ **Strong Foundation**
- Modern 128-bit integer support (`INT128`)
- IEEE 754-2008 compliant decimal floating-point (`DECFLOAT(16)`, `DECFLOAT(34)`)
- Full temporal support including timezone-aware types
- JSON support with native storage
- UUID native support
- Boolean type support
- Advanced hierarchical schema system (11-level deep)

---

## Part I: Datatype System Analysis

### 1.1 Current ScratchBird Datatype Capabilities

**✅ Strong Areas:**
```cpp
// From src/include/scratchbird/impl/dsc_pub.h
#define dtype_int128        24    // 128-bit integer (industry-leading)
#define dtype_dec64         22    // DECFLOAT(16) - IEEE 754-2008
#define dtype_dec128        23    // DECFLOAT(34) - IEEE 754-2008
#define dtype_json          30    // Native JSON support
#define dtype_uuid          29    // Native UUID support
#define dtype_boolean       21    // Boolean type
#define dtype_sql_time_tz   25    // Time with timezone
#define dtype_timestamp_tz  26    // Timestamp with timezone
```

**ScratchBird's Competitive Advantages:**
1. **128-bit Integer**: Exceeds PostgreSQL (no native 128-bit), Oracle (38-digit NUMBER), MySQL/MariaDB (64-bit max)
2. **IEEE 754-2008 Decimal**: Full compliance, better than MySQL's legacy DECIMAL
3. **Timezone Support**: Native timezone-aware temporal types
4. **JSON**: Native support ahead of many enterprise databases

### 1.2 Missing Datatypes (High Priority)

#### 1.2.1 Network Address Types
**Status:** ❌ **Missing**  
**Complexity:** 🟡 **Medium**  
**Business Value:** 🟢 **High** (IoT, networking applications)

**Implementation Required:**
```cpp
// New datatypes to add to dsc_pub.h
#define dtype_inet          31    // IPv4/IPv6 addresses
#define dtype_cidr          32    // Network blocks  
#define dtype_macaddr       33    // MAC addresses
```

**Files to Modify:**
- `src/include/scratchbird/impl/dsc_pub.h` - Add type definitions
- `src/jrd/cvt.cpp` - Add conversion routines
- `src/dsql/parse.y` - Add parser support
- `src/common/classes/` - Create InetAddress, CidrBlock, MacAddress classes

#### 1.2.2 Advanced Array Types
**Status:** ❌ **Missing**  
**Complexity:** 🔴 **High**  
**Business Value:** 🟡 **Medium** (PostgreSQL compatibility)

**Current:** Basic array support exists but limited
**Enhancement Needed:** Multi-dimensional arrays, array slicing, advanced operations

#### 1.2.3 Range Types
**Status:** ❌ **Missing**  
**Complexity:** 🟡 **Medium**  
**Business Value:** 🟡 **Medium** (PostgreSQL feature parity)

```sql
-- Target functionality
CREATE TABLE reservations (
    room INTEGER,
    during INT4RANGE
);
```

### 1.3 Enhanced String/Text Types

#### 1.3.1 Full-Text Search Integration
**Status:** 🟡 **Partial** (BLOB TEXT subtype exists)  
**Complexity:** 🔴 **High**  
**Business Value:** 🟢 **High**

**Current:** Text storage in BLOB, no native FTS
**Enhancement:** Native `TSVECTOR` and `TSQUERY` equivalents

#### 1.3.2 Case-Insensitive Text Type
**Status:** ❌ **Missing**  
**Complexity:** 🟢 **Low**  
**Business Value:** 🟢 **High**

```cpp
#define dtype_citext        34    // Case-insensitive text
```

---

## Part II: Index System Analysis

### 2.1 Current ScratchBird Index Capabilities

**✅ Strong Foundation:**
- B-Tree indexes (primary workhorse)
- Unique indexes with constraints
- Composite indexes
- Partial indexes (conditional)
- Expression indexes (computed columns)

**From Analysis:**
```cpp
// src/jrd/idx.h - Current index support
- Single and compound B-Tree indexes
- Ascending/Descending support  
- Unique constraint indexes
- Expression-based indexes
```

### 2.2 Missing Index Types (Priority Ranking)

#### 2.2.1 Hash Indexes
**Status:** ❌ **Missing**  
**Complexity:** 🟡 **Medium**  
**Business Value:** 🟢 **High** (O(1) equality lookups)

**Implementation Plan:**
```cpp
// New index type to add
enum IndexType {
    idx_btree = 0,     // Existing
    idx_hash = 1,      // NEW: Hash indexes for equality
    idx_bitmap = 2,    // NEW: Bitmap for low cardinality
    idx_gin = 3        // NEW: Generalized Inverted (JSON/arrays)
};
```

**Files to Modify:**
- `src/jrd/idx.h` - Add hash index definitions
- `src/jrd/btr.cpp` - Hash index implementation 
- `src/dsql/parse.y` - `CREATE INDEX ... USING HASH`
- `src/jrd/optimizer/` - Query planner integration

#### 2.2.2 Bitmap Indexes  
**Status:** ❌ **Missing**  
**Complexity:** 🔴 **High**  
**Business Value:** 🟡 **Medium** (OLAP workloads)

**Use Case:** Low-cardinality columns (gender, status, category)
**Performance:** Excellent for `WHERE status IN ('active', 'pending')` queries

#### 2.2.3 GIN (Generalized Inverted) Indexes
**Status:** ❌ **Missing**  
**Complexity:** 🔴 **High**  
**Business Value:** 🟢 **High** (JSON queries, full-text search)

**Critical for:**
- JSON path queries: `WHERE json_col->>'name' = 'John'`
- Array contains: `WHERE array_col @> ARRAY['value']`
- Full-text search

#### 2.2.4 Spatial Indexes (R-Tree)
**Status:** ❌ **Missing**  
**Complexity:** 🔴 **Very High**  
**Business Value:** 🟡 **Medium** (GIS applications)

**Implementation:** Requires full spatial datatypes first

---

## Part III: Implementation Roadmap

### Phase 1: Foundation Enhancement (Months 1-3)
**Priority:** 🔴 **Critical**

1. **Network Address Types**
   - Implement `INET`, `CIDR`, `MACADDR` datatypes
   - Add parsing, validation, conversion routines
   - Create operator functions (contains, overlaps, etc.)

2. **Hash Indexes**
   - Implement hash table data structure
   - Add query planner optimization rules
   - Create maintenance routines

3. **Case-Insensitive Text**
   - Add `CITEXT` datatype with collation support
   - Implement comparison operators
   - Add index support

### Phase 2: Advanced Features (Months 4-6)
**Priority:** 🟡 **High**

1. **GIN Indexes for JSON**
   - Implement inverted index structure
   - Add JSON path query optimization
   - Create operator classes for JSON types

2. **Enhanced Array Support**
   - Multi-dimensional array slicing
   - Array operators (`@>`, `<@`, `&&`)
   - Array aggregation functions

3. **Range Types**
   - Implement basic range datatypes (`INT4RANGE`, `TSRANGE`)
   - Add range operators and functions
   - Create range index support

### Phase 3: Enterprise Features (Months 7-12)
**Priority:** 🟢 **Medium**

1. **Bitmap Indexes**
   - Implement bitmap data structure
   - Add bitmap combining operations
   - Create OLAP query optimizations

2. **Full-Text Search**
   - Implement `TSVECTOR` equivalent
   - Add text search configuration
   - Create ranking and highlighting functions

3. **Spatial Support**
   - Basic geometry datatypes
   - R-Tree index implementation
   - Spatial operators and functions

---

## Part IV: Technical Implementation Details

### 4.1 Core File Modifications Required

#### 4.1.1 Datatype System Files
```
src/include/scratchbird/impl/dsc_pub.h     - Add new dtype constants
src/jrd/cvt.cpp                            - Type conversion routines  
src/jrd/evl.cpp                            - Expression evaluation
src/dsql/parse.y                           - SQL parser grammar
src/common/classes/                        - New datatype classes
src/jrd/mov.cpp                            - Data movement operations
```

#### 4.1.2 Index System Files
```
src/jrd/idx.h                              - Index type definitions
src/jrd/btr.cpp                            - B-Tree implementation
src/jrd/idx.cpp                            - Index management
src/jrd/optimizer/                         - Query optimization
src/dsql/DdlNodes.epp                      - DDL node handling
```

#### 4.1.3 SQL Parser Enhancements
```sql
-- New syntax to support
CREATE INDEX idx_name ON table USING HASH (column);
CREATE INDEX idx_name ON table USING GIN (json_column);
CREATE TABLE test (
    ip_addr INET,
    network CIDR,
    mac_addr MACADDR,
    name CITEXT
);
```

### 4.2 Storage Format Considerations

#### 4.2.1 On-Disk Structure (ODS Version Impact)
- New datatypes require ODS version increment
- Backward compatibility maintenance required
- Storage optimization for new types

#### 4.2.2 Memory Representation
```cpp
// Example network address storage
struct InetAddr {
    uint8_t family;          // IPv4 or IPv6
    uint8_t bits;           // CIDR bits
    union {
        uint32_t ipv4;
        uint8_t ipv6[16];
    } addr;
};
```

### 4.3 Performance Considerations

#### 4.3.1 Index Performance Targets
- Hash indexes: O(1) equality lookups
- GIN indexes: < 100ms for JSON path queries on 1M+ records
- Bitmap indexes: 10x improvement for low-cardinality analytics

#### 4.3.2 Memory Usage
- Hash indexes: ~1.5x storage overhead vs B-Tree
- GIN indexes: ~2-3x overhead for comprehensive JSON indexing
- Bitmap indexes: Minimal overhead for low-cardinality data

---

## Part V: Migration and Compatibility

### 5.1 Backward Compatibility
**Critical Requirement:** All existing ScratchBird applications must continue to work

**Strategy:**
1. New datatypes use separate dtype constants (31+)
2. Existing index types remain unchanged  
3. ODS versioning protects older databases
4. Optional migration utilities for new features

### 5.2 Standards Compliance
**Target Standards:**
- SQL:2016 compliance for JSON features
- PostgreSQL compatibility for network types
- Oracle compatibility for advanced numeric operations

### 5.3 Documentation Requirements
1. **Developer Documentation**
   - New datatype reference
   - Index selection guidelines
   - Migration procedures

2. **User Documentation**  
   - SQL syntax examples
   - Performance tuning guides
   - Best practices for new features

---

## Part VI: Success Metrics

### 6.1 Performance Benchmarks
- **Hash Index Performance:** 5-10x improvement for equality queries
- **JSON Query Performance:** 50x improvement with GIN indexes
- **Network Query Performance:** Native operations 100x faster than string manipulation

### 6.2 Feature Completeness
- **PostgreSQL Parity:** 80% of commonly-used PostgreSQL datatypes
- **Enterprise Readiness:** Network, JSON, and advanced text features
- **Modern Standards:** Full SQL:2016 JSON compliance

### 6.3 Adoption Metrics
- **Migration Tools:** Support for 95% of common database schemas
- **Documentation:** Complete coverage of all new features
- **Community Feedback:** Positive reception from early adopters

---

## Conclusion

ScratchBird's current foundation is exceptionally strong, with industry-leading 128-bit integer support and modern IEEE 754-2008 decimal types. The proposed enhancements focus on three key areas:

1. **Network/Infrastructure Support** - Critical for modern IoT and cloud applications
2. **Advanced Indexing** - Essential for high-performance query optimization  
3. **JSON/NoSQL Integration** - Required for hybrid relational/document workflows

**Total Estimated Effort:** 12-18 months with 2-3 senior database engineers
**Risk Level:** Medium (building on solid Firebird foundation)
**Business Impact:** High (positions ScratchBird as PostgreSQL alternative with unique advantages)

The roadmap prioritizes practical, high-value features that leverage ScratchBird's existing strengths while addressing the most common developer requirements in modern database applications.