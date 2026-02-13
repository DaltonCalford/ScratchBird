# Integration Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

## Full Type System Integration for New Data Types

**Generated**: 2025-11-18
**Status**: Ready for Implementation
**Estimated Total Effort**: 40-55 hours

---

## Executive Summary

This plan outlines the complete integration of newly implemented data types (ARRAY, multi-geometry types, UINT variants, INT128, MONEY, INTERVAL) across the ScratchBird stack:

1. **Type Conversions**: Add 50+ conversion handlers
2. **Parser Integration**: Register keywords and type constructors
3. **SBLR Integration**: Implement bytecode handlers
4. **Function Implementation**: Complete 8+ missing functions

**Priority Order**: Functions → Type Conversions → Parser → SBLR

**Rationale**: Functions are critical for usability and have clear implementation patterns. Type conversions enable interoperability. Parser/SBLR integration enables SQL syntax support.

---

## Phase 1: Multi-Geometry Function Implementation (CRITICAL)
**Priority**: P0 (Blocking user functionality)
**Effort**: 6-8 hours
**Files Modified**: 4-5

### 1.1 Constructor Functions (2 hours)

**Functions to implement**:
- `ST_MultiPoint(geom1, geom2, ...)`
- `ST_MultiLineString(geom1, geom2, ...)`
- `ST_MultiPolygon(geom1, geom2, ...)`
- `ST_GeometryCollection(geom1, geom2, ...)`

**File**: `/home/user/ScratchBird/src/sblr/executor.cpp`

**Implementation pattern**:
```cpp
case Opcode::EXT_ST_MULTIPOINT:
{
    // Pop arguments from stack (count determined by function signature)
    std::vector<Point> points;
    size_t numArgs = /* from function metadata */;

    for (size_t i = 0; i < numArgs; i++) {
        Value arg = stack.pop();
        if (arg.getType() != DataType::POINT) {
            throw std::runtime_error("ST_MultiPoint requires POINT arguments");
        }
        points.push_back(arg.getPoint());
    }

    // Create MultiPoint and push to stack
    auto mp = TypedValue::makeMultiPoint(points);
    stack.push(Value(mp));
    break;
}
```

**Testing**:
```sql
SELECT ST_MultiPoint(POINT(0,0), POINT(1,1), POINT(2,2));
SELECT ST_MultiLineString(LINESTRING(0 0, 1 1), LINESTRING(2 2, 3 3));
```

### 1.2 Accessor Functions (2 hours)

**Functions to implement**:
- `ST_NumGeometries(multi_geom)` → INTEGER
- `ST_GeometryN(multi_geom, n)` → GEOMETRY (1-indexed)

**File**: `/home/user/ScratchBird/src/sblr/executor.cpp`

**Implementation pattern**:
```cpp
case Opcode::EXT_ST_NUMGEOMETRIES:
{
    Value arg = stack.pop();
    size_t count = 0;

    switch (arg.getType()) {
        case DataType::MULTIPOINT:
            count = arg.getMultiPoint().numGeometries();
            break;
        case DataType::MULTILINESTRING:
            count = arg.getMultiLineString().numGeometries();
            break;
        case DataType::MULTIPOLYGON:
            count = arg.getMultiPolygon().numGeometries();
            break;
        case DataType::GEOMETRYCOLLECTION:
            count = arg.getGeometryCollection().numGeometries();
            break;
        default:
            throw std::runtime_error("ST_NumGeometries requires multi-geometry type");
    }

    stack.push(Value(TypedValue::makeInt32(static_cast<int32_t>(count))));
    break;
}

case Opcode::EXT_ST_GEOMETRYN:
{
    Value indexVal = stack.pop();
    Value geomVal = stack.pop();

    int32_t index = indexVal.getInt32(); // 1-indexed in SQL

    if (geomVal.getType() == DataType::MULTIPOINT) {
        MultiPoint mp = geomVal.getMultiPoint();
        if (index < 1 || index > static_cast<int32_t>(mp.numGeometries())) {
            stack.push(Value::makeNull());
        } else {
            Point pt = mp.points[index - 1];
            stack.push(Value(TypedValue::makePoint(pt)));
        }
    }
    // ... similar for other multi-geometry types
    break;
}
```

