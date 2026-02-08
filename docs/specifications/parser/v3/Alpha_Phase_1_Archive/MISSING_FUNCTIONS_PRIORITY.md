# Missing Functions - Priority Implementation Guide

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Created**: 2025-11-18 | **Status**: Critical Gaps Identified
**Total Scope**: ~130-170 hours of development work

---

## CRITICAL PRIORITY - Block implementation of new types

### 1. MULTI-GEOMETRY FUNCTIONS (8 functions) ⭐⭐⭐⭐⭐
**Severity**: CRITICAL - Blocks geometry type completion  
**Scope**: 6-8 hours  
**Files to modify**: 4

**Functions Missing**:
- ST_MULTIPOINT(...) - Construct multi-point geometry
- ST_MULTILINESTRING(...) - Construct multi-linestring
- ST_MULTIPOLYGON(...) - Construct multi-polygon
- ST_GEOMETRYCOLLECTION(...) - Construct geometry collection
- ST_COLLECT(...) - Alias for GeometryCollection
- ST_NUMGEOMETRIES(geom) - Get count of geometries
- ST_GEOMETRYN(geom, n) - Get Nth geometry
- ST_DUMP(geom) - Extract all geometries as table

**Opcodes**: Defined in opcodes.h:388-398 (EXT_ST_MULTIPOINT through EXT_ST_DUMP)
**Missing**:
- Bytecode generation (bytecode_generator.cpp FunctionCallExpr visitor)
- Executor handlers (executor.cpp evaluateExpression)
- MultiGeometry type in Value class
- Tests

**Implementation Steps**:
1. Add MultiGeometry struct to types.h (vector<Geometry>)
2. Add makeMultiGeometry() to Value class
3. Add 8 function cases to bytecode_generator.cpp:1415+
4. Add 8 handler cases to executor.cpp:8590+
5. Add tests in tests/integration/test_geometry_functions.cpp

**Related**: MULTIPOINT, MULTILINESTRING, MULTIPOLYGON types already in DATA_TYPE enum

---

### 2. ARRAY SUBSCRIPT OPERATOR ([index] notation) ⭐⭐⭐⭐
**Severity**: HIGH - Arrays incomplete without subscript  
**Scope**: 4-6 hours  
**Files to modify**: 4

**Syntax**: `SELECT array[1] FROM table;` → get first element

**Current Status**:
- Arrays stored as JSON in Value class
- ARRAY_LENGTH, ARRAY_APPEND implemented
- Subscript syntax NOT parsed, NOT in bytecode, NOT executed

**Missing**:
- Parser: Array subscript expression (e.g., `array[1]`, `array[1:3]`)
- Bytecode: EXT_ARRAY_SUBSCRIPT opcode
- Executor: Handler for subscript access/slicing
- Type safety: Validate index bounds

**Implementation Steps**:
1. Add array subscript parsing to parser.cpp parsePostfixExpr or similar
2. Create ArraySubscript AST node
3. Add EXT_ARRAY_SUBSCRIPT to opcodes.h
4. Add bytecode generation in bytecode_generator.cpp
5. Add executor handler in executor.cpp
6. Add tests

**Related**: ARRAY_UPPER, ARRAY_LOWER can give bounds for validation

---

## HIGH PRIORITY - Partial implementations

### 3. RANGE TYPE FUNCTIONS (15 functions) ⭐⭐⭐⭐
**Severity**: HIGH - Complete data type system  
**Scope**: 16-20 hours (complex - involves new type system)  
**Files to modify**: 6+

**Functions Missing**:
Type constructors:
- int4range(lower, upper [, bounds])
- int8range(lower, upper [, bounds])
- numrange(lower, upper [, bounds])
- daterange(lower, upper [, bounds])
- tsrange(lower, upper [, bounds])
- tstzrange(lower, upper [, bounds])

Operators:
- && (overlap)
- @> (contains range or element)
- <@ (contained by)
- << (strictly left of)
- >> (strictly right of)
- -|- (adjacent)
- + (union)
- & (intersection)
- - (difference)

Accessors:
- lower(range) → element
- upper(range) → element
- isempty(range) → bool
- lower_inc(range) → bool
- upper_inc(range) → bool
- lower_inf(range) → bool
- upper_inf(range) → bool
- range_merge(range, range) → range

**Current Status**:
- Type markers defined (EXT_TYPE_INT4RANGE etc)
- No Range data structure in Value class
- No constructor functions
- No operators
- No accessors

**Implementation Steps**:
1. Create Range<T> template in types.h with lower, upper, bounds flags
2. Add Range specialization to Value class
3. Add 6 type marker cases to opcodes
4. Add range constructors to bytecode generator
5. Add 15+ operators to executor
6. Add comprehensive tests

