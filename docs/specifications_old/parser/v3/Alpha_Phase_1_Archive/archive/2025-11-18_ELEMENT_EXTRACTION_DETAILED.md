# Element Extraction Operations Analysis for ScratchBird

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

**Date**: November 18, 2025
**Analysis Level**: Very Thorough
**Purpose**: Comprehensive audit of extraction operations for composite data types

---

## Executive Summary

This report examines element extraction capabilities across all composite data types in ScratchBird. The analysis reveals a **partial implementation** with strong foundational infrastructure but missing high-level SQL function interfaces for many operations.

**Key Finding**: While low-level extraction methods exist in the type system, most are not exposed through SQL EXTRACT/DATE_PART functions or operators.

---

## 1. UUID Operations

### 1.1 Implemented ✅

**Location**: `/home/user/ScratchBird/include/scratchbird/core/types.h` (lines 774-778)

**C++ API Available**:
```cpp
class TypeExtractor {
    static auto extractUUIDVersion(const std::vector<uint8_t> &uuid) -> int32_t;
    static auto extractUUIDVariant(const std::vector<uint8_t> &uuid) -> int32_t;
    static auto extractUUIDTimestamp(const std::vector<uint8_t> &uuid,
                                     ErrorContext *ctx = nullptr) -> std::optional<int64_t>;
};
```

**Implementation**: `/home/user/ScratchBird/src/core/types.cpp` (lines 2043-2055)
- ✅ `extractUUIDVersion()` - Extracts version field (bits 4-7 of byte 6)
- ✅ `extractUUIDVariant()` - Extracts variant field (bits 6-7 of byte 8)
- ⚠️ `extractUUIDTimestamp()` - **Declared but NOT implemented**

### 1.2 Missing ❌

**SQL Functions**: None exposed
- ❌ No `EXTRACT(VERSION FROM uuid_column)` support
- ❌ No `EXTRACT(VARIANT FROM uuid_column)` support  
- ❌ No `EXTRACT(TIMESTAMP FROM uuid_column)` support
- ❌ No opcodes for UUID extraction in SBLR bytecode
- ❌ No expression evaluator support

**Impact**: UUID component extraction requires C++ API, not usable in SQL queries.

---

## 2. DateTime Operations

### 2.1 Implemented ✅

**Location**: `/home/user/ScratchBird/include/scratchbird/core/types.h` (lines 754-772)

**C++ API Available**:
```cpp
class TypeExtractor {
    // Date extraction (from days_since_epoch)
    static auto extractYear(int64_t days_since_epoch) -> int32_t;
    static auto extractMonth(int64_t days_since_epoch) -> int32_t;
    static auto extractDay(int64_t days_since_epoch) -> int32_t;
    static auto extractDayOfWeek(int64_t days_since_epoch) -> int32_t;
    static auto extractDayOfYear(int64_t days_since_epoch) -> int32_t;
    
    // Time extraction (from microseconds)
    static auto extractHour(int64_t microseconds) -> int32_t;
    static auto extractMinute(int64_t microseconds) -> int32_t;
    static auto extractSecond(int64_t microseconds) -> int32_t;
    static auto extractMicrosecond(int64_t microseconds) -> int32_t;
    
    // Timestamp extraction (declared but NOT implemented)
    static auto extractTimestampYear(int64_t microseconds_since_epoch) -> int32_t;
    static auto extractTimestampMonth(int64_t microseconds_since_epoch) -> int32_t;
    static auto extractTimestampDay(int64_t microseconds_since_epoch) -> int32_t;
    static auto extractTimestampHour(int64_t microseconds_since_epoch) -> int32_t;
    static auto extractTimestampMinute(int64_t microseconds_since_epoch) -> int32_t;
    static auto extractTimestampSecond(int64_t microseconds_since_epoch) -> int32_t;
    static auto extractTimestampMicrosecond(int64_t microseconds_since_epoch) -> int32_t;
};
```