**Testing**:
```sql
SELECT ST_NumGeometries(ST_MultiPoint(POINT(0,0), POINT(1,1)));  -- Returns 2
SELECT ST_GeometryN(ST_MultiPoint(POINT(0,0), POINT(1,1)), 1);   -- Returns POINT(0,0)
```

### 1.3 Aggregation Functions (2-3 hours)

**Functions to implement**:
- `ST_Collect(geom1, geom2, ...)` → GEOMETRYCOLLECTION
- `ST_Dump(multi_geom)` → TABLE(path, geom) (set-returning function)

**File**: `/home/user/ScratchBird/src/sblr/executor.cpp`

**Implementation notes**:
- `ST_Collect`: Similar to constructor, but accepts heterogeneous geometry types
- `ST_Dump`: Requires set-returning function support (may need SBLR enhancement)

**Deferral option**: ST_Dump can be deferred if set-returning functions aren't fully supported yet.

### 1.4 Testing (1 hour)

**File**: Create `/home/user/ScratchBird/tests/unit/test_spatial_functions.cpp`

**Test coverage**:
- Constructor functions with valid/invalid inputs
- Accessor functions with boundary conditions
- Type validation and error handling
- NULL handling

---

## Phase 2: Type Conversion Implementation (HIGH)
**Priority**: P1 (Enables type interoperability)
**Effort**: 15-20 hours
**Files Modified**: 2-3

### 2.1 ARRAY Conversions (3-4 hours)

**File**: `/home/user/ScratchBird/src/core/type_conversions.cpp`

**Conversions to add**:

| From | To | Implementation |
|------|-----|----------------|
| ARRAY | VARCHAR | `arrayToString()` - format as `{1,2,3}` PostgreSQL syntax |
| ARRAY | JSON | `arrayToJSON()` - format as `[1,2,3]` JSON array |
| VARCHAR | ARRAY | `stringToArray()` - parse `{1,2,3}` syntax |

**Implementation pattern**:
```cpp
static std::string arrayToString(const Array& arr)
{
    std::ostringstream oss;
    oss << "{";

    for (size_t i = 0; i < arr.elements.size(); i++) {
        if (i > 0) oss << ",";

        // Recursively convert element to string
        const auto& elem = arr.elements[i];
        if (elem.isNull()) {
            oss << "NULL";
        } else {
            // Use existing conversion logic
            auto str = TypeConverter::convertToString(elem);
            oss << str;
        }
    }

    oss << "}";
    return oss.str();
}
```

**Add to TypedValue::convertTo()**:
```cpp
case DataType::ARRAY:
    if (targetType == DataType::VARCHAR) {
        return TypedValue::makeString(arrayToString(getArray()));
    }
    break;
```

### 2.2 Multi-Geometry Conversions (4-5 hours)

**Conversions to add**:

| From | To | Implementation |
|------|-----|----------------|
| MULTIPOINT | VARCHAR | Use WKT format: `MULTIPOINT((0 0), (1 1))` |
| MULTILINESTRING | VARCHAR | Use WKT format: `MULTILINESTRING((0 0, 1 1), (2 2, 3 3))` |
| MULTIPOLYGON | VARCHAR | Use WKT format with polygon syntax |
| GEOMETRYCOLLECTION | VARCHAR | Use WKT format with type prefix per geometry |
| VARCHAR | MULTIPOINT | Parse WKT using existing WKT parser patterns |

**Implementation pattern**:
```cpp
static std::string multiPointToWKT(const MultiPoint& mp)
{
    std::ostringstream oss;
    oss << "MULTIPOINT(";

    for (size_t i = 0; i < mp.points.size(); i++) {
        if (i > 0) oss << ", ";
        oss << "(" << mp.points[i].x << " " << mp.points[i].y << ")";
    }

    oss << ")";
    return oss.str();
}
```

**Note**: May need to create `/home/user/ScratchBird/src/spatial/wkt_parser.cpp` if WKT parsing doesn't exist.

### 2.3 UINT Conversions (3-4 hours)

**Conversions to add**:

