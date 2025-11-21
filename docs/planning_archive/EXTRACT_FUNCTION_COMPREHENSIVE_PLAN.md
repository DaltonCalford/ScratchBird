# EXTRACT Function - Comprehensive Implementation Plan

**Date:** November 19, 2025
**Task:** Implement EXTRACT(field FROM value) SQL function for all data types with sub-information

---

## Executive Summary

ScratchBird has **TypeExtractor** class with extraction functions, but they are not exposed as SQL functions. This plan implements the EXTRACT() and DATE_PART() SQL functions to provide access to sub-information for all complex data types.

**Goal:** `SELECT EXTRACT(year FROM date_col), EXTRACT(x FROM point_col), EXTRACT(version FROM uuid_col)`

---

## Data Types With Sub-Information

### 1. DATE ✅ (Functions Exist, Need SQL Integration)
**Type:** Temporal
**Storage:** int64_t (days since epoch)
**Extractable Fields:**
- `year` → int32 (e.g., 2025)
- `month` → int32 (1-12)
- `day` → int32 (1-31)
- `dow` (day_of_week) → int32 (0=Sunday, 6=Saturday)
- `doy` (day_of_year) → int32 (1-366)
- `quarter` → int32 (1-4) *needs implementation*
- `week` → int32 (ISO week number) *needs implementation*
- `epoch` → int64 (seconds since epoch)

**Existing Functions:** ✅ extractYear, extractMonth, extractDay, extractDayOfWeek, extractDayOfYear

---

### 2. TIME ✅ (Functions Exist, Need SQL Integration)
**Type:** Temporal
**Storage:** int64_t (microseconds since midnight)
**Extractable Fields:**
- `hour` → int32 (0-23)
- `minute` → int32 (0-59)
- `second` → int32 (0-59)
- `microsecond` → int32 (0-999999)
- `millisecond` → int32 (0-999) *needs implementation*
- `epoch` → int64 (seconds since midnight)

**Existing Functions:** ✅ extractHour, extractMinute, extractSecond, extractMicrosecond

---

### 3. TIMESTAMP ✅ (Functions Exist, Need SQL Integration)
**Type:** Temporal
**Storage:** int64_t (microseconds since epoch) + optional timezone
**Extractable Fields:**
- `year` → int32
- `month` → int32
- `day` → int32
- `hour` → int32
- `minute` → int32
- `second` → int32
- `microsecond` → int32
- `millisecond` → int32 *needs implementation*
- `dow` (day_of_week) → int32
- `doy` (day_of_year) → int32
- `quarter` → int32 *needs implementation*
- `week` → int32 *needs implementation*
- `epoch` → int64 (seconds since epoch)
- `timezone` → varchar (if TIMESTAMPTZ) *needs implementation*
- `timezone_hour` → int32 *needs implementation*
- `timezone_minute` → int32 *needs implementation*

**Existing Functions:** ✅ extractTimestampYear/Month/Day/Hour/Minute/Second/Microsecond

---

### 4. INTERVAL ⚠️ (Needs Implementation)
**Type:** Temporal
**Storage:** struct { int32 months, int32 days, int64 microseconds }
**Extractable Fields:**
- `year` → int32 (months / 12)
- `month` → int32 (months % 12)
- `day` → int32
- `hour` → int32
- `minute` → int32
- `second` → int32
- `microsecond` → int32
- `epoch` → double (total seconds as fractional)

**Existing Functions:** ❌ None - Need to implement all

---

### 5. UUID ✅ (Functions Exist, Need SQL Integration)
**Type:** Special
**Storage:** 16 bytes (128-bit)
**Extractable Fields:**
- `version` → int32 (1-7, extracted from bits 48-51)
- `variant` → int32 (0-2, extracted from bits 64-65)
- `timestamp` → int64 (for UUIDv1/v7, microseconds since epoch) *partially implemented*
- `node` → varchar (for UUIDv1, MAC address) *needs implementation*
- `clock_seq` → int32 (for UUIDv1, clock sequence) *needs implementation*

**Existing Functions:** ✅ extractUUIDVersion, extractUUIDVariant, extractUUIDTimestamp (partial)

---

### 6. INET ⚠️ (Needs Implementation)
**Type:** Network
**Storage:** class InetAddr { AddressFamily, uint8_t[16], uint8_t netmask }
**Extractable Fields:**
- `family` → int32 (4 for IPv4, 6 for IPv6)
- `netmask` → int32 (0-32 for IPv4, 0-128 for IPv6)
- `address` → varchar (IP address without netmask)
- `network` → inet (network address with host bits zeroed)
- `broadcast` → inet (broadcast address)
- `hostmask` → inet (inverse of netmask)