**Implementation**: `/home/user/ScratchBird/src/core/types.cpp` (lines 1987-2041)
- ✅ `extractYear()` - Full implementation with Proleptic Gregorian calendar
- ✅ `extractMonth()` - Full implementation
- ✅ `extractDay()` - Full implementation
- ✅ `extractHour()` - Full implementation
- ✅ `extractMinute()` - Full implementation
- ✅ `extractSecond()` - Full implementation
- ✅ `extractMicrosecond()` - Full implementation
- ⚠️ `extractDayOfWeek()` - **Declared but NOT implemented**
- ⚠️ `extractDayOfYear()` - **Declared but NOT implemented**
- ⚠️ All `extractTimestamp*()` functions - **Declared but NOT implemented**

### 2.2 Missing ❌

**SQL Functions**: None exposed
- ❌ No `EXTRACT(YEAR FROM date_column)` support
- ❌ No `EXTRACT(MONTH FROM timestamp_column)` support
- ❌ No `DATE_PART('hour', timestamp_column)` support
- ❌ No opcodes for DATE extraction in SBLR bytecode
- ❌ No expression evaluator support

**PostgreSQL Standard Functions Missing**:
- ❌ `EXTRACT(field FROM source)`
- ❌ `DATE_PART(field, source)`
- ❌ `DATE_TRUNC(field, source)`
- ❌ Timezone extraction

**Impact**: DateTime component extraction requires C++ API, severely limiting query capabilities.

---

## 3. JSON Operations

### 3.1 Implemented ✅

**Location**: `/home/user/ScratchBird/include/scratchbird/core/jsonb.h` (lines 73-79)

**C++ API Available**:
```cpp
class JSONBValue {
    auto operator[](const std::string& key) const -> std::optional<JSONBValue>;
    auto operator[](size_t index) const -> std::optional<JSONBValue>;
    auto getPath(const std::string& path) const -> std::optional<JSONBValue>;
};

class JSONB {
    static auto getPath(const std::vector<uint8_t>& binary, const std::string& path)
        -> std::optional<JSONBValue>;
};
```

**SQL Operators - IMPLEMENTED** ✅:
- ✅ `->` operator (JSON_ARROW) - Returns JSON
- ✅ `->>` operator (JSON_DOUBLE_ARROW) - Returns text
- ✅ `#>` operator (JSON_HASH_ARROW) - Path array, returns JSON
- ✅ `#>>` operator (JSON_HASH_DOUBLE_ARROW) - Path array, returns text

**SQL Functions - IMPLEMENTED** ✅:
- ✅ `JSON_EXTRACT(json, path)` - Opcode 0xEA
- ✅ `JSONB_EXTRACT_PATH(jsonb, path_elem...)` - Opcode 0xEB

**Implementation**: 
- Opcodes: `/home/user/ScratchBird/include/scratchbird/sblr/opcodes.h` (lines 226-231)
- Executor: `/home/user/ScratchBird/src/sblr/executor.cpp` (lines 7841-7970)

**Example Usage**:
```sql
SELECT data->'user'->'name' FROM users;
SELECT data->>'email' FROM users;
SELECT data#>'{address,city}' FROM users;
```

### 3.2 Assessment

**Status**: ✅ **FULLY IMPLEMENTED AND PRODUCTION READY**

JSON extraction is the most complete extraction subsystem with:
- Full operator support (`->`, `->>`, `#>`, `#>>`)
- Function support (`JSON_EXTRACT`, `JSONB_EXTRACT_PATH`)
- Both path-based and key-based access
- Array index access
- Deep path traversal

---

## 4. Array Operations

### 4.1 Implemented ✅

**Location**: `/home/user/ScratchBird/include/scratchbird/core/array.h` (lines 47-56)

**C++ API Available**:
```cpp
class ArrayValue {
    auto getElement(size_t flat_index) const -> std::optional<Element>;
    auto at(const std::vector<size_t>& indices) const -> std::optional<Element>;
    auto slice(const std::vector<std::pair<size_t, size_t>>& ranges) const 
        -> std::optional<ArrayValue>;
};
```

**SQL Functions - IMPLEMENTED** ✅:
- ✅ `ARRAY_LENGTH(array, dimension)` - Get array length (EXT_ARRAY_LENGTH)
- ✅ `ARRAY_DIMS(array)` - Get dimensions as text (EXT_ARRAY_DIMS)
- ✅ `ARRAY_UPPER(array, dimension)` - Upper bound (EXT_ARRAY_UPPER)
- ✅ `ARRAY_LOWER(array, dimension)` - Lower bound (EXT_ARRAY_LOWER)

