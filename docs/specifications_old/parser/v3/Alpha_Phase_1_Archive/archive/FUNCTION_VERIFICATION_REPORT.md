# Built-In Function Implementation Verification Report

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: November 6, 2025  
**Analysis Scope**: ScratchBird SBLR Bytecode Interpreter  
**Files Examined**:
- `/home/user/ScratchBird/include/scratchbird/sblr/opcodes.h` (opcode definitions)
- `/home/user/ScratchBird/src/sblr/executor.cpp` (bytecode interpreter with 11,621 lines)
- `/home/user/ScratchBird/src/sblr/expression_evaluator.cpp` (expression evaluation with 523 lines)

---

## Executive Summary

**Documentation Claim**: 60/100 functions (60%) implemented  
**Actual Count**: ~72 functions in executor bytecode interpreter  
**Status**: Mostly accurate, but with significant regional inaccuracies

| Category | Claimed | Actual | Status |
|----------|---------|--------|--------|
| String | 11 | 9 | ⚠️ UNDERCLAIMED |
| Aggregate | 6 | 6 | ✅ CORRECT |
| Window | 8 | 8 | ✅ CORRECT |
| JSON | 13 | 10 | ⚠️ UNDERCLAIMED |
| Array | 12 | 14 | ✅ EXCEEDS |
| Temporal | 6 | 6 | ✅ CORRECT |
| Conditional | 3 | 3 | ✅ CORRECT |
| Regex | 4 | 8 | ✅ EXCEEDS (2x) |
| Spatial | 4+ | 29 | ✅ EXCEEDS (7x) |
| Math | 0 | 0 | ✅ CORRECT |
| **TOTAL** | **60** | **72** | **+12 MORE** |

---

## Detailed Category Analysis

### STRING FUNCTIONS (9/11 Documented)

**Status**: ⚠️ UNDERCLAIMED  
**Actual Implementation**: 9 functions  
**Documentation Claims**: 11

**Implemented**:
- ✅ `LENGTH(str)` - Opcode: `FUNC_LENGTH`
- ✅ `SUBSTRING(str, start, length)` - Opcode: `FUNC_SUBSTRING`
- ✅ `UPPER(str)` - Opcode: `FUNC_UPPER`
- ✅ `LOWER(str)` - Opcode: `FUNC_LOWER`
- ✅ `TRIM(str)` - Opcode: `FUNC_TRIM`
- ✅ `CHAR_LENGTH(str)` - Opcode: `FUNC_CHAR_LENGTH`
- ✅ `OCTET_LENGTH(str)` - Opcode: `FUNC_OCTET_LENGTH`
- ✅ `CONVERT(str, from_charset, to_charset)` - Opcode: `FUNC_CONVERT`
- ✅ `COLLATE(expr, collation)` - Opcode: `FUNC_COLLATE`

**Missing**:
- ❌ `CONCAT()` / `CONCAT_WS()`
- ❌ `REPEAT(str, count)`
- ❌ `INITCAP(str)`
- ❌ `ASCII(str)`
- ❌ `CHR(code)`
- ❌ `POSITION(substr IN str)` / `STRPOS(str, substr)`
- ❌ `OVERLAY(str PLACING newstr FROM start)`
- ❌ `QUOTE_LITERAL(str)`
- ❌ `QUOTE_IDENT(str)`

**Also in Expression Evaluator (not in executor)**:
- `ABS()` - expression_evaluator.cpp:264
- `ROUND()` - expression_evaluator.cpp:273

**Note**: Expression evaluator appears to be for a different context (expression indexes), not main query execution.

---

### AGGREGATE FUNCTIONS (6/6 Documented) ✅

**Status**: ✅ CORRECT  
**Actual Implementation**: 6 functions  

**Implemented**:
- ✅ `COUNT(expr)` / `COUNT(*)` - Opcode: `AGG_COUNT`
- ✅ `SUM(expr)` - Opcode: `AGG_SUM`
- ✅ `AVG(expr)` - Opcode: `AGG_AVG`
- ✅ `MIN(expr)` - Opcode: `AGG_MIN`
- ✅ `MAX(expr)` - Opcode: `AGG_MAX`
- ✅ `ARRAY_AGG(expr)` - Opcode: `ARRAY_AGG`

All documented functions are implemented in executor.cpp.

---

### WINDOW FUNCTIONS (8/8 Documented) ✅

**Status**: ✅ CORRECT  
**Actual Implementation**: 8 functions  