**Note**: This is substantial work due to type system changes

---

### 4. WINDOW FUNCTIONS - FRAME LOGIC ⭐⭐⭐⭐
**Severity**: HIGH - Only stubs exist  
**Scope**: 20-30 hours  
**Files to modify**: 3

**Status**: ROW_NUMBER, RANK, DENSE_RANK working  
**Missing Frame Logic**:
- PARTITION BY (partial)
- ORDER BY within window (partial)
- ROWS frame mode (unbounded preceding → current row, etc.)
- RANGE frame mode
- LAG/LEAD with offset parameter
- FIRST_VALUE, LAST_VALUE context
- NTH_VALUE context

**Current**: Executor stubs at line 5894-5915 just accumulate values

**Implementation Steps**:
1. Enhance window context tracking in executor
2. Implement frame boundary calculation
3. Implement partition grouping
4. Implement peer grouping for RANK/DENSE_RANK
5. Implement LAG/LEAD offset logic
6. Add comprehensive frame test suite

---

### 5. STATISTICAL AGGREGATE FUNCTIONS - COMPLETE MATH ⭐⭐⭐
**Severity**: MEDIUM - Math is stubbed  
**Scope**: 8-10 hours  
**Files to modify**: 2

**Functions**: STDDEV_SAMP, STDDEV_POP, VAR_SAMP, VAR_POP, CORR, COVAR_POP

**Current Status**:
- Bytecode generation works
- Opcode dispatch exists (executor.cpp:5158-5176)
- But handlers just accumulate values without calculation

**Missing Math**:
- Mean calculation (sum/count)
- Sum of squares for variance/stddev
- Biased vs unbiased estimators (sample vs population)
- Correlation coefficient calculation (Pearson)
- Covariance calculation

**Implementation Steps**:
1. Create AggregateAccumulator struct with:
   - count, sum, sum_squared, pairs vector
2. Modify executor handlers to populate accumulator
3. Implement finalization formulas
4. Add tests with known statistical values

---

## MEDIUM PRIORITY - Infrastructure gaps

### 6. NETWORK TYPE FUNCTIONS ⭐⭐⭐
**Severity**: MEDIUM - Useful but not critical  
**Scope**: 12-16 hours  
**Files to modify**: 4+

**Missing Types**:
- INET (IP address + netmask)
- CIDR (IP + netmask)
- MACADDR (MAC address)

**Missing Functions** (per type):
```
INET:
  inet(text) → inet
  host(inet) → text
  netmask(inet) → inet
  broadcast(inet) → inet
  network(inet) → inet
  prefix(inet) → int

CIDR:
  cidr(text) → cidr
  (same functions as INET)

MACADDR:
  macaddr(text) → macaddr
  trunc(macaddr) → macaddr

Operators:
  >> (contains)
  << (is contained by)
  = (equals)
  <> (not equals)
```

**Implementation Steps**:
1. Create Inet and MacAddr types in Value class
2. Implement parsing/validation
3. Add constructor functions to bytecode generator
4. Add operators and accessor functions
5. Comprehensive IPv4/IPv6 tests

---

### 7. INTERVAL TYPE ENHANCEMENT ⭐⭐⭐
**Severity**: MEDIUM - Partial implementation  
**Scope**: 10-12 hours  
**Files to modify**: 3+

**Current**: Limited date arithmetic support

**Missing**:
- Proper INTERVAL data type (months, days, seconds separately)
- INTERVAL literal: INTERVAL '1 year 2 months 3 days'
- Operators with different types:
  - date + interval → date
  - timestamp + interval → timestamp
  - interval + interval → interval
  - interval * numeric → interval
  - interval / numeric → interval
- Functions:
  - EXTRACT(field FROM interval)
  - DATE_TRUNC(field, interval)
  - JUSTIFY_INTERVAL, JUSTIFY_HOURS, JUSTIFY_DAYS

**Implementation Steps**:
1. Create Interval struct (months, days, microseconds)
2. Add interval parsing to lexer/parser
3. Implement all operators in executor
4. Add extraction and truncation functions
5. Temporal arithmetic tests

---

### 8. TEXT SEARCH (TSVECTOR/TSQUERY) ⭐⭐⭐
**Severity**: MEDIUM - Framework exists  
**Scope**: 20-25 hours  
**Files to modify**: 5+

**Current**: Type markers and opcodes defined

**Missing**:
- TSVECTOR type implementation (tokens + positions + weights)
- TSQUERY type implementation (query tree)
- Parser integration for tsvector literals
- Functions:
  - to_tsvector(config, text) → tsvector
  - to_tsquery(config, query) → tsquery
  - plainto_tsquery(config, text) → tsquery
  - phraseto_tsquery(config, phrase) → tsquery