**Existing Functions:** ❌ None - Need to implement all (methods exist on InetAddr class)

---

### 7. CIDR ⚠️ (Needs Implementation)
**Type:** Network
**Storage:** Same as INET
**Extractable Fields:**
- `family` → int32
- `netmask` → int32
- `network` → cidr
- `broadcast` → inet

**Existing Functions:** ❌ None - Need to implement all

---

### 8. MACADDR ⚠️ (Needs Implementation)
**Type:** Network
**Storage:** 6 bytes (EUI-48)
**Extractable Fields:**
- `trunc` → macaddr (Set last 3 bytes to 0, for manufacturer lookup)
- `vendor` → varchar (OUI, first 3 bytes as hex) *needs implementation*

**Existing Functions:** ❌ None - Need to implement all

---

### 9. POINT ⚠️ (Needs Implementation)
**Type:** Spatial
**Storage:** struct { double x, double y, int32 srid }
**Extractable Fields:**
- `x` → float64
- `y` → float64
- `srid` → int32

**Existing Functions:** ❌ None - Trivial accessors needed

---

### 10. LINESTRING ⚠️ (Needs Implementation)
**Type:** Spatial
**Storage:** vector<Point> + int32 srid
**Extractable Fields:**
- `num_points` → int32
- `point_n` → point (requires index parameter, use different syntax)
- `start_point` → point
- `end_point` → point
- `length` → float64 (use ST_Length function instead)
- `srid` → int32

**Existing Functions:** ❌ None - Need to implement all

---

### 11. POLYGON ⚠️ (Needs Implementation)
**Type:** Spatial
**Storage:** vector<LineString> (exterior + holes) + int32 srid
**Extractable Fields:**
- `num_rings` → int32
- `exterior_ring` → linestring
- `num_interior_rings` → int32
- `interior_ring_n` → linestring (requires index)
- `area` → float64 (use ST_Area function instead)
- `perimeter` → float64 (use ST_Perimeter function instead)
- `srid` → int32

**Existing Functions:** ❌ None - Need to implement all

---

### 12. MULTIPOINT, MULTILINESTRING, MULTIPOLYGON, GEOMETRYCOLLECTION ⚠️
**Type:** Spatial
**Storage:** vector<Geometry> + int32 srid
**Extractable Fields:**
- `num_geometries` → int32 (use ST_NumGeometries instead)
- `geometry_n` → geometry (use ST_GeometryN instead)
- `srid` → int32

**Existing Functions:** ❌ None - Need to implement, but ST_NumGeometries and ST_GeometryN already exist

---

### 13. ARRAY ⚠️ (Needs Implementation)
**Type:** Composite
**Storage:** class ArrayValue { element_type, dimensions, data }
**Extractable Fields:**
- `cardinality` → int32 (total number of elements)
- `ndims` → int32 (number of dimensions / rank)
- `dims` → int32[] (array of dimension sizes) *needs special handling*
- `lower` → int32 (always 1 in PostgreSQL, lower bound of first dimension)
- `upper` → int32 (upper bound of first dimension)

**Note:** Element access uses subscript operator `array[index]`, not EXTRACT

**Existing Functions:** ✅ getRank, getTotalElements, getDimensions (on ArrayValue class)

---

### 14. RANGE (INT4RANGE, INT8RANGE, DATERANGE, TSRANGE, TSTZRANGE, NUMRANGE) ⚠️
**Type:** Composite
**Storage:** template<T> Range { optional<T> lower, optional<T> upper, BoundType, empty }
**Extractable Fields:**
- `lower` → T (lower bound value, NULL if unbounded)
- `upper` → T (upper bound value, NULL if unbounded)
- `lower_inc` → bool (lower bound inclusive)
- `upper_inc` → bool (upper bound inclusive)
- `lower_inf` → bool (lower bound infinite/unbounded)
- `upper_inf` → bool (upper bound infinite/unbounded)
- `isempty` → bool

**Existing Functions:** ✅ lower(), upper(), lowerInc(), upperInc() (on Range<T> class)

---

### 15. COMPOSITE (User-Defined) ⚠️
**Type:** Composite
**Storage:** class CompositeRecord { map<string, CompositeFieldValue> }
**Extractable Fields:**
- *field_name* → value (use composite.field_name syntax, not EXTRACT)

**Note:** Field access uses dot notation, not EXTRACT

---

## Implementation Strategy