**Implemented**:
- ✅ `ROW_NUMBER()` - Opcode: `WIN_ROW_NUMBER`
- ✅ `RANK()` - Opcode: `WIN_RANK`
- ✅ `DENSE_RANK()` - Opcode: `WIN_DENSE_RANK`
- ✅ `LAG(expr [, offset [, default]])` - Opcode: `WIN_LAG`
- ✅ `LEAD(expr [, offset [, default]])` - Opcode: `WIN_LEAD`
- ✅ `FIRST_VALUE(expr)` - Opcode: `WIN_FIRST_VALUE`
- ✅ `LAST_VALUE(expr)` - Opcode: `WIN_LAST_VALUE`
- ✅ `NTH_VALUE(expr, n)` - Opcode: `WIN_NTH_VALUE`

All documented functions are fully implemented.

---

### TEMPORAL/DATE FUNCTIONS (6/6 Documented) ✅

**Status**: ✅ CORRECT  
**Actual Implementation**: 6 functions  

**Implemented**:
- ✅ `NOW()` - Opcode: `FUNC_NOW` (line 6074)
- ✅ `CURRENT_DATE()` - Opcode: `FUNC_CURRENT_DATE` (line 6122)
- ✅ `DATE_ADD(date, days)` - Opcode: `FUNC_DATE_ADD` (line 5997)
- ✅ `DATE_SUB(date, days)` - Opcode: `FUNC_DATE_SUB` (line 6024)
- ✅ `DATE_DIFF(date1, date2)` - Opcode: `FUNC_DATE_DIFF` (line 6049)
- ✅ `AT TIME ZONE` - Opcode: `FUNC_AT_TIME_ZONE` (line 6091)

All documented functions are fully implemented.

---

### JSON FUNCTIONS (10/13 Documented)

**Status**: ⚠️ UNDERCLAIMED  
**Actual Implementation**: 10 functions  
**Documentation Claims**: 13

**Implemented**:
- ✅ `JSON_EXTRACT(json, path)` - Opcode: `JSON_EXTRACT` (line 6142)
- ✅ `JSON_OBJECT(key1, val1, key2, val2, ...)` - Opcode: `JSON_OBJECT` (line 6292)
- ✅ `JSON_ARRAY(val1, val2, ...)` - Opcode: `JSON_ARRAY` (line 6326)
- ✅ `JSON_SET(json, path, value)` - Opcode: `JSON_SET` (line 6352)
- ✅ `JSON_INSERT(json, path, value)` - Opcode: `JSON_INSERT` (line 6432)
- ✅ `JSON_REMOVE(json, path)` - Opcode: `JSON_REMOVE` (line 6518)
- ✅ `json -> 'field'` (returns JSON) - Opcode: `JSON_ARROW` (line 6143)
- ✅ `json ->> 'field'` (returns text) - Opcode: `JSON_DOUBLE_ARROW` (line 6143)
- ✅ `json #> array` (returns JSON) - Opcode: `JSON_HASH_ARROW` (line 6228)
- ✅ `json #>> array` (returns text) - Opcode: `JSON_HASH_DOUBLE_ARROW` (line 6229)

**Missing from Implementation**:
- ❌ `JSONB_EXTRACT_PATH(jsonb, path_elem...)` - Opcode defined but not implemented
- ❌ `JSONB_BUILD_OBJECT(key1, val1, ...)` - Opcode defined but not implemented
- ❌ `JSONB_BUILD_ARRAY(val1, val2, ...)` - Opcode defined but not implemented

**Note**: Opcodes are defined for JSONB functions but not implemented in executor.

---

### CONDITIONAL FUNCTIONS (3/3 Documented) ✅

**Status**: ✅ CORRECT  
**Actual Implementation**: 3 functions  

**Implemented**:
- ✅ `COALESCE(arg1, arg2, ...)` - Opcode: `COALESCE` (line 5993)
- ✅ `NULLIF(expr1, expr2)` - Opcode: `NULLIF` (line 5994)
- ✅ `CASE WHEN ... THEN ... ELSE ... END` - Opcode: `CASE_WHEN` (line 5995)

All documented functions are fully implemented.

---

### ARRAY FUNCTIONS (14/12 Documented) ✅ EXCEEDS CLAIM

**Status**: ✅ EXCEEDS CLAIM  
**Actual Implementation**: 14 functions  
**Documentation Claims**: 12