- Operators:
  - @@ (matches)
  - <<, >> (distance operators for phrases)
- Functions:
  - ts_rank(tsvector, tsquery) → float
  - ts_headline(config, text, tsquery) → text

**Related Files**:
- include/scratchbird/core/ts_functions.h
- src/core/ts_functions.cpp (has some helpers)

---

## LOWER PRIORITY - Advanced features

### 9. USER-DEFINED FUNCTIONS (PSQL) ⭐⭐
**Severity**: LOW-MEDIUM - Complex feature  
**Scope**: 40-60 hours  
**Files to modify**: 5+

**Current Status**:
- CREATE FUNCTION parser works
- Catalog storage works
- Function invocation NOT implemented
- PSQL bytecode generation partial
- Variable declaration framework exists
- Control flow (IF, LOOP, etc.) framework exists

**Missing**:
- Function call operator (invoke user function from bytecode)
- PSQL statement bytecode generation (currently expression-only)
- Variable scope management
- Exception handling (TRY/EXCEPT blocks)
- Cursor support
- RETURN statement
- Parameter binding

**Estimated**: Full PSQL execution engine = 40-60 hours investment

---

### 10. AGGREGATE FILTER CLAUSE ⭐⭐
**Severity**: LOW - SQL feature  
**Scope**: 6-8 hours  
**Files to modify**: 3

**Syntax**: `SELECT COUNT(*) FILTER (WHERE condition) FROM table;`

**Missing**:
- Parser: FILTER keyword after aggregate function
- Bytecode: Conditional aggregate dispatch
- Executor: Aggregate function with WHERE condition

---

### 11. LATERAL JOINS ⭐⭐
**Severity**: LOW - Advanced SQL  
**Scope**: 12-16 hours  
**Files to modify**: 4

**Syntax**: `SELECT * FROM t1, LATERAL function_call(t1.col) AS t2`

**Current**: Keyword exists, parser/executor stub

**Missing**: Full lateral join planner + executor

---

## IMPLEMENTATION CHECKLIST

### For Each New Function Group:

Priority 1: Multi-Geometry (MUST DO)
- [ ] Add/modify Value class types
- [ ] Update bytecode generator
- [ ] Update executor handlers
- [ ] Unit tests
- [ ] Integration tests

Priority 2: Array Subscript (HIGH)
- [ ] Parser enhancements
- [ ] AST node creation
- [ ] Bytecode generation
- [ ] Executor handler
- [ ] Bound checking tests

Priority 3: Range Type (HIGH - complex)
- [ ] Type system design
- [ ] Value class Range<T>
- [ ] Constructor functions
- [ ] Operator implementations
- [ ] Accessor implementations
- [ ] Tests with various bounds

Priority 4: Window Frame Logic (HIGH - complex)
- [ ] Window context refactoring
- [ ] Frame boundary logic
- [ ] Partition/order tracking
- [ ] Frame tests (unit)
- [ ] Integration tests

Then: Statistical, Network, Interval, TextSearch, UDF, Filter, Lateral

---

## RELATED DOCUMENTATION

- `/docs/IMPLEMENTATION_AUDIT.md` - Comprehensive function audit
- `/include/scratchbird/sblr/opcodes.h` - All defined opcodes
- `/src/sblr/bytecode_generator.cpp:1415-3050` - Function name mapping
- `/src/sblr/executor.cpp:7271-12500` - Execution handlers

---

## QUICK START - Add a Simple Function (Template)

See `/docs/FUNCTION_IMPLEMENTATION_ANALYSIS.md` Section 7 for detailed pattern.

Example: Add ST_CENTROID(geom) → point

```cpp
// 1. opcodes.h
EXT_ST_CENTROID = 0x8F,  // Compute centroid of geometry

// 2. bytecode_generator.cpp:1415+
else if (func_name == "ST_CENTROID") {
    for (auto *arg : node->args()) generateExpression(arg);
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_CENTROID));
    return;
}

// 3. executor.cpp:9355+
else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_CENTROID)) {
    Value geom = pop();
    if (geom.isNull()) { push(Value::makeNull()); }
    else if (geom.type() == core::DataType::POLYGON) {
        try {
            auto poly = geom.getPolygon();
            auto centroid = spatial::computeCentroid(poly);
            push(Value::makePoint(centroid));
        } catch (...) { push(Value::makeNull()); }
    } else { push(Value::makeNull()); }
}

// 4. Add test case
bool testCentroid() { /* ... */ }
```