### Phase 1: Core EXTRACT Infrastructure (4-6 hours)
- Add EXTRACT opcode to opcodes.h
- Implement parser for `EXTRACT(field FROM expression)` syntax
- Implement bytecode generator for EXTRACT
- Create executor dispatch for EXTRACT based on value type
- Add tests for parsing

**Files:**
- `include/scratchbird/sblr/opcodes.h`
- `src/parser/lexer.cpp` (EXTRACT keyword)
- `src/parser/parser.cpp` (parseExtractExpression)
- `src/sblr/bytecode_generator.cpp`
- `src/sblr/executor.cpp` (executeExtract)

---

### Phase 2: Temporal Type Extraction (2-3 hours)
Wire up existing TypeExtractor functions for DATE, TIME, TIMESTAMP:
- Map field names (year/month/day/hour/minute/second/microsecond) to TypeExtractor calls
- Add missing fields: quarter, week, millisecond, timezone info
- Add INTERVAL extraction (all fields)
- Add comprehensive tests

**Implementation:**
```cpp
// In executor.cpp
case Opcode::EXTRACT:
{
    uint8_t field_id = readByte();  // Field enum
    Value source = pop();

    if (source.type() == DataType::DATE) {
        int64_t days = source.getDate();
        switch (field_id) {
            case EXTRACT_YEAR:
                push(Value::makeInt32(TypeExtractor::extractYear(days)));
                break;
            case EXTRACT_MONTH:
                push(Value::makeInt32(TypeExtractor::extractMonth(days)));
                break;
            // ...
        }
    }
    // ...
}
```

---

### Phase 3: UUID Extraction (1-2 hours)
- Wire up existing extractUUIDVersion, extractUUIDVariant
- Implement extractUUIDNode, extractUUIDClockSeq
- Test with UUIDv1, UUIDv4, UUIDv7

---

### Phase 4: Network Type Extraction (2-3 hours)
- Implement extractINETFamily, extractINETNetmask, etc.
- Use existing InetAddr methods (family(), netmask(), network(), etc.)
- Implement CIDR and MACADDR extraction
- Add tests

---

### Phase 5: Spatial Type Extraction (3-4 hours)
- Implement POINT extraction (x, y, srid)
- Implement LINESTRING extraction (num_points, start_point, end_point, srid)
- Implement POLYGON extraction (num_rings, exterior_ring, srid)
- Multi-geometry extraction (delegate to ST_NumGeometries/ST_GeometryN for most)
- Add comprehensive tests

---

### Phase 6: Array and Range Extraction (2-3 hours)
- Implement ARRAY extraction (cardinality, ndims, dims, lower, upper)
- Implement RANGE extraction (lower, upper, lower_inc, upper_inc, etc.)
- Handle NULL bounds for unbounded ranges
- Add tests for all range types

---

### Phase 7: DATE_PART Alias (1 hour)
Implement DATE_PART as alias for EXTRACT:
```sql
DATE_PART('year', timestamp_col) ≡ EXTRACT(year FROM timestamp_col)
```

**Implementation:**
- Add DATE_PART function opcode (or reuse EXTRACT)
- Parse `DATE_PART(field, expression)` as function call
- Generate same bytecode as EXTRACT

---

## Field Name Enumeration

Add to opcodes.h or separate header:

```cpp
enum class ExtractField : uint8_t {
    // Temporal fields
    YEAR = 0,
    MONTH = 1,
    DAY = 2,
    HOUR = 3,
    MINUTE = 4,
    SECOND = 5,
    MICROSECOND = 6,
    MILLISECOND = 7,
    DOW = 8,          // day of week
    DOY = 9,          // day of year
    QUARTER = 10,
    WEEK = 11,
    EPOCH = 12,
    TIMEZONE = 13,
    TIMEZONE_HOUR = 14,
    TIMEZONE_MINUTE = 15,

    // UUID fields
    VERSION = 20,
    VARIANT = 21,
    TIMESTAMP = 22,
    NODE = 23,
    CLOCK_SEQ = 24,

    // Network fields
    FAMILY = 30,
    NETMASK = 31,
    ADDRESS = 32,
    NETWORK = 33,
    BROADCAST = 34,
    HOSTMASK = 35,
    VENDOR = 36,

    // Spatial fields
    X = 40,
    Y = 41,
    SRID = 42,
    NUM_POINTS = 43,
    START_POINT = 44,
    END_POINT = 45,
    NUM_RINGS = 46,
    EXTERIOR_RING = 47,
    NUM_INTERIOR_RINGS = 48,
    NUM_GEOMETRIES = 49,

    // Array fields
    CARDINALITY = 60,
    NDIMS = 61,
    DIMS = 62,
    LOWER = 63,
    UPPER = 64,

    // Range fields
    LOWER_INC = 70,
    UPPER_INC = 71,
    LOWER_INF = 72,
    UPPER_INF = 73,
    ISEMPTY = 74,
};
```