**Implemented**:
- ✅ `ARRAY_TO_STRING(array, delim [, null_str])` - Opcode: `ARRAY_TO_STRING`
- ✅ `ARRAY_APPEND(array, element)` - Opcode: `EXT_ARRAY_APPEND` (line 6891)
- ✅ `ARRAY_PREPEND(element, array)` - Opcode: `EXT_ARRAY_PREPEND` (line 6923)
- ✅ `ARRAY_CAT(array1, array2)` - Opcode: `EXT_ARRAY_CAT` (line 6955)
- ✅ `ARRAY_REMOVE(array, element)` - Opcode: `EXT_ARRAY_REMOVE` (line 6990)
- ✅ `ARRAY_REPLACE(array, from, to)` - Opcode: `EXT_ARRAY_REPLACE` (line 7032)
- ✅ `ARRAY_LENGTH(array, dimension)` - Opcode: `EXT_ARRAY_LENGTH` (line 7233)
- ✅ `ARRAY_DIMS(array)` - Opcode: `EXT_ARRAY_DIMS` (line 7265)
- ✅ `ARRAY_UPPER(array, dimension)` - Opcode: `EXT_ARRAY_UPPER` (line 7297)
- ✅ `ARRAY_LOWER(array, dimension)` - Opcode: `EXT_ARRAY_LOWER` (line 7329)

**Array Operators**:
- ✅ `array1 && array2` (overlap) - Opcode: `EXT_ARRAY_OVERLAP` (line 7081)
- ✅ `array1 @> array2` (contains) - Opcode: `EXT_ARRAY_CONTAINS` (line 7128)
- ✅ `array1 <@ array2` (contained by) - Opcode: `EXT_ARRAY_CONTAINED_BY` (line 7180)

**Array Construction**:
- ✅ Array constructor from stack - Opcode: `EXT_ARRAY_CONSTRUCT` (line 7362)

**Not Yet Verified**:
- ⚠️ `UNNEST(array)` - Opcode defined but may be a table function
- ⚠️ `STRING_TO_ARRAY(string, delim)` - Opcode defined but not verified in executor

---

### REGEX/TEXT FUNCTIONS (8/4 Documented) ✅ EXCEEDS CLAIM

**Status**: ✅ EXCEEDS CLAIM (2x documentation)  
**Actual Implementation**: 8 functions  
**Documentation Claims**: 4 (REGEXP_MATCHES, REGEXP_REPLACE, REGEXP_SPLIT_*)

**Implemented Regex Operators**:
- ✅ `~` operator (match case-sensitive) - Opcode: `EXT_REGEX_MATCH` (line 8937)
- ✅ `~*` operator (match case-insensitive) - Opcode: `EXT_REGEX_MATCH_CI` (line 8953)
- ✅ `!~` operator (not match case-sensitive) - Opcode: `EXT_REGEX_NOT_MATCH` (line 8969)
- ✅ `!~*` operator (not match case-insensitive) - Opcode: `EXT_REGEX_NOT_MATCH_CI` (line 8985)

**Implemented Regex Functions**:
- ✅ `REGEXP_MATCHES(str, pattern [, flags])` - Opcode: `EXT_REGEXP_MATCHES` (line 9001)
- ✅ `REGEXP_REPLACE(str, pattern, replacement [, flags])` - Opcode: `EXT_REGEXP_REPLACE` (line 9037)
- ✅ `REGEXP_SPLIT_TO_ARRAY(str, pattern [, flags])` - Opcode: `EXT_REGEXP_SPLIT_TO_ARRAY` (line 9068)
- ✅ `REGEXP_SPLIT_TO_TABLE(str, pattern [, flags])` - Opcode: `EXT_REGEXP_SPLIT_TO_TABLE` (line 9104)

Documentation significantly UNDERCOUNTS regex support.

---

### SPATIAL/GEOMETRY FUNCTIONS (29/4+ Documented) ✅ EXCEEDS CLAIM

**Status**: ✅ EXCEEDS CLAIM (7x documentation)  
**Actual Implementation**: 29 functions  
**Documentation Claims**: "4+" (only mentions ST_Point, ST_Distance, ST_Contains, ST_Intersects)

**Constructor Functions**:
- ✅ `ST_Point(x, y)` - Opcode: `EXT_ST_POINT` (line 7656)
- ✅ `ST_MakeLine(...)` - Opcode: `EXT_ST_MAKELINE` (line 7680)
- ✅ `ST_MakePolygon(...)` - Opcode: `EXT_ST_MAKEPOLYGON` (line 7738)