**Implementation**: 
- Opcodes: `/home/user/ScratchBird/include/scratchbird/sblr/opcodes.h` (lines 280-284)
- Documented: `/home/user/ScratchBird/docs/status_archive/phase_completions/TASK_12_ARRAY_FUNCTIONS_COMPLETE.md`

### 4.2 Missing ❌

**Subscript Operator**: 
- ❌ No `array[index]` subscript operator for element access
- ❌ No `array[start:end]` slice operator

**Impact**: Element access requires function calls instead of natural subscript syntax.

### 4.3 Assessment

**Status**: ✅ **MOSTLY COMPLETE** (95%)

Array bounds and metadata extraction fully implemented via functions. Only missing convenient subscript operator syntax.

---

## 5. Composite Type Operations

### 5.1 Implemented ✅

**Location**: `/home/user/ScratchBird/include/scratchbird/core/types.h` (lines 543-547)

**C++ API Available**:
```cpp
class TypedValue {
    TypedValue getField(const std::string& field_name) const;
    bool hasField(const std::string& field_name) const;
    size_t getFieldCount() const;
    const std::vector<std::string>& getFieldNames() const;
};
```

**Implementation**: `/home/user/ScratchBird/src/core/types.cpp` (lines 669-718)
- ✅ `getField()` - Field access by name with runtime error on missing field
- ✅ `hasField()` - Field existence check
- ✅ `getFieldCount()` - Number of fields
- ✅ `getFieldNames()` - All field names

**Composite Structure**:
```cpp
struct CompositeValue {
    std::vector<std::string> field_names;
    std::vector<std::shared_ptr<TypedValue>> field_values;
};
```

### 5.2 Missing ❌

**SQL Access**:
- ❌ No `composite_value.field_name` dot notation
- ❌ No `(composite_value).field_name` parenthesized access
- ❌ No opcodes for composite field extraction
- ❌ No expression evaluator support

**PostgreSQL-style Access**:
```sql
-- Not supported:
SELECT (person).name FROM employees;
SELECT person.age FROM employees;
```

### 5.3 Assessment

**Status**: ⚠️ **PARTIAL** (40%)

Full C++ API but no SQL-level access mechanism.

---

## 6. Range Operations

### 6.1 Implemented ✅

**Location**: `/home/user/ScratchBird/include/scratchbird/core/range.h` (lines 123-132)

**C++ API Available**:
```cpp
template <typename T>
class Range {
    bool isEmpty() const;
    std::optional<T> lower() const;
    std::optional<T> upper() const;
    BoundType lowerBoundType() const;
    BoundType upperBoundType() const;
    bool isLowerInclusive() const;
    bool isUpperInclusive() const;
    bool isLowerBounded() const;
    bool isUpperBounded() const;
};
```

**SQL Functions - IMPLEMENTED** ✅:
- ✅ `LOWER(range)` - Get lower bound (EXT_RANGE_LOWER)
- ✅ `UPPER(range)` - Get upper bound (EXT_RANGE_UPPER)
- ✅ `ISEMPTY(range)` - Check if empty (EXT_RANGE_ISEMPTY)
- ✅ `LOWER_INC(range)` - Lower bound inclusive? (EXT_RANGE_LOWER_INC)
- ✅ `UPPER_INC(range)` - Upper bound inclusive? (EXT_RANGE_UPPER_INC)
- ✅ `LOWER_INF(range)` - Lower bound infinite? (EXT_RANGE_LOWER_INF)
- ✅ `UPPER_INF(range)` - Upper bound infinite? (EXT_RANGE_UPPER_INF)
- ✅ `RANGE_MERGE(r1, r2)` - Smallest range containing both (EXT_RANGE_MERGE)

**Implementation**:
- Helper Functions: `/home/user/ScratchBird/include/scratchbird/core/range_functions.h` (lines 23-135)
- Opcodes: `/home/user/ScratchBird/include/scratchbird/sblr/opcodes.h` (lines 467-474)

### 6.2 Assessment

**Status**: ✅ **FULLY IMPLEMENTED** (100%)