| From | To | Implementation | Validation |
|------|-----|----------------|------------|
| UINT8 | INT16 | Direct cast | None needed (range fits) |
| UINT16 | INT32 | Direct cast | None needed (range fits) |
| UINT32 | INT64 | Direct cast | None needed (range fits) |
| UINT64 | INT64 | Checked cast | Error if value > INT64_MAX |
| INT* | UINT* | Checked cast | Error if value < 0 |
| UINT* | FLOAT64 | Direct cast | Precision loss warning for large values? |
| UINT* | VARCHAR | `std::to_string()` | None needed |

**Implementation pattern**:
```cpp
static int64_t uint64ToInt64Checked(uint64_t value)
{
    if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        throw std::runtime_error("UINT64 value exceeds INT64 range");
    }
    return static_cast<int64_t>(value);
}

static uint32_t int32ToUint32Checked(int32_t value)
{
    if (value < 0) {
        throw std::runtime_error("Cannot convert negative INT32 to UINT32");
    }
    return static_cast<uint32_t>(value);
}
```

### 2.4 INT128 Conversions (2-3 hours)

**Conversions to add**:

| From | To | Implementation | Validation |
|------|-----|----------------|------------|
| INT128 | VARCHAR | Custom 128-bit to string | None needed |
| INT128 | INT64 | Checked cast | Error if overflow |
| INT128 | FLOAT64 | Direct cast | Precision loss for large values |
| VARCHAR | INT128 | Parse string to 128-bit | Error on invalid format |
| INT64 | INT128 | Direct cast | None needed |

**Implementation notes**:
- May need to use `boost::multiprecision` or custom 128-bit string conversion
- Check if `__int128` builtin is available (GCC/Clang specific)

### 2.5 MONEY Conversions (2 hours)

**Conversions to add**:

| From | To | Implementation |
|------|-----|----------------|
| MONEY | FLOAT64 | Divide by scale factor (e.g., cents to dollars) |
| MONEY | DECIMAL | Convert to fixed-point decimal |
| MONEY | VARCHAR | Format with currency symbol: `$1,234.56` |
| FLOAT64 | MONEY | Multiply by scale factor and round |
| VARCHAR | MONEY | Parse currency string |

**Implementation pattern**:
```cpp
// Assuming MONEY is stored as int64_t cents
static double moneyToFloat64(int64_t cents)
{
    return static_cast<double>(cents) / 100.0;
}

static std::string moneyToString(int64_t cents)
{
    std::ostringstream oss;
    oss << "$";
    if (cents < 0) {
        oss << "-";
        cents = -cents;
    }

    int64_t dollars = cents / 100;
    int64_t remainder = cents % 100;

    // Add thousands separators
    std::string dollarsStr = std::to_string(dollars);
    // ... (add comma formatting)

    oss << dollarsStr << "." << std::setfill('0') << std::setw(2) << remainder;
    return oss.str();
}
```

### 2.6 INTERVAL Conversions (2-3 hours)

**Conversions to add**:

| From | To | Implementation |
|------|-----|----------------|
| INTERVAL | VARCHAR | Format as `1 day 2 hours 30 minutes` |
| INTERVAL | INT64 | Total microseconds |
| VARCHAR | INTERVAL | Parse PostgreSQL interval syntax |
| INT64 | INTERVAL | Microseconds to interval |

**Implementation pattern**:
```cpp
static std::string intervalToString(const Interval& iv)
{
    std::ostringstream oss;

    if (iv.months != 0) {
        int32_t years = iv.months / 12;
        int32_t months = iv.months % 12;
        if (years != 0) oss << years << " year" << (years > 1 ? "s " : " ");
        if (months != 0) oss << months << " month" << (months > 1 ? "s " : " ");
    }

    if (iv.days != 0) {
        oss << iv.days << " day" << (iv.days > 1 ? "s " : " ");
    }

    if (iv.microseconds != 0) {
        int64_t hours = iv.microseconds / 3600000000LL;
        int64_t minutes = (iv.microseconds / 60000000LL) % 60;
        int64_t seconds = (iv.microseconds / 1000000LL) % 60;

        if (hours != 0) oss << hours << " hour" << (hours > 1 ? "s " : " ");
        if (minutes != 0) oss << minutes << " minute" << (minutes > 1 ? "s " : " ");
        if (seconds != 0) oss << seconds << " second" << (seconds > 1 ? "s " : " ");
    }

    return oss.str();
}
```

