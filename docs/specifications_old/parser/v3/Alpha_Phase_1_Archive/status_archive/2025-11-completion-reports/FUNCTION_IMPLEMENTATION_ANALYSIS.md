
**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

FUNCTION IMPLEMENTATION ANALYSIS - ScratchBird Database Engine
================================================================

## 1. ARCHITECTURE OVERVIEW

Function implementation follows a 3-layer pattern:

Layer 1: OPCODES (include/scratchbird/sblr/opcodes.h)
  - Defines Opcode enum with function codes
  - Extended opcodes (0xFF prefix) for 256+ functions
  - Range allocations for function categories

Layer 2: BYTECODE GENERATION (src/sblr/bytecode_generator.cpp)
  - visit(FunctionCallExpr) maps function names to opcodes
  - Emits opcode + arguments in bytecode
  - Handles both standard (0x7X-0xFE) and extended (0xFF + byte) opcodes

Layer 3: EXECUTION (src/sblr/executor.cpp)
  - evaluateExpression() dispatches opcodes
  - Extended opcodes handled in switch at line 8590+
  - Each function has handler for argument popping, execution, result pushing

## 2. BYTECODE GENERATOR - FUNCTION NAME MAPPING

File: src/sblr/bytecode_generator.cpp:1415-3050 (FunctionCallExpr visitor)

Pattern for each function:
  1. Check if func_name matches (e.g., "LENGTH", "UPPER", "ST_POINT")
  2. Generate arguments via generateExpression()
  3. Emit opcode (standard or EXTENDED_OPCODE + ext_opcode)
  4. Write argument count as uint8_t if needed
  5. Return early to skip fallback handling

### IMPLEMENTED FUNCTION GROUPS

#### STRING FUNCTIONS (5)
- LENGTH (FUNC_LENGTH, 0x73)
- SUBSTRING (FUNC_SUBSTRING, 0x74)
- UPPER (FUNC_UPPER, 0x75)
- LOWER (FUNC_LOWER, 0x76)
- TRIM (FUNC_TRIM, 0x77)

#### AGGREGATE FUNCTIONS (6)
- SUM, AVG, MIN, MAX (AGG_SUM-AGG_MAX, 0x7A-0x7D)
- COUNT (AGG_COUNT, 0x7E)
- ARRAY_AGG (extended)

#### TEMPORAL FUNCTIONS (6)
- DATE_ADD, DATE_SUB, DATE_DIFF (0x84-0x86)
- NOW, CURRENT_DATE (0x87-0x88)
- AT TIME ZONE (0x8D)

#### SPATIAL FUNCTIONS (40+)
Basic (4): ST_POINT, ST_MAKELINE, ST_MAKEPOLYGON, [ASTEXT needs extension]
Predicates (8): ST_INTERSECTS, ST_CONTAINS, ST_WITHIN, ST_EQUALS, ST_DISJOINT, ST_OVERLAPS, ST_TOUCHES, ST_CROSSES
Operations (3): ST_INTERSECTION, ST_UNION, ST_DIFFERENCE
Metrics (4): ST_AREA, ST_LENGTH, ST_DISTANCE, ST_PERIMETER
Geometric (3): ST_BUFFER, ST_CONVEXHULL, ST_ENVELOPE
SRID (3): ST_SRID, ST_SETSRID, ST_TRANSFORM
Distance (1): ST_DISTANCE_SPHERE
Output (3): ST_ASTEXT, ST_ASBINARY, ST_GEOMETRYTYPE
Validation (1): ST_ISVALID

#### ARRAY FUNCTIONS (12)
Manipulation (5): ARRAY_APPEND, ARRAY_PREPEND, ARRAY_CAT, ARRAY_REMOVE, ARRAY_REPLACE
Operators (5): && (overlap), @> (contains), <@ (contained_by), = (eq), <> (ne)
Accessors (4): ARRAY_LENGTH, ARRAY_DIMS, ARRAY_UPPER, ARRAY_LOWER
Conversion (2): ARRAY_TO_STRING, STRING_TO_ARRAY
Table function (1): UNNEST

#### REGEX & TEXT FUNCTIONS (13)
- REGEXP_MATCHES (4 args: str, pattern, flags, flags_str)
- REGEXP_REPLACE (4 args)
- REGEXP_SPLIT_TO_ARRAY (3 args)
- REGEXP_SPLIT_TO_TABLE (3 args)
- SPLIT_PART (3 args)
- STRING_TO_TABLE (2 args)
- UNNEST_TEXT (1 arg)
- STRPOS (2 args)
- POSITION (2 args)
- OVERLAY (4 args)
- QUOTE_LITERAL (1 arg)
- QUOTE_IDENT (1 arg)