---

## Test Plan

### Temporal Tests (20 tests)
- EXTRACT(year/month/day/hour/minute/second/microsecond FROM date/time/timestamp)
- EXTRACT(quarter/week FROM date)
- EXTRACT(epoch FROM timestamp)
- EXTRACT(year/month/day/hour/minute/second FROM interval)
- INTERVAL with negative values
- Edge cases: leap years, DST transitions

### UUID Tests (8 tests)
- EXTRACT(version FROM uuid) for v1/v4/v7
- EXTRACT(variant FROM uuid)
- EXTRACT(timestamp FROM uuid) for v7
- Invalid UUID handling

### Network Tests (12 tests)
- EXTRACT(family/netmask FROM inet)
- EXTRACT(network/broadcast FROM inet) for IPv4/IPv6
- EXTRACT(family/netmask FROM cidr)
- EXTRACT(vendor FROM macaddr)

### Spatial Tests (15 tests)
- EXTRACT(x/y/srid FROM point)
- EXTRACT(num_points/start_point/end_point FROM linestring)
- EXTRACT(num_rings/exterior_ring FROM polygon)
- EXTRACT(srid FROM all geometry types)

### Array Tests (8 tests)
- EXTRACT(cardinality FROM array) for 1D/2D arrays
- EXTRACT(ndims FROM array)
- EXTRACT(lower/upper FROM array)

### Range Tests (10 tests)
- EXTRACT(lower/upper FROM int4range/int8range/daterange)
- EXTRACT(lower_inc/upper_inc FROM range)
- EXTRACT(isempty FROM range)
- Unbounded ranges (NULL lower/upper)

### DATE_PART Tests (5 tests)
- DATE_PART equivalence with EXTRACT
- Error handling

---

## Estimated Effort

| Phase | Hours | Priority |
|-------|-------|----------|
| Phase 1: Core Infrastructure | 4-6 | High |
| Phase 2: Temporal Types | 2-3 | High |
| Phase 3: UUID | 1-2 | Medium |
| Phase 4: Network Types | 2-3 | Medium |
| Phase 5: Spatial Types | 3-4 | Low |
| Phase 6: Array & Range | 2-3 | Medium |
| Phase 7: DATE_PART | 1 | Low |
| **Total** | **15-22 hours** | |

**Test Development:** ~10-15 hours (78+ tests)

**Grand Total:** ~25-37 hours

---

## Success Criteria

✅ All 15 data types with sub-information support EXTRACT
✅ 78+ tests passing
✅ PostgreSQL-compatible field names
✅ DATE_PART alias working
✅ Comprehensive error handling (invalid field for type, NULL handling)
✅ Documentation with examples

---

## SQL Examples (Target Functionality)

```sql
-- Temporal extraction
SELECT EXTRACT(year FROM hire_date) AS hire_year FROM employees;
SELECT EXTRACT(hour FROM log_time) AS log_hour FROM logs;
SELECT EXTRACT(microsecond FROM event_timestamp) AS event_us FROM events;
SELECT EXTRACT(month FROM rental_duration) AS rental_months FROM intervals;

-- UUID extraction
SELECT EXTRACT(version FROM user_id) AS uuid_version FROM users;
SELECT EXTRACT(timestamp FROM trace_id) AS trace_time FROM traces;

-- Network extraction
SELECT EXTRACT(family FROM ip_address) AS ip_family FROM connections;
SELECT EXTRACT(netmask FROM network_range) AS prefix_len FROM networks;

-- Spatial extraction
SELECT EXTRACT(x FROM location) AS longitude FROM places;
SELECT EXTRACT(num_points FROM route) AS waypoint_count FROM routes;
SELECT EXTRACT(srid FROM boundary) AS coordinate_system FROM regions;

-- Array extraction
SELECT EXTRACT(cardinality FROM tags) AS tag_count FROM articles;
SELECT EXTRACT(ndims FROM matrix) AS dimensions FROM matrices;

-- Range extraction
SELECT EXTRACT(lower FROM age_range) AS min_age FROM demographics;
SELECT EXTRACT(isempty FROM time_slot) AS is_empty FROM schedules;

-- DATE_PART alias
SELECT DATE_PART('year', hire_date) FROM employees;
```

---

**Document End**