### 2.7 Integration (1 hour)

**Update**: `/home/user/ScratchBird/src/core/types.cpp` - `TypedValue::convertTo()`

Add all new conversion cases to the large switch statement.

### 2.8 Testing (2-3 hours)

**File**: `/home/user/ScratchBird/tests/unit/test_type_conversions.cpp`

**Test matrix**:
- All conversions listed above
- Boundary conditions (max/min values)
- Overflow/underflow detection
- NULL handling
- Invalid input handling

---

## Phase 3: Parser Integration (MEDIUM)
**Priority**: P2 (Enables SQL syntax support)
**Effort**: 3-4 hours
**Files Modified**: 3-4

### 3.1 Keyword Registration (30 minutes)

**File**: `/home/user/ScratchBird/src/parser/lexer.cpp`

**Add to KEYWORDS table**:
```cpp
{"multipoint", TokenType::KW_MULTIPOINT},
{"multilinestring", TokenType::KW_MULTILINESTRING},
{"multipolygon", TokenType::KW_MULTIPOLYGON},
{"geometrycollection", TokenType::KW_GEOMETRYCOLLECTION},
```

**File**: `/home/user/ScratchBird/include/scratchbird/parser/token.h`

**Add to TokenType enum**:
```cpp
KW_MULTIPOINT,
KW_MULTILINESTRING,
KW_MULTIPOLYGON,
KW_GEOMETRYCOLLECTION,
```

### 3.2 Type Name Parsing (1 hour)

**File**: `/home/user/ScratchBird/src/parser/parser.cpp`

**Update**: `parseTypeName()` function

**Add cases**:
```cpp
case TokenType::KW_MULTIPOINT:
    advance();
    return DataType::MULTIPOINT;

case TokenType::KW_MULTILINESTRING:
    advance();
    return DataType::MULTILINESTRING;

case TokenType::KW_MULTIPOLYGON:
    advance();
    return DataType::MULTIPOLYGON;

case TokenType::KW_GEOMETRYCOLLECTION:
    advance();
    return DataType::GEOMETRYCOLLECTION;
```

**Testing**:
```sql
CREATE TABLE spatial_test (
    id INT PRIMARY KEY,
    mp MULTIPOINT,
    mls MULTILINESTRING,
    mpoly MULTIPOLYGON,
    gc GEOMETRYCOLLECTION
);
```

### 3.3 Type Constructor Parsing (1.5-2 hours)

**File**: `/home/user/ScratchBird/src/parser/parser.cpp`

**Update**: `parsePrimary()` function

**Goal**: Parse constructor syntax like `MULTIPOINT((0 0), (1 1))`

**Implementation strategy**:
```cpp
case TokenType::KW_MULTIPOINT:
{
    advance(); // consume MULTIPOINT
    expect(TokenType::LEFT_PAREN);

    std::vector<std::shared_ptr<Expression>> points;

    do {
        // Parse nested parentheses: (x y)
        expect(TokenType::LEFT_PAREN);
        auto x = parseExpression();
        auto y = parseExpression();
        expect(TokenType::RIGHT_PAREN);

        // Create POINT literal expression
        auto pointExpr = std::make_shared<PointLiteral>(x, y);
        points.push_back(pointExpr);

    } while (match(TokenType::COMMA));

    expect(TokenType::RIGHT_PAREN);

    // Create MultiPoint literal expression
    return std::make_shared<MultiPointLiteral>(std::move(points));
}
```

**Note**: May need to create new Expression subclasses:
- `MultiPointLiteral`
- `MultiLineStringLiteral`
- `MultiPolygonLiteral`
- `GeometryCollectionLiteral`

**Alternative**: Rely on function call syntax instead of special literals
```sql
SELECT ST_MultiPoint(POINT(0,0), POINT(1,1));  -- Function approach (easier)
vs
SELECT MULTIPOINT((0 0), (1 1));               -- Literal approach (more work)
```

**Recommendation**: Start with function approach, add literal syntax if time permits.

### 3.4 Testing (1 hour)

**File**: `/home/user/ScratchBird/tests/unit/test_parser.cpp`