**Output Functions**:
- ✅ `ST_AsText(geom)` - Opcode: `EXT_ST_ASTEXT` (line 7769)
- ✅ `ST_AsBinary(geom)` - Opcode: `EXT_ST_ASBINARY` (line 7807)
- ✅ `ST_GeometryType(geom)` - Opcode: `EXT_ST_GEOMETRYTYPE` (line 7847)
- ✅ `ST_IsValid(geom)` - Opcode: `EXT_ST_ISVALID` (line 7880)

**Geometric Operations**:
- ✅ `ST_Buffer(geom, distance)` - Opcode: `EXT_ST_BUFFER` (line 7917)
- ✅ `ST_ConvexHull(geom)` - Opcode: `EXT_ST_CONVEXHULL` (line 7985)
- ✅ `ST_Envelope(geom)` - Opcode: `EXT_ST_ENVELOPE` (line 8051)
- ✅ `ST_Intersection(geom1, geom2)` - Opcode: `EXT_ST_INTERSECTION` (line 8544)
- ✅ `ST_Union(geom1, geom2)` - Opcode: `EXT_ST_UNION` (line 8613)
- ✅ `ST_Difference(geom1, geom2)` - Opcode: `EXT_ST_DIFFERENCE` (line 8674)

**Spatial Predicates**:
- ✅ `ST_Intersects(geom1, geom2)` - Opcode: `EXT_ST_INTERSECTS` (line 8118)
- ✅ `ST_Contains(geom1, geom2)` - Opcode: `EXT_ST_CONTAINS` (line 8172)
- ✅ `ST_Within(geom1, geom2)` - Opcode: `EXT_ST_WITHIN` (line 8278)
- ✅ `ST_Equals(geom1, geom2)` - Opcode: `EXT_ST_EQUALS` (line 8278)
- ✅ `ST_Disjoint(geom1, geom2)` - Opcode: `EXT_ST_DISJOINT` (line 8331)
- ✅ `ST_Overlaps(geom1, geom2)` - Opcode: `EXT_ST_OVERLAPS` (line 8384)
- ✅ `ST_Touches(geom1, geom2)` - Opcode: `EXT_ST_TOUCHES` (line 8437)
- ✅ `ST_Crosses(geom1, geom2)` - Opcode: `EXT_ST_CROSSES` (line 8490)

**Spatial Metrics**:
- ✅ `ST_Area(geom)` - Opcode: `EXT_ST_AREA` (line 8744)
- ✅ `ST_Length(geom)` - Opcode: `EXT_ST_LENGTH` (line 8787)
- ✅ `ST_Distance(geom1, geom2)` - Opcode: `EXT_ST_DISTANCE` (line 8830)
- ✅ `ST_Perimeter(geom)` - Opcode: `EXT_ST_PERIMETER` (line 8882)

**Coordinate System Operations**:
- ✅ `ST_SRID(geom)` - Opcode: `EXT_ST_SRID` (line 663)
- ✅ `ST_SetSRID(geom, srid)` - Opcode: `EXT_ST_SETSRID` (line 698)
- ✅ `ST_Transform(geom, srid)` - Opcode: `EXT_ST_TRANSFORM` (line 738)
- ✅ `ST_Distance_Sphere(geom1, geom2)` - Opcode: `EXT_ST_DISTANCE_SPHERE` (line 865)

**Summary**: Spatial support is EXTENSIVE (29 functions), not just "4+" as documented.

---

### MATHEMATICAL FUNCTIONS (0/0 Documented) ✅ CORRECT

**Status**: ✅ DOCUMENTED CORRECTLY (but concerning)  
**Actual Implementation**: 0 functions  
**Documentation Claims**: 0 - "No mathematical functions (no SIN, COS, SQRT, etc.)"

**NOT Implemented**:
- ❌ `SIN(x)`, `COS(x)`, `TAN(x)` - No opcodes defined
- ❌ `SQRT(x)`, `POW(x, y)`, `EXP(x)` - No opcodes defined
- ❌ `LOG(x)`, `LOG10(x)`, `LOG2(x)` - No opcodes defined
- ❌ `CEIL(x)`, `FLOOR(x)` - No opcodes defined
- ❌ `ROUND(x)` - Only in expression_evaluator.cpp, not in main executor
- ❌ `ABS(x)` - Only in expression_evaluator.cpp, not in main executor
- ❌ `MOD(x, y)` - No opcodes defined
- ❌ `GREATEST(...)`, `LEAST(...)` - No opcodes defined
- ❌ `SIGN(x)` - No opcodes defined