#### STRING MANIPULATION (5)
- INITCAP (1 arg)
- ASCII (1 arg)
- CHR (1 arg)
- REPEAT (2 args)
- REVERSE (1 arg)

#### MATHEMATICAL FUNCTIONS (29)
Trigonometric (7): SIN, COS, TAN, ASIN, ACOS, ATAN, ATAN2
Degrees (2): DEGREES, RADIANS
Constants (1): PI
Algebraic (6): ABS, SIGN, ROUND, CEIL/CEILING, FLOOR, TRUNC/TRUNCATE, MOD
Roots (3): SQRT, CBRT, POWER/POW
Exponential (1): EXP
Logarithmic (4): LN, LOG, LOG10, LOG2

#### STATISTICAL FUNCTIONS (6)
- STDDEV / STDDEV_SAMP
- STDDEV_POP
- VARIANCE / VAR_SAMP
- VAR_POP
- CORR (2 args)
- COVAR_POP (2 args)

#### CRYPTOGRAPHIC FUNCTIONS (4)
- MD5, SHA1, SHA256, SHA512
Opcodes: 0xF9-0xFC (opcodes.h:549-553)

#### BIT MANIPULATION FUNCTIONS (14)
Byte Access (4): GET_BYTE, SET_BYTE, GET_BIT, SET_BIT
Bitwise Ops (4): BIT_AND, BIT_OR, BIT_XOR, BIT_NOT
Shift Ops (3): BIT_SHIFT_LEFT, BIT_SHIFT_RIGHT, BIT_SHIFT_RIGHT_LOGICAL
Utilities (3): BIT_COUNT, BIT_LENGTH, BIT_MASK

#### ENCODING FUNCTIONS (2)
- ENCODE (2 args)
- DECODE (2 args)

#### XML FUNCTIONS (9)
- XMLPARSE (2 args)
- XMLSERIALIZE (variable)
- XMLELEMENT (variable)
- XMLCONCAT (variable)
- XMLFOREST (variable)
- XMLCOMMENT (1 arg)
- XMLROOT (3 args)
- XPATH (2 args)
- XMLEXISTS (2 args)
- XMLAGG (aggregate)

#### CONDITIONAL EXPRESSIONS (3)
- COALESCE (variable args)
- NULLIF (2 args)
- CASE WHEN ... (variable args)

#### JSON FUNCTIONS (13)
Extraction (4): JSON_EXTRACT, JSONB_EXTRACT_PATH, ->, ->>
Path ops (2): #>, #>>
Construction (4): JSON_OBJECT, JSON_ARRAY, JSONB_BUILD_OBJECT, JSONB_BUILD_ARRAY
Modification (4): JSON_SET, JSON_INSERT, JSON_REMOVE, JSONB_SET

#### TEXT SEARCH FUNCTIONS (9)
Operators (1): @@ (TSMATCH)
Operations (1): TS_RANK
Types (2): TSVECTOR (type marker), TSQUERY (type marker)
Construction (4): TO_TSVECTOR, TO_TSQUERY, PLAINTO_TSQUERY, PHRASETO_TSQUERY
Aggregate (1): XMLAGG

#### RANGE FUNCTIONS (15)
Type Markers (6): INT4RANGE, INT8RANGE, NUMRANGE, DATERANGE, TSRANGE, TSTZRANGE
Constructor (1): RANGE_CONSTRUCT
Operators (8): &&, @>, @> (elem), <@, <<, >>, -|-, + (union), & (intersection), - (difference)
Accessors (7): LOWER, UPPER, ISEMPTY, LOWER_INC, UPPER_INC, LOWER_INF, UPPER_INF
Utilities (1): RANGE_MERGE

#### CTE (Common Table Expressions) (3)
- WITH clause (0x62)
- CTE definition (0x60)
- CTE scan (0x61)

TOTAL OPCODES DEFINED: 200+ (including extended opcodes)
TOTAL FUNCTIONS: 123+ directly implemented

## 3. EXECUTOR - FUNCTION HANDLERS

File: src/sblr/executor.cpp

### Expression Evaluation Entry Point
Location: executor.cpp:7271 (evaluateExpression)
  - Reads opcode
  - Dispatches to handler based on opcode type
  - Stack-based: pop arguments, execute, push result