**Test cases**:
- Type names in CREATE TABLE
- Function call syntax for constructors
- (Optional) Literal syntax parsing

---

## Phase 4: SBLR Integration (MEDIUM)
**Priority**: P2 (Required for bytecode execution)
**Effort**: 4-6 hours
**Files Modified**: 2-3

### 4.1 Value Class Extension (2 hours)

**File**: `/home/user/ScratchBird/src/sblr/value.cpp` or similar

**Add factory methods**:
```cpp
Value Value::makeMultiPoint(const MultiPoint& mp)
{
    return Value(TypedValue::makeMultiPoint(mp));
}

Value Value::makeMultiLineString(const MultiLineString& mls)
{
    return Value(TypedValue::makeMultiLineString(mls));
}

// ... etc
```

**Add getter methods**:
```cpp
MultiPoint Value::getMultiPoint() const
{
    return typedValue_.getMultiPoint();
}

// ... etc
```

### 4.2 Opcode Validation (1 hour)

**File**: `/home/user/ScratchBird/include/scratchbird/sblr/opcodes.h`

**Verify opcodes exist** (likely already present based on exploration):
- `EXT_ST_MULTIPOINT`
- `EXT_ST_MULTILINESTRING`
- `EXT_ST_MULTIPOLYGON`
- `EXT_ST_GEOMETRYCOLLECTION`
- `EXT_ST_NUMGEOMETRIES`
- `EXT_ST_GEOMETRYN`
- `EXT_ST_COLLECT`

**If missing, add them** to the Opcode enum.

### 4.3 Bytecode Generator Integration (1-2 hours)

**File**: `/home/user/ScratchBird/src/sblr/bytecode_generator.cpp`

**Update**: Function call code generation

**Ensure mapping exists**:
```cpp
// In function call code generation
if (functionName == "ST_MULTIPOINT") {
    emitOpcode(Opcode::EXT_ST_MULTIPOINT);
} else if (functionName == "ST_NUMGEOMETRIES") {
    emitOpcode(Opcode::EXT_ST_NUMGEOMETRIES);
}
// ... etc
```

### 4.4 Executor Handlers (1-2 hours)

**File**: `/home/user/ScratchBird/src/sblr/executor.cpp`

**Add handler cases** (already covered in Phase 1, but verify integration):
- All multi-geometry function handlers
- Type construction handlers
- Accessor handlers

### 4.5 Testing (1 hour)

**File**: `/home/user/ScratchBird/tests/unit/test_sblr.cpp`

**Test cases**:
- Bytecode generation for spatial functions
- Stack operations for multi-geometry types
- End-to-end query execution

---

## Phase 5: Integration Testing & Validation (HIGH)
**Priority**: P1 (Ensures correctness)
**Effort**: 8-12 hours
**Files Modified**: 3-5 test files

### 5.1 End-to-End SQL Tests (4-5 hours)

**File**: Create `/home/user/ScratchBird/tests/integration/test_spatial_types.cpp`

**Test scenarios**:
```sql
-- Table creation
CREATE TABLE cities (
    id INT PRIMARY KEY,
    name VARCHAR(100),
    locations MULTIPOINT
);

-- Insert with constructor
INSERT INTO cities VALUES (
    1,
    'San Francisco',
    ST_MultiPoint(POINT(-122.4, 37.8), POINT(-122.5, 37.7))
);

-- Type conversion
SELECT name, locations::VARCHAR FROM cities;

-- Function calls
SELECT
    name,
    ST_NumGeometries(locations),
    ST_GeometryN(locations, 1)
FROM cities;

-- Type casting
SELECT
    ARRAY[1,2,3]::VARCHAR,
    '{1,2,3}'::ARRAY<INT>
;
```

### 5.2 Type Conversion Tests (2-3 hours)

**File**: `/home/user/ScratchBird/tests/unit/test_type_conversions.cpp`

**Coverage matrix**:
- All 50+ conversions
- Boundary values
- Error conditions
- NULL handling

### 5.3 Parser Tests (1-2 hours)

**File**: `/home/user/ScratchBird/tests/unit/test_parser.cpp`

**Test cases**:
- All new keywords recognized
- Type names in DDL
- Function syntax parsing

### 5.4 Function Tests (2-3 hours)