Complete range bound extraction with both C++ API and SQL functions.

---

## 7. Network Type Operations

### 7.1 Implemented ✅

**Location**: `/home/user/ScratchBird/include/scratchbird/core/network.h`

**C++ API Available** (INET):
```cpp
class InetAddr {
    AddressFamily family() const;           // IPv4 or IPv6
    uint8_t netmask() const;                // Netmask bits
    bool isIPv4() const;
    bool isIPv6() const;
    
    InetAddr network() const;               // Network address (host bits zeroed)
    InetAddr broadcast() const;             // Broadcast address
    InetAddr netmaskAddr() const;           // Netmask as address (e.g., 255.255.255.0)
    InetAddr hostmask() const;              // Hostmask (inverse of netmask)
    
    std::string toString() const;
    std::string toStringWithoutNetmask() const;
    std::string toAbbreviated() const;      // Abbreviated form
};
```

**C++ API Available** (MACADDR):
```cpp
class MacAddr {
    const std::array<uint8_t, 6>& bytes() const;
    uint8_t operator[](size_t index) const;
    
    MacAddr trunc() const;                  // Truncate to manufacturer ID (first 3 bytes)
    
    std::string toString() const;           // Colon-separated
    std::string toStringHyphen() const;     // Hyphen-separated
    std::string toStringCisco() const;      // Cisco format
};
```

### 7.2 Missing ❌

**SQL Functions**: None exposed
- ❌ No `HOST(inet)` function
- ❌ No `NETMASK(inet)` function
- ❌ No `NETWORK(inet)` function
- ❌ No `BROADCAST(inet)` function
- ❌ No `ABBREV(inet)` function
- ❌ No `FAMILY(inet)` function
- ❌ No `MASKLEN(inet)` function
- ❌ No `TRUNC(macaddr)` function

**PostgreSQL Standard Functions Missing**:
```sql
-- Not supported:
SELECT HOST(ip_address) FROM network_log;
SELECT NETMASK(subnet) FROM routing_table;
SELECT FAMILY(ip_address) FROM connections;
SELECT TRUNC(mac_address) FROM devices;
```

### 7.3 Assessment

**Status**: ⚠️ **PARTIAL** (30%)

Comprehensive C++ API but no SQL function exposure. All extraction operations require application-level code.

---

## 8. Spatial Operations

### 8.1 Implemented ✅

**Location**: `/home/user/ScratchBird/include/scratchbird/core/types.h` (lines 182-298)

**C++ API Available** (Point):
```cpp
struct Point {
    double x;
    double y;
    int32_t srid;
    
    int32_t getSRID() const;
    void setSRID(int32_t new_srid);
    bool hasSRID() const;
};
```

**Direct Field Access**: 
- ✅ `point.x` - X coordinate (public field)
- ✅ `point.y` - Y coordinate (public field)
- ✅ `point.srid` - Spatial Reference ID (public field)

**C++ API Available** (LineString/Polygon):
```cpp
struct LineString {
    std::vector<Point> points;    // Direct access to points
    int32_t getSRID() const;
};

struct Polygon {
    std::vector<std::vector<Point>> rings;
    const std::vector<Point>& exteriorRing() const;
    size_t numInteriorRings() const;
    const std::vector<Point>& interiorRing(size_t index) const;
    int32_t getSRID() const;
};
```

### 8.2 Missing ❌

**SQL Functions**: None exposed
- ❌ No `ST_X(point)` function
- ❌ No `ST_Y(point)` function
- ❌ No `ST_Z(point)` function (3D not supported)
- ❌ No `ST_M(point)` function (measure not supported)
- ❌ No `ST_SRID(geometry)` function accessible from SQL
- ❌ No `ST_NumPoints(linestring)` function
- ❌ No `ST_PointN(linestring, n)` function
- ❌ No `ST_NumInteriorRings(polygon)` function
- ❌ No `ST_ExteriorRing(polygon)` function
- ❌ No `ST_InteriorRingN(polygon, n)` function

**PostGIS Standard Functions Missing**:
```sql
-- Not supported:
SELECT ST_X(location), ST_Y(location) FROM places;
SELECT ST_SRID(geometry) FROM spatial_data;
SELECT ST_PointN(path, 1) FROM routes;
SELECT ST_ExteriorRing(boundary) FROM parcels;
```