### Function Handlers - Key Sections

String Functions: 7399-7562 (FUNC_LENGTH, UPPER, LOWER, TRIM)
Aggregate Functions: 5140-5176 (SUM, AVG, MIN, MAX, COUNT)
Window Functions: 5894-5915 (ROW_NUMBER, RANK, DENSE_RANK, LAG, LEAD, FIRST_VALUE, LAST_VALUE, NTH_VALUE)
JSON Functions: 8254-8589 (EXTRACT, BUILD, SET, INSERT, REMOVE)
Array Functions: 8590-9200 (APPEND, PREPEND, CAT, REMOVE, REPLACE, OVERLAP, CONTAINS, LENGTH, DIMS, UPPER, LOWER)
Spatial Functions: 9355-10500 (POINT, MAKELINE, MAKEPOLYGON, ASTEXT, ASBINARY, GEOMETRYTYPE, ISVALID, BUFFER, CONVEXHULL, ENVELOPE, etc.)
Regex Functions: 10100-10300 (REGEXP_MATCHES, REGEXP_REPLACE, SPLIT_PART, etc.)
String Manipulation: 10310-10560 (INITCAP, ASCII, CHR, REPEAT, REVERSE)
Mathematical Functions: 10562-11437 (SIN, COS, TAN, ASIN, ACOS, ATAN, ATAN2, DEGREES, RADIANS, SQRT, CBRT, ABS, SIGN, ROUND, CEIL, FLOOR, TRUNC, MOD, POWER, EXP, LN, LOG, LOG10, LOG2)
Bit Manipulation: 11439-11975 (GET_BYTE, SET_BYTE, GET_BIT, SET_BIT, BIT_AND, BIT_OR, BIT_XOR, BIT_NOT, BIT_SHIFT_LEFT, BIT_SHIFT_RIGHT, BIT_SHIFT_RIGHT_LOGICAL, BIT_COUNT, BIT_LENGTH, BIT_MASK)
Statistical Functions: 5158-5176 (STDDEV, VAR, CORR, COVAR_POP) [STUBS]
Cryptographic Functions: 11977-12100 (MD5, SHA1, SHA256, SHA512)

### Handling Pattern
Each function:
1. Read argument count (uint8_t)
2. Validate arg count
3. Pop values from stack (in LIFO order)
4. Check for NULL
5. Execute logic
6. Push result

Example (ARRAY_APPEND, line 8590):
```cpp
uint8_t arg_count = readByte();
if (arg_count != 2) error(...);
Value element = pop();
Value array = pop();
if (array.isNull()) push(Value::makeNull());
else {
    // Execute appending logic
    json j_array = json::parse(array.toString());
    j_array.push_back(valueToJSON(element));
    push(Value::makeJSON(j_array.dump()));
}
```

## 4. WHAT'S MISSING - CRITICAL GAPS

### MULTI-GEOMETRY FUNCTIONS (8 functions) - OPCODE DEFINED BUT NOT IMPLEMENTED
Located in opcodes.h but missing from bytecode_generator and executor:

Constructors (4):
- ST_MULTIPOINT (0x87)
- ST_MULTILINESTRING (0x88)
- ST_MULTIPOLYGON (0x89)
- ST_GEOMETRYCOLLECTION (0x8A)
- ST_COLLECT (0x8B) [alias for GeometryCollection]

Accessors (3):
- ST_GEOMETRYN (0x8C) - get Nth geometry from collection
- ST_NUMGEOMETRIES (0x8D) - get count of geometries
- ST_DUMP (0x8E) - dump all geometries from collection

Status: Opcodes exist (opcodes.h:388-398) but:
  - NOT in bytecode_generator.cpp FunctionCallExpr visitor
  - NOT in executor.cpp evaluateExpression dispatch
  - Value class may not have MultiGeometry/GeometryCollection types
  
Estimated effort: 6-8 hours
  - 2 hours: Add MultiGeometry type to Value class
  - 2 hours: Add bytecode generation for 8 functions
  - 2 hours: Implement 8 executor handlers
  - 1 hour: Add tests

### ARRAY SUBSCRIPT OPERATOR - PARTIALLY MISSING
Opcode: None defined (needs EXT_ARRAY_SUBSCRIPT or similar)
Status: Arrays exist as JSON, but bracket notation [n] not implemented
Pattern: SELECT array[1] FROM table; -- Should return first element
Missing: Parser support for subscript syntax, bytecode generation, executor handling
Estimated effort: 4-6 hours
  - 1.5 hours: Parser array subscript syntax
  - 1.5 hours: Bytecode generation
  - 1 hour: Executor implementation
  - 1 hour: Tests