**File**: `/home/user/ScratchBird/tests/unit/test_spatial_functions.cpp`

**Per-function testing**:
- Valid inputs
- Invalid inputs
- Edge cases (empty geometries, NULL values)
- Type validation

### 5.5 Regression Testing (1 hour)

**Run full test suite**:
```bash
cd /home/user/ScratchBird
mkdir -p build && cd build
cmake ..
make -j$(nproc)
ctest --output-on-failure
```

**Verify**: No existing tests broken by new changes.

---

## Implementation Order & Dependencies

```
Phase 1 (Functions) ─────┐
                          ├──> Phase 5 (Integration Testing)
Phase 2 (Conversions) ───┤
                          │
Phase 3 (Parser) ────────┤
                          │
Phase 4 (SBLR) ──────────┘
```

**Recommended order**:
1. **Phase 1** (Functions) - Highest user value, clear implementation pattern
2. **Phase 2** (Conversions) - Enables type interoperability
3. **Phase 3 + 4** (Parser + SBLR) - Can be done in parallel or sequentially
4. **Phase 5** (Testing) - Continuous throughout, final validation at end

---

## Risk Assessment

### High Risk
- **WKT Parsing**: If WKT parser doesn't exist, creating one adds 4-6 hours
- **INT128 String Conversion**: Platform-specific, may need custom implementation
- **Set-Returning Functions**: ST_Dump may require SBLR enhancements

### Medium Risk
- **Parser Literal Syntax**: Multi-geometry literals are complex, may defer to function syntax
- **Type Conversion Edge Cases**: Overflow/underflow handling needs careful validation

### Low Risk
- **Function Implementation**: Clear patterns exist, well-documented
- **Keyword Registration**: Straightforward, low complexity

---

## Acceptance Criteria

### Phase 1 (Functions)
- ✅ All 8 multi-geometry functions compile without errors
- ✅ All functions have unit tests with 100% pass rate
- ✅ Functions work in SQL queries end-to-end

### Phase 2 (Conversions)
- ✅ All 50+ conversions implemented
- ✅ All conversions have tests covering success/error cases
- ✅ CAST syntax works in SQL: `SELECT value::TARGET_TYPE`

### Phase 3 (Parser)
- ✅ All multi-geometry type names recognized in CREATE TABLE
- ✅ Function call syntax parses correctly
- ✅ (Optional) Literal syntax parses correctly

### Phase 4 (SBLR)
- ✅ All new types work in bytecode execution
- ✅ Value class supports all new types
- ✅ Opcodes execute without errors

### Phase 5 (Testing)
- ✅ All new tests pass (target: 100+ new tests)
- ✅ No regressions in existing tests
- ✅ End-to-end SQL scenarios work

---

## Deferred Items

These can be addressed in future work:

1. **GeoJSON Conversions**: GEOMETRY ↔ GeoJSON (requires JSON parser)
2. **WKB Hex Format**: VARCHAR ↔ GEOMETRY via hex-encoded WKB
3. **ST_Dump Set-Returning**: May need SBLR enhancements
4. **Spatial Indexing**: R-tree or similar (large separate project)
5. **ARRAY Subscript Assignment**: `arr[1] = value` (requires lvalue support)
6. **INT128 Full Arithmetic**: If not already implemented

---

## Timeline Estimate

**Sequential execution**: 40-55 hours (5-7 business days)

**Parallel execution** (with 2-3 developers):
- Week 1: Phases 1 + 2 (Functions + Conversions)
- Week 2: Phases 3 + 4 (Parser + SBLR)
- Week 3: Phase 5 (Testing) + Polish

**Solo execution** (focused sessions):
- Day 1-2: Phase 1 (Functions)
- Day 3-4: Phase 2 (Conversions)
- Day 5: Phase 3 (Parser)
- Day 6: Phase 4 (SBLR)
- Day 7: Phase 5 (Testing)

---

## Next Steps

1. ✅ **This document** - Implementation plan created
2. ⏭️ **Begin Phase 1** - Implement multi-geometry functions
3. ⏭️ **Continue sequentially** through phases
4. ⏭️ **Update STATUS.md** after each phase completion
5. ⏭️ **Create PR** when all phases complete

**Ready to begin implementation!**