### 8.3 Assessment

**Status**: ⚠️ **PARTIAL** (25%)

Spatial types exist with accessible fields in C++, but no SQL extraction functions. GEOS wrapper provides some operations, but coordinate/component extraction not exposed to SQL.

---

## 9. VECTOR Operations (Embeddings)

### 9.1 Implemented ✅

**Location**: `/home/user/ScratchBird/include/scratchbird/core/types.h` (lines 549-559)

**C++ API Available**:
```cpp
class TypedValue {
    TypedValue getVectorElement(size_t index) const;
    TypedValue getVectorSlice(size_t start, size_t end) const;
    size_t getVectorDimensions() const;
    
    // Distance operators
    TypedValue vectorDistance(const TypedValue& other, DistanceMetric metric) const;
    TypedValue vectorEuclideanDistance(const TypedValue& other) const;
    TypedValue vectorManhattanDistance(const TypedValue& other) const;
    TypedValue vectorCosineSimilarity(const TypedValue& other) const;
    TypedValue vectorDotProduct(const TypedValue& other) const;
};
```

**Implementation**: `/home/user/ScratchBird/src/core/types.cpp` (lines 727-809)
- ✅ `getVectorElement()` - Element access by index
- ✅ `getVectorSlice()` - Slice extraction
- ✅ `getVectorDimensions()` - Dimension count
- ✅ All distance operations implemented

**VectorValue Class**: `/home/user/ScratchBird/include/scratchbird/core/vector.h`
- ✅ `getFloat32(index)` - Element access
- ✅ `getFloat64(index)` - Element access
- ✅ `getAsFloat64(index)` - Universal element access

### 9.2 Missing ❌

**SQL Access**:
- ❌ No `vector[index]` subscript operator
- ❌ No `vector[start:end]` slice operator
- ❌ No opcodes for vector element access
- ❌ No expression evaluator support

**SQL Functions**:
```sql
-- Not supported:
SELECT embedding[1] FROM documents;
SELECT embedding[1:10] FROM documents;
SELECT vector_dims(embedding) FROM documents;
```

### 9.3 Assessment

**Status**: ⚠️ **PARTIAL** (60%)

Full C++ API for element access and slicing. Distance operations fully implemented. Missing SQL-level subscript syntax.

---

## 10. Summary Matrix

| Data Type | C++ API | SQL Functions | SQL Operators | Completeness |
|-----------|---------|---------------|---------------|--------------|
| **UUID** | ✅ Partial | ❌ None | ❌ None | 20% |
| **DateTime** | ✅ Partial | ❌ None | ❌ None | 30% |
| **JSON/JSONB** | ✅ Full | ✅ Full | ✅ Full | 100% |
| **Array** | ✅ Full | ✅ Full | ⚠️ No subscript | 95% |
| **Composite** | ✅ Full | ❌ None | ❌ None | 40% |
| **Range** | ✅ Full | ✅ Full | ✅ Full | 100% |
| **Network (INET)** | ✅ Full | ❌ None | ❌ None | 30% |
| **Network (MAC)** | ✅ Full | ❌ None | ❌ None | 30% |
| **Spatial** | ✅ Full | ❌ None | ❌ None | 25% |
| **Vector** | ✅ Full | ❌ None | ❌ None | 60% |

---

## 11. Gap Analysis

### 11.1 Critical Gaps 🔴

**1. DateTime EXTRACT/DATE_PART Functions**
- **Impact**: HIGH - Core SQL functionality missing
- **Effort**: Medium (2-3 days)
- **Requirement**: 
  - Implement EXTRACT opcode
  - Add parser support for EXTRACT(field FROM source)
  - Wire TypeExtractor methods to executor
  - Complete missing extractTimestamp*() implementations

**2. Spatial Coordinate Extraction (ST_X, ST_Y)**
- **Impact**: HIGH - Essential PostGIS compatibility
- **Effort**: Small (1 day)
- **Requirement**:
  - Add ST_X, ST_Y opcodes
  - Simple field access to Point.x, Point.y
  - Executor integration