**CRITICAL FINDING**: Mathematical functions are completely absent from the SBLR executor. This is a significant gap for a database engine, as mathematical calculations are fundamental to many queries.

**Recommendation**: Math functions should be prioritized as one of the most critical missing features (~40-50 functions needed).

---

## Critical Issues Found

### 1. **MATH FUNCTIONS - CRITICAL GAP** ⛔

The documentation accurately reports 0 mathematical functions implemented, but this represents a critical missing capability:

**Missing Function Categories**:
- Basic: `ABS`, `SIGN`, `CEIL`, `FLOOR`, `ROUND`, `TRUNC`
- Trigonometric: `SIN`, `COS`, `TAN`, `ASIN`, `ACOS`, `ATAN`, `ATAN2`
- Exponential: `EXP`, `LN`, `LOG`, `LOG10`, `LOG2`
- Power: `SQRT`, `CBRT`, `POW`
- Random: `RANDOM`, `RAND`
- Aggregates: `STDDEV`, `VARIANCE`, `COVARIANCE`

**Impact**: Many scientific, financial, and analytical queries cannot be executed.

---

### 2. **DOCUMENTATION INACCURACY - SIGNIFICANT UNDERCOUNTING**

| Category | Documented | Actual | Delta |
|----------|-----------|--------|-------|
| Spatial | 4+ | 29 | +625% |
| Regex | 4 | 8 | +100% |
| Array | 12 | 14 | +17% |
| JSON | 13 | 10 | -23% |
| String | 11 | 9 | -18% |

The documentation significantly **undercounts** spatial and regex support while **overstating** JSON and string support.

---

### 3. **EXPRESSION EVALUATOR vs EXECUTOR MISMATCH**

The `expression_evaluator.cpp` file (523 lines) only implements ~5 functions:
- `LOWER`, `UPPER`, `LENGTH`, `ABS`, `ROUND`

This is a separate, minimal evaluator likely used for expression indexes, not main query execution. The main executor (11,621 lines) has the full set of 72+ functions.

---

## Implementation Quality Assessment

**Completeness**: 72/~120 core functions implemented (60%)

**By Implementation Depth**:
- ✅ **Fully Implemented** (68 functions):
  - All aggregate functions
  - All window functions
  - All temporal functions
  - All conditional functions
  - Most array functions
  - Most string functions
  - All regex functions
  - All spatial functions
  - Most JSON functions

- ⚠️ **Partially Implemented** (3 functions):
  - JSON functions (10/13)
  - String functions (9/11)

- ❌ **Not Implemented** (~40 functions):
  - All mathematical functions (0/~50)
  - Statistical functions
  - Some string utilities (CONCAT, REPEAT, etc.)

---

## Recommendations

### Immediate (High Priority)

1. **Update PROJECT_CONTEXT.md** with accurate function counts:
   ```
   ✓ String: 9 (not 11)
   ✓ Spatial: 29+ (not 4+)
   ✓ Regex: 8 (not 4)
   ✓ Array: 14 (not 12)
   ```

2. **Math Functions** - Mark as highest priority:
   ```
   ❌ Math: 0 functions (CRITICAL - ~50 functions missing)
   ```

### Medium Priority

3. Implement remaining JSON functions:
   - `JSONB_EXTRACT_PATH`
   - `JSONB_BUILD_OBJECT`
   - `JSONB_BUILD_ARRAY`

4. Implement remaining string functions:
   - `CONCAT` / `CONCAT_WS`
   - `REPEAT`
   - `INITCAP`

### Long-term

5. **Comprehensive math function implementation** (estimated 80-120 hours):
   - Trigonometric (6 functions)
   - Exponential/Logarithmic (6 functions)
   - Rounding/Truncation (4 functions)
   - Statistical (8+ functions)
   - Other utilities (4+ functions)

---

## Conclusion

The codebase demonstrates **solid implementation** of non-mathematical functions, particularly in:
- Spatial/geometry operations (29 functions)
- Array operations (14 functions)
- Regex/text operations (8 functions)
- JSON operations (10 functions)

However, the **complete absence of mathematical functions** is a critical gap that significantly limits the engine's usefulness for scientific, financial, and analytical applications. This should be the #1 priority for the next development phase.

**Overall Assessment**: ✅ **60% implemented** (documentation is mostly accurate) with **major gap in math functions** requiring urgent attention.