### RANGE TYPE FUNCTIONS - OPCODE DEFINED BUT NOT FULLY IMPLEMENTED
Location: opcodes.h:442-474 (EXT_TYPE_INT4RANGE through EXT_RANGE_DIFFERENCE)
Status: Type markers exist, but no actual range type or handlers
Missing:
- Range data type definition in Value class (struct with lower, upper, bounds flags)
- Constructor function RANGE_CONSTRUCT
- All range operators and accessors
- Range constructor syntax: RANGE(1, 10, '[)')
- Literal syntax: int4range(1, 10, '[]')
Estimated effort: 16-20 hours (complex type system)

### WINDOW FUNCTIONS - PARTIAL IMPLEMENTATION
Opcodes: 0xE2-0xE9 (WIN_ROW_NUMBER through WIN_NTH_VALUE)
Status: Definitions exist in opcodes, executor stubs at 5894-5915
Issue: Full window frame logic not implemented
  - PARTITION BY clause partial
  - ORDER BY within window partial
  - Frame specification (ROWS, RANGE, CURRENT ROW, etc.) stubbed
  - Lead/Lag offset handling incomplete
Estimated effort: 20-30 hours

### STATISTICAL AGGREGATE FUNCTIONS - STUBBED
Functions: STDDEV, VAR, CORR, COVAR_POP (6 functions)
Opcodes: 0x7F-0x84, 0xF3-0xF8
Status: Opcode dispatch exists (executor.cpp:5158-5176) but handlers just accumulate values
Issue: Need proper aggregate accumulator for statistics
  - Mean accumulation
  - Sum of squares
  - Pair-wise correlation calculation
  - Population vs sample logic
Estimated effort: 8-10 hours

### NETWORK TYPE FUNCTIONS - NOT IMPLEMENTED
Types: INET, CIDR, MACADDR
Status: Types defined in opcodes.h (enums only), no functions
Missing:
- Type definitions in Value class
- Constructor functions: INET('192.168.1.1/24'), CIDR(), MACADDR()
- Operators: contains (>>), is contained by (<<), equals
- Functions: host(), netmask(), masklen(), broadcast(), network()
Estimated effort: 12-16 hours

### INTERVAL TYPE FUNCTIONS - LIMITED IMPLEMENTATION
Type: INTERVAL exists (0x2D in opcodes.h)
Status: Partial implementation for date arithmetic
Missing:
- Proper INTERVAL data type (value type)
- INTERVAL literal parsing
- Operators: +, - (with dates, timestamps, intervals)
- Functions: EXTRACT(field FROM interval), DATE_TRUNC
- Arithmetic with different interval units (years, months, days, etc.)
Estimated effort: 10-12 hours

### TEXT SEARCH - PARTIAL IMPLEMENTATION
Functions: TO_TSVECTOR, TO_TSQUERY, PLAINTO_TSQUERY, PHRASETO_TSQUERY, TS_RANK, @@ operator
Status: Type markers and opcodes defined, but:
- TSVECTOR and TSQUERY types not fully implemented as Value types
- Parser integration for TO_TSVECTOR incomplete
- Full text search index integration not done
- Phrase queries and ranking not fully implemented
Estimated effort: 20-25 hours

### AGGREGATE FILTER CLAUSE - NOT IMPLEMENTED
Syntax: SELECT COUNT(*) FILTER (WHERE condition) FROM table;
Status: Not parsed, not in opcodes
Estimated effort: 6-8 hours
  - 2 hours: Parser support
  - 2 hours: Bytecode generation
  - 2 hours: Executor implementation
  - 1 hour: Tests

### LATERAL JOINS - NOT IMPLEMENTED
Syntax: SELECT * FROM table1 t1, LATERAL function_call(t1.col) AS t2
Status: Keyword exists in lexer but parser/executor not done
Estimated effort: 12-16 hours

### USER-DEFINED FUNCTIONS - PARSER ONLY
Status: CREATE FUNCTION parsing done, but:
- PSQL function body bytecode generation incomplete
- Function registration in catalog done but invocation missing
- Stored procedure execution framework stubbed
- Variable declaration, control flow (IF, LOOP, etc.) framework exists but incomplete
Estimated effort: 40-60 hours (significant effort)