**3. Composite Field Access Syntax**
- **Impact**: HIGH - PostgreSQL compatibility
- **Requirement**:
  - Parser support for `(composite).field` syntax
  - Expression evaluator integration
  - Consider dot notation support

### 11.2 Important Gaps 🟡

**4. Network Type SQL Functions**
- **Impact**: MEDIUM
- **Effort**: Medium (2 days)
- **Functions**: HOST(), NETMASK(), NETWORK(), BROADCAST(), FAMILY(), MASKLEN()

**5. UUID Timestamp Extraction**
- **Impact**: MEDIUM (UUIDv7 specific)
- **Effort**: Small (0.5 day)
- **Requirement**: Implement extractUUIDTimestamp() for UUIDv7

**6. Array/Vector Subscript Operators**
- **Impact**: MEDIUM - Syntax convenience
- **Effort**: Medium (1-2 days)
- **Requirement**: Parser and operator support for `array[index]`, `vector[index]`

### 11.3 Nice-to-Have 🟢

**7. Additional Spatial Accessors**
- ST_NumPoints(), ST_PointN(), ST_ExteriorRing(), etc.
- Impact: LOW
- Effort: Small-Medium

**8. Complete DateTime Extractors**
- extractDayOfWeek(), extractDayOfYear()
- Impact: LOW
- Effort: Small

---

## 12. Implementation Recommendations

### Priority 1 (Alpha Blocker)
1. ✅ **EXTRACT/DATE_PART functions** - Core SQL requirement
2. ✅ **ST_X/ST_Y spatial extractors** - Essential PostGIS compatibility
3. ✅ **Composite field access** - PostgreSQL standard

### Priority 2 (Important)
4. **Network type functions** - INET/CIDR usability
5. **Array subscript operator** - Natural syntax
6. **UUID timestamp extraction** - Complete UUIDv7 support

### Priority 3 (Enhancement)
7. Additional spatial extractors
8. Vector subscript operator
9. Complete datetime helper extractors

---

## 13. Architectural Observations

### Strengths ✅
1. **Excellent Infrastructure**: TypeExtractor class provides clean abstraction
2. **Complete JSON System**: Best-in-class implementation with all operators
3. **Full Range Support**: Complete PostgreSQL-compatible range functions
4. **Comprehensive C++ APIs**: All types have proper accessor methods

### Weaknesses ❌
1. **SQL Integration Gap**: Most C++ methods not exposed to SQL
2. **Missing Opcodes**: No EXTRACT, no coordinate extraction opcodes
3. **Parser Gaps**: No EXTRACT syntax, no composite dot notation
4. **Incomplete Implementations**: Several declared functions not implemented

### Design Pattern Observation
The codebase follows a pattern:
1. Define type in `types.h`
2. Implement C++ methods in `types.cpp`
3. **[MISSING]** Add opcodes to `opcodes.h`
4. **[MISSING]** Wire to executor in `executor.cpp`
5. **[MISSING]** Add parser support in `parser.cpp`

**Recommendation**: Follow the JSON/Range models which completed all 5 steps successfully.

---

## 14. Testing Requirements

For each implemented extraction operation, ensure:
- ✅ Unit tests for C++ API methods
- ⚠️ Integration tests for SQL functions (missing for most)
- ⚠️ Edge case tests (NULL handling, out of bounds, type errors)
- ⚠️ Performance tests for large datasets

**Current Test Coverage**:
- JSON: ✅ Comprehensive
- Arrays: ✅ Comprehensive
- Ranges: ✅ Good
- DateTime: ❌ None for extraction
- Spatial: ❌ None for coordinate extraction
- Network: ❌ None
- UUID: ❌ None

---

## 15. Conclusion

ScratchBird has **excellent foundational infrastructure** for element extraction with comprehensive C++ APIs across all composite types. However, **SQL-level exposure is incomplete**, with only JSON and Range types offering full functionality.

**Overall Completeness**: ~50%

**Recommended Action**: Prioritize EXTRACT/DATE_PART implementation and spatial coordinate extractors to achieve SQL Standard and PostGIS compatibility. Use JSON and Range implementations as reference patterns.

---

**Analysis Completed**: November 18, 2025
**Analyst**: Claude Code Agent
**Methodology**: Very Thorough Code Search + API Analysis