## 5. FUNCTION REGISTRY PATTERN

File: src/sblr/executor.cpp:806-848 (initBuiltinFunctions)

BuiltinFunction enum in executor.h defines all 140+ function codes
Registry maps name → enum value:
```cpp
builtins_["LENGTH"] = BuiltinFunction::LENGTH;
builtins_["UPPER"] = BuiltinFunction::UPPER;
// ... 140+ more
```

This registry is used when parsing function calls in expressions.
Note: This is separate from the bytecode generator mapping!

Pattern for adding new function:
1. Add opcode to opcodes.h enum
2. Add BuiltinFunction enum to executor.h
3. Add to builtins_ map in executor.cpp::initBuiltinFunctions()
4. Add bytecode generation in bytecode_generator.cpp::visit(FunctionCallExpr)
5. Add executor handler in executor.cpp::evaluateExpression()
6. Add tests in tests/integration/

## 6. KEY FILES SUMMARY

Architecture & Design:
- /include/scratchbird/sblr/opcodes.h (200+ opcodes)
- /include/scratchbird/sblr/executor.h (BuiltinFunction enum, handlers)
- /include/scratchbird/core/types.h (Value class)

Implementation:
- /src/sblr/bytecode_generator.cpp:1415-3050 (function name → opcode mapping)
- /src/sblr/executor.cpp:7271-12500 (evaluateExpression and handlers)
- /src/core/hash_functions.cpp (MurmurHash64 for indexing)
- /src/core/ts_functions.cpp (text search helpers)

Tests:
- /tests/integration/test_array_functions.cpp
- /tests/integration/test_mathematical_functions.cpp
- /tests/integration/test_statistical_functions.cpp
- /tests/integration/test_spatial_functions.cpp
- /tests/unit/test_json_functions.cpp
- /tests/integration/test_bit_manipulation.cpp
- /tests/unit/test_window_functions.cpp

Documentation:
- /docs/IMPLEMENTATION_AUDIT.md (comprehensive audit)
- /docs/specifications/parser/v3/status/*.md (phase-specific completion reports)

## 7. PATTERNS & BEST PRACTICES

### For Adding a New Function:

Step 1: Opcode Definition (include/scratchbird/sblr/opcodes.h)
```cpp
EXT_NEW_FUNCTION = 0xXX,  // New function description
```

Step 2: Bytecode Generator (src/sblr/bytecode_generator.cpp::visit(FunctionCallExpr))
```cpp
else if (func_name == "NEW_FUNCTION") {
    // Generate arguments
    for (auto *arg : node->args()) {
        generateExpression(arg);
    }
    // Emit opcode
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_NEW_FUNCTION));
    current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
    return;
}
```

Step 3: Executor Handler (src/sblr/executor.cpp::evaluateExpression)
```cpp
else if (ext_op == static_cast<uint8_t>(Opcode::EXT_NEW_FUNCTION)) {
    uint8_t arg_count = readByte();
    if (arg_count != expected_count) {
        error("NEW_FUNCTION expects N arguments");
    }
    // Pop arguments in reverse order
    Value arg1 = pop();
    // ... more pops
    
    // Check for NULL
    if (arg1.isNull() || ...) {
        push(Value::makeNull());
    } else {
        try {
            // Implementation logic
            auto result = executeLogic(arg1, ...);
            push(Value::makeType(result));
        } catch (...) {
            push(Value::makeNull());
        }
    }
}
```

Step 4: Registration (src/sblr/executor.cpp::initBuiltinFunctions) [if needed]
```cpp
builtins_["NEW_FUNCTION"] = BuiltinFunction::NEW_FUNCTION;
```

Step 5: Testing (tests/integration/test_new_functions.cpp)
```cpp
bool testNewFunction() {
    std::string sql = "SELECT NEW_FUNCTION(arg1, arg2) FROM table";
    // Parse, generate bytecode, execute, verify results
    return success;
}
```

### Stack-Based Execution Pattern:
1. Arguments pushed left-to-right during evaluation
2. Pop in reverse (LIFO): last arg pushed = first popped
3. Always check for NULL before operations
4. Return NULL for any invalid operation
5. Use try-catch for defensive programming

### Extended Opcode Pattern:
For functions 256+:
1. Write: EXTENDED_OPCODE (0xFF)
2. Write: ext_opcode_byte (0x01-0xFF)
3. Write: arg_count if variable
4. Arguments already on stack

