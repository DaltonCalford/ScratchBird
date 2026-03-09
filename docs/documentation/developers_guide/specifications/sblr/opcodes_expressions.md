# Specification: SBLR Expression Opcodes

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | sblr |
| **Spec Version** | 1.0.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_opcodes.generated.h:283-481`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:842-844`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/expression_evaluator.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_type_opcodes.cpp`

## Synopsis

This specification defines the Expression evaluation opcodes for SBLR v3. These opcodes handle literal values, column references, arithmetic operations, comparisons, logical operations, function calls, aggregates, and window functions.

## Scope

### In Scope

- Literal value representation
- Arithmetic and mathematical operations
- Comparison operators
- Logical operators (AND, OR, NOT)
- SQL functions (string, math, datetime, etc.)
- Aggregate functions
- Window functions
- Type casting
- Pattern matching (LIKE, regex)
- Conditional expressions (CASE, COALESCE)

### Out of Scope

- Query structure (see opcodes_query.md)
- DML operations (see opcodes_dml.md)
- Index operations (see opcodes_index.md)

## Background

Expression evaluation is central to SQL execution. SBLR uses a stack-based expression evaluator where operands are pushed onto a stack and operators pop operands, compute results, and push the result back.

## Specification

### Expression Opcode Families

| Range | Family |
|-------|--------|
| 0x0701-0x071D | Basic expressions and conditionals |
| 0x0802-0x08CF | SQL functions |
| 0x0901-0x0926 | Aggregate functions |
| 0x0A02-0x0A0C | Window functions |
| 0x0B00-0x0B46 | Type identifiers |
| 0x0C01-0x0C33 | Literal value constructors |

---

### Literal Opcodes (0x0C01-0x0C33)

#### Literal Value Encoding

```cpp
// Source: src/sblr/v3_payloads.cpp:41-115
bool encodeLiteralPayload(uint16_t opcode, const Value& payload, Buffer& out, DecodeError& err);
```

| Opcode | Hex | Type | Encoding |
|--------|-----|------|----------|
| SBLR3_LITERAL_NULL | 0x0C12 | NULL | No payload |
| SBLR3_LITERAL_BOOLEAN | 0x0C09 | BOOLEAN | 1 byte (0/1) |
| SBLR3_LITERAL_INT32 | 0x0C0F | INT32 | 4 bytes LE |
| SBLR3_LITERAL_INT64 | 0x0C10 | INT64 | 8 bytes LE |
| SBLR3_LITERAL_INT8 | 0x0C23 | INT8 | 1 byte |
| SBLR3_LITERAL_INT16 | 0x0C24 | INT16 | 2 bytes LE |
| SBLR3_LITERAL_UINT8 | 0x0C25 | UINT8 | 1 byte |
| SBLR3_LITERAL_UINT16 | 0x0C26 | UINT16 | 2 bytes LE |
| SBLR3_LITERAL_UINT32 | 0x0C27 | UINT32 | 4 bytes LE |
| SBLR3_LITERAL_UINT64 | 0x0C28 | UINT64 | 8 bytes LE |
| SBLR3_LITERAL_INT128 | 0x0C2A | INT128 | 16 bytes LE |
| SBLR3_LITERAL_UINT128 | 0x0C29 | UINT128 | 16 bytes LE |
| SBLR3_LITERAL_FLOAT32 | 0x0C2B | FLOAT32 | 4 bytes IEEE 754 |
| SBLR3_LITERAL_DOUBLE | 0x0C0E | DOUBLE | 8 bytes IEEE 754 |
| SBLR3_LITERAL_DECIMAL | 0x0C0D | DECIMAL | String representation |
| SBLR3_LITERAL_STRING | 0x0C13 | VARCHAR/TEXT | Varint length + UTF-8 |
| SBLR3_LITERAL_BINARY | 0x0C08 | BYTEA/VARBINARY | Varint length + bytes |
| SBLR3_LITERAL_DATE | 0x0C0C | DATE | 4 bytes (days since epoch) |
| SBLR3_LITERAL_TIME | 0x0C14 | TIME | 8 bytes (microseconds) |
| SBLR3_LITERAL_TIME_TZ | 0x0C2C | TIMETZ | 8 bytes + 2 bytes TZ offset |
| SBLR3_LITERAL_TIMESTAMP | 0x0C15 | TIMESTAMP | 8 bytes (microseconds) |
| SBLR3_LITERAL_TIMESTAMP_TZ | 0x0C2D | TIMESTAMPTZ | 8 bytes microseconds + 2 bytes offset |
| SBLR3_LITERAL_DATETIME | 0x0C1A | DATETIME | 8 bytes |
| SBLR3_LITERAL_INTERVAL | 0x0C03 | INTERVAL | Complex structure |
| SBLR3_LITERAL_UUID | 0x0C16 | UUID | 16 bytes |
| SBLR3_LITERAL_JSON | 0x0C11 | JSON | String encoding |
| SBLR3_LITERAL_JSONB | 0x0C04 | JSONB | Binary encoding |
| SBLR3_LITERAL_XML | 0x0C17 | XML | String encoding |
| SBLR3_LITERAL_BIT | 0x0C18 | BIT | Bit string |
| SBLR3_LITERAL_INET | 0x0C02 | INET | 4 or 16 bytes + mask |
| SBLR3_LITERAL_CIDR | 0x0C01 | CIDR | 4 or 16 bytes + mask |
| SBLR3_LITERAL_MACADDR | 0x0C05 | MACADDR | 6 bytes |
| SBLR3_LITERAL_MACADDR8 | 0x0C06 | MACADDR8 | 8 bytes |
| SBLR3_LITERAL_MONEY | 0x0C07 | MONEY | 8 bytes (cents) |
| SBLR3_LITERAL_ARRAY | 0x0C2F | ARRAY | Element count + elements |
| SBLR3_LITERAL_RANGE | 0x0C2E | RANGE | Lower bound + upper bound |
| SBLR3_LITERAL_ENUM | 0x0C1E | ENUM | String label |
| SBLR3_LITERAL_SET | 0x0C1F | SET | Set of labels |
| SBLR3_LITERAL_TSVECTOR | 0x0C31 | TSVECTOR | Lexeme array |
| SBLR3_LITERAL_TSQUERY | 0x0C32 | TSQUERY | Query structure |

**Execution:**
```
All literal opcodes:
  1. Read value from payload
  2. Construct TypedValue
  3. Push onto evaluation stack
```

---

### Arithmetic Opcodes

#### Binary Arithmetic

| Opcode | Hex | Operation |
|--------|-----|-----------|
| SBLR3_EXPR_ADD | 0x0703 | Addition (+) |
| SBLR3_EXPR_SUBTRACT | 0x0712 | Subtraction (-) |
| SBLR3_EXPR_MULTIPLY | 0x070F | Multiplication (*) |
| SBLR3_EXPR_DIVIDE | 0x0706 | Division (/) |
| SBLR3_EXPR_DIV_INT | 0x0713 | Integer division |
| SBLR3_EXPR_MODULO | 0x070E | Modulo (%) |

**Payload:** None (stack-based)

**Execution:**
```cpp
// Source: src/sblr/executor.cpp:844
void executeBinaryOp(Opcode op);
```

```
Input: Two operands on stack (left, right)
Output: Result pushed to stack

1. Pop right operand
2. Pop left operand
3. Promote types if needed
4. Perform operation
5. Handle overflow/underflow
6. Push result
```

**Type Promotion Rules:**
```
INT + INT -> INT
INT + FLOAT -> FLOAT
INT + DECIMAL -> DECIMAL
FLOAT + FLOAT -> FLOAT
DECIMAL + DECIMAL -> DECIMAL
DATE + INTERVAL -> TIMESTAMP
INTERVAL + INTERVAL -> INTERVAL
```

**Special Cases:**
- Division by zero: ERROR or NULL (depending on setting)
- Integer overflow: ERROR or wrap (implementation-defined)
- NULL operand: result is NULL

#### Bitwise Operations

| Opcode | Hex | Operation |
|--------|-----|-----------|
| SBLR3_BIT_AND | 0x0804 | Bitwise AND |
| SBLR3_BIT_OR | 0x080E | Bitwise OR |
| SBLR3_BIT_XOR | 0x0816 | Bitwise XOR |
| SBLR3_BIT_NOT | 0x080C | Bitwise NOT (unary) |
| SBLR3_BIT_SHIFT_LEFT | 0x0810 | Left shift (<<) |
| SBLR3_BIT_SHIFT_RIGHT | 0x0812 | Right shift (>>) |
| SBLR3_BIT_SHIFT_RIGHT_LOGICAL | 0x0814 | Logical right shift |

---

### Comparison Opcodes

| Opcode | Hex | Operation |
|--------|-----|-----------|
| SBLR3_EXPR_EQ | 0x0707 | Equal (=) |
| SBLR3_EXPR_NE | 0x0710 | Not equal (<>) |
| SBLR3_EXPR_LT | 0x070D | Less than (<) |
| SBLR3_EXPR_LE | 0x070B | Less than or equal (<=) |
| SBLR3_EXPR_GT | 0x0709 | Greater than (>) |
| SBLR3_EXPR_GE | 0x0708 | Greater than or equal (>=) |
| SBLR3_NULL_SAFE_EQ | 0x0719 | NULL-safe equal (<=>) |

**Execution:**
```
1. Pop right operand
2. Pop left operand
3. Handle NULL:
   - Regular comparisons: NULL op anything = NULL
   - NULL_SAFE_EQ: treats NULL as comparable value
4. Compare values according to type
5. Push BOOLEAN result (or NULL)
```

**Collation Handling:**
```
String comparisons use collation:
  - Default collation for column type
  - Or explicit COLLATE clause
  - Comparison follows collation sort order
```

---

### Logical Opcodes

| Opcode | Hex | Operation |
|--------|-----|-----------|
| SBLR3_EXPR_AND | 0x0704 | Logical AND |
| SBLR3_EXPR_OR | 0x0711 | Logical OR |
| SBLR3_EXPR_NOT | 0x0716 | Logical NOT |

**Three-Valued Logic:**

| A | B | A AND B | A OR B | NOT A |
|---|---|---------|--------|-------|
| TRUE | TRUE | TRUE | TRUE | FALSE |
| TRUE | FALSE | FALSE | TRUE | FALSE |
| TRUE | NULL | NULL | TRUE | FALSE |
| FALSE | TRUE | FALSE | TRUE | TRUE |
| FALSE | FALSE | FALSE | FALSE | TRUE |
| FALSE | NULL | FALSE | NULL | TRUE |
| NULL | TRUE | NULL | TRUE | NULL |
| NULL | FALSE | FALSE | NULL | NULL |
| NULL | NULL | NULL | NULL | NULL |

**Short-Circuit Evaluation:**
```
AND: If left is FALSE, return FALSE (don't evaluate right)
OR:  If left is TRUE, return TRUE (don't evaluate right)
```

---

### Pattern Matching Opcodes

#### LIKE Opcodes

| Opcode | Hex | Operation |
|--------|-----|-----------|
| SBLR3_EXPR_LIKE | 0x070C | Pattern match (case-sensitive) |
| SBLR3_EXPR_ILIKE | 0x070A | Pattern match (case-insensitive) |
| SBLR3_LIKE_ESCAPE | 0x0718 | LIKE with escape character |
| SBLR3_ILIKE_ESCAPE | 0x0717 | ILIKE with escape character |

**Pattern Syntax:**
- `%` matches any sequence of characters
- `_` matches any single character
- Escape character allows literal `%` and `_`

**Execution:**
```cpp
// Source: src/sblr/executor.cpp:847-851
bool matchPattern(const std::string& text, const std::string& pattern, bool case_insensitive);
```

#### Regular Expression Opcodes

| Opcode | Hex | Operation |
|--------|-----|-----------|
| SBLR3_REGEX_MATCH | 0x100E | Match regex (case-sensitive) |
| SBLR3_REGEX_MATCH_CI | 0x1010 | Match regex (case-insensitive) |
| SBLR3_REGEX_NOT_MATCH | 0x1012 | Does not match regex |
| SBLR3_REGEX_NOT_MATCH_CI | 0x1014 | Does not match regex (CI) |
| SBLR3_REGEXP_MATCHES | 0x1006 | Return all matches |
| SBLR3_REGEXP_REPLACE | 0x1008 | Replace regex matches |
| SBLR3_REGEXP_SPLIT_TO_ARRAY | 0x100A | Split by regex |
| SBLR3_REGEXP_SPLIT_TO_TABLE | 0x100C | Split by regex to rows |

---

### Conditional Expression Opcodes

#### SBLR3_CASE_WHEN (0x0701)

**Purpose**: CASE expression.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| simple_form | uint8_t | 0=searched CASE, 1=simple CASE |
| operand_present | uint8_t | For simple form |
| operand_len | uint32_t | Operand expression length |
| operand | bytes | Operand expression |
| when_count | uint16_t | Number of WHEN clauses |
| when_clauses | WhenClause[] | WHEN...THEN pairs |
| else_present | uint8_t | 0=no, 1=yes |
| else_len | uint32_t | ELSE expression length |
| else_expr | bytes | ELSE expression |

**WhenClause Structure:**
| Field | Type | Description |
|-------|------|-------------|
| condition_len | uint32_t | Condition expression length |
| condition | bytes | Boolean expression |
| result_len | uint32_t | Result expression length |
| result | bytes | Result expression |

**Execution:**
```
Simple CASE:
  1. Evaluate operand
  2. For each WHEN:
     - Compare operand to WHEN value
     - If equal: evaluate and return THEN
  3. If no match: evaluate ELSE (or return NULL)

Searched CASE:
  1. For each WHEN:
     - Evaluate condition
     - If TRUE: evaluate and return THEN
  2. If no match: evaluate ELSE (or return NULL)
```

#### SBLR3_COALESCE (0x0702)

**Purpose**: Return first non-NULL value.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| arg_count | uint16_t | Number of arguments |
| args | bytes[] | Expression bytecodes |

**Execution:**
```
For each argument:
  1. Evaluate expression
  2. If result is not NULL: return it
Return NULL if all are NULL
```

#### SBLR3_NULLIF (0x071D)

**Purpose**: Return NULL if values equal.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| left_len | uint32_t | Left expression length |
| left_expr | bytes | Left expression |
| right_len | uint32_t | Right expression length |
| right_expr | bytes | Right expression |

**Execution:**
```
1. Evaluate left
2. Evaluate right
3. If left = right: return NULL
4. Else: return left
```

---

### Type Casting

#### SBLR3_EXPR_CAST (0x0705)

**Purpose**: Convert value to different type.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| operand_len | uint32_t | Source expression length |
| operand | bytes | Expression bytecode |
| target_type | uint16_t | Target type code |
| precision | uint32_t | For DECIMAL/VARCHAR |
| scale | uint32_t | For DECIMAL |
| using_present | uint8_t | Custom cast function |
| using_name_len | uint16_t | Cast function name length |
| using_name | char[] | Cast function name |

**Cast Rules:**
```
Implicit casts (automatic):
  INT -> FLOAT
  INT -> DECIMAL
  VARCHAR -> TEXT
  TIMESTAMP -> DATE (truncate time)

Explicit casts required:
  FLOAT -> INT (truncation)
  TEXT -> INT (parsing)
  TEXT -> JSON
  Any -> TEXT (formatting)
```

---

### Null Handling

#### SBLR3_EXPR_IS_NULL (0x0715)

**Purpose**: Test for NULL.

**Execution:**
```
1. Pop operand
2. Push TRUE if NULL, FALSE otherwise
```

#### SBLR3_EXPR_IS_NOT_NULL (implicit via IS_NULL + NOT)

---

### SQL Functions (0x0802-0x08CF)

#### String Functions

| Opcode | Hex | Function |
|--------|-----|----------|
| SBLR3_FUNC_LENGTH | 0x08B8 | LENGTH(string) |
| SBLR3_FUNC_CHAR_LENGTH | 0x08B1 | CHAR_LENGTH |
| SBLR3_FUNC_OCTET_LENGTH | 0x08BB | OCTET_LENGTH |
| SBLR3_FUNC_LOWER | 0x08B9 | LOWER |
| SBLR3_FUNC_UPPER | 0x08BE | UPPER |
| SBLR3_FUNC_SUBSTRING | 0x08BC | SUBSTRING |
| SBLR3_FUNC_TRIM | 0x08BD | TRIM |
| SBLR3_FUNC_LTRIM | 0x0857 | LTRIM |
| SBLR3_FUNC_RTRIM | 0x0864 | RTRIM |
| SBLR3_FUNC_REPLACE | 0x0861 | REPLACE |
| SBLR3_FUNC_CONCAT | 0x0837 | Concatenation |
| SBLR3_FUNC_CONCAT_WS | 0x0838 | Concat with separator |
| SBLR3_ASCII | 0x0802 | ASCII code |
| SBLR3_CHR | 0x0818 | Character from code |
| SBLR3_INITCAP | 0x087C | Capitalize words |
| SBLR3_LPAD | 0x087E | Left pad |
| SBLR3_RPAD | 0x088E | Right pad |
| SBLR3_POSITION | 0x0884 | String position |
| SBLR3_STRPOS | 0x089A | String position |
| SBLR3_REPEAT | 0x088A | Repeat string |
| SBLR3_REVERSE | 0x088C | Reverse string |
| SBLR3_SPLIT_PART | 0x0896 | Split and extract |
| SBLR3_MD5 | 0x0880 | MD5 hash |
| SBLR3_SHA1 | 0x0890 | SHA1 hash |
| SBLR3_SHA256 | 0x0892 | SHA256 hash |
| SBLR3_SHA512 | 0x0894 | SHA512 hash |

#### Mathematical Functions

| Opcode | Hex | Function |
|--------|-----|----------|
| SBLR3_FUNC_ABS | 0x0820 | ABS(x) |
| SBLR3_FUNC_CEIL | 0x0835 | CEILING(x) |
| SBLR3_FUNC_FLOOR | 0x0849 | FLOOR(x) |
| SBLR3_FUNC_ROUND | 0x0863 | ROUND(x, d) |
| SBLR3_FUNC_TRUNC | 0x0876 | TRUNC(x) |
| SBLR3_FUNC_SIGN | 0x0867 | SIGN(x) |
| SBLR3_FUNC_SQRT | 0x086D | SQRT(x) |
| SBLR3_FUNC_CBRT | 0x0833 | CBRT(x) |
| SBLR3_FUNC_POWER | 0x085E | POWER(x, y) |
| SBLR3_FUNC_EXP | 0x0847 | EXP(x) |
| SBLR3_FUNC_LN | 0x0850 | LN(x) |
| SBLR3_FUNC_LOG | 0x0852 | LOG(base, x) |
| SBLR3_FUNC_LOG10 | 0x0854 | LOG10(x) |
| SBLR3_FUNC_LOG2 | 0x0856 | LOG2(x) |
| SBLR3_FUNC_MOD | 0x0859 | MOD(x, y) |
| SBLR3_FUNC_PI | 0x085C | PI() |
| SBLR3_FUNC_SIN | 0x0869 | SIN(x) |
| SBLR3_FUNC_COS | 0x083A | COS(x) |
| SBLR3_FUNC_TAN | 0x086F | TAN(x) |
| SBLR3_FUNC_ASIN | 0x0829 | ASIN(x) |
| SBLR3_FUNC_ACOS | 0x0822 | ACOS(x) |
| SBLR3_FUNC_ATAN | 0x082D | ATAN(x) |
| SBLR3_FUNC_ATAN2 | 0x082F | ATAN2(y, x) |
| SBLR3_FUNC_SINH | 0x086B | SINH(x) |
| SBLR3_FUNC_COSH | 0x083C | COSH(x) |
| SBLR3_FUNC_TANH | 0x0871 | TANH(x) |
| SBLR3_FUNC_DEGREES | 0x0844 | DEGREES(x) |
| SBLR3_FUNC_RADIANS | 0x0860 | RADIANS(x) |
| SBLR3_FUNC_GREATEST | 0x084B | GREATEST(...) |
| SBLR3_FUNC_LEAST | 0x084E | LEAST(...) |

#### Date/Time Functions

| Opcode | Hex | Function |
|--------|-----|----------|
| SBLR3_FUNC_CURRENT_DATE | 0x08B4 | CURRENT_DATE |
| SBLR3_FUNC_CURRENT_TIME | 0x0841 | CURRENT_TIME |
| SBLR3_FUNC_NOW | 0x08BA | NOW() |
| SBLR3_FUNC_DATE_ADD | 0x08B5 | DATE_ADD |
| SBLR3_FUNC_DATE_SUB | 0x08B7 | DATE_SUB |
| SBLR3_FUNC_DATE_DIFF | 0x08B6 | DATEDIFF |
| SBLR3_EXTRACT | 0x081E | EXTRACT(field FROM source) |
| SBLR3_FUNC_TO_CHAR | 0x0872 | TO_CHAR (format) |
| SBLR3_FUNC_TO_DATE | 0x0873 | TO_DATE (parse) |
| SBLR3_FUNC_TO_TIMESTAMP | 0x0874 | TO_TIMESTAMP |

---

### Aggregate Functions (0x0901-0x0926)

#### Aggregate Opcodes

| Opcode | Hex | Function |
|--------|-----|----------|
| SBLR3_AGG_COUNT | 0x0904 | COUNT(*) / COUNT(expr) |
| SBLR3_AGG_SUM | 0x0915 | SUM(expr) |
| SBLR3_AGG_AVG | 0x0902 | AVG(expr) |
| SBLR3_AGG_MIN | 0x0909 | MIN(expr) |
| SBLR3_AGG_MAX | 0x0908 | MAX(expr) |
| SBLR3_ARRAY_AGG | 0x0918 | ARRAY_AGG(expr) |
| SBLR3_AGG_STDDEV_SAMP | 0x0914 | STDDEV_SAMP |
| SBLR3_AGG_STDDEV_POP | 0x0913 | STDDEV_POP |
| SBLR3_AGG_VAR_SAMP | 0x0917 | VAR_SAMP |
| SBLR3_AGG_VAR_POP | 0x0916 | VAR_POP |
| SBLR3_AGG_CORR | 0x0903 | CORR(y, x) |
| SBLR3_AGG_COVAR_POP | 0x0905 | COVAR_POP |
| SBLR3_AGG_REGR_SLOPE | 0x090F | REGR_SLOPE |
| SBLR3_AGG_REGR_INTERCEPT | 0x090D | REGR_INTERCEPT |
| SBLR3_AGG_REGR_R2 | 0x090E | REGR_R2 |
| SBLR3_AGG_REGR_COUNT | 0x090C | REGR_COUNT |
| SBLR3_AGG_REGR_AVGX | 0x090A | REGR_AVGX |
| SBLR3_AGG_REGR_AVGY | 0x090B | REGR_AVGY |
| SBLR3_AGG_REGR_SXX | 0x0910 | REGR_SXX |
| SBLR3_AGG_REGR_SYY | 0x0912 | REGR_SYY |
| SBLR3_AGG_REGR_SXY | 0x0911 | REGR_SXY |

**Aggregate Execution:**

```cpp
// Source: src/sblr/executor.h:881-922
struct AggregateAccumulator {
    AggFunc func;
    bool distinct;
    core::DataType input_type;
    Value result;
    int64_t count;
    double sum;
    // ... statistical state
    void accumulate(const Value& val);
    void accumulate2(const Value& val1, const Value& val2);
    Value finalize();
};
```

**Execution Model:**
```
Without GROUP BY (scalar aggregate):
  1. Initialize single accumulator
  2. Scan all rows
  3. For each row: accumulate value
  4. Finalize and return single result

With GROUP BY:
  1. Initialize hash map: GroupKey -> Accumulator[]
  2. Scan all rows
  3. For each row:
     a. Compute group key
     b. Find/create accumulator set for key
     c. Accumulate values
  4. For each group: finalize, output row
```

---

### Window Functions (0x0A02-0x0A0C)

#### Window Opcodes

| Opcode | Hex | Function |
|--------|-----|----------|
| SBLR3_WIN_ROW_NUMBER | 0x0A0C | ROW_NUMBER() |
| SBLR3_WIN_RANK | 0x0A0B | RANK() |
| SBLR3_WIN_DENSE_RANK | 0x0A05 | DENSE_RANK() |
| SBLR3_WIN_PERCENT_RANK | 0x0A04 | PERCENT_RANK() |
| SBLR3_WIN_CUME_DIST | 0x0A02 | CUME_DIST() |
| SBLR3_WIN_FIRST_VALUE | 0x0A06 | FIRST_VALUE(expr) |
| SBLR3_WIN_LAST_VALUE | 0x0A08 | LAST_VALUE(expr) |
| SBLR3_WIN_NTH_VALUE | 0x0A0A | NTH_VALUE(expr, n) |
| SBLR3_WIN_LAG | 0x0A07 | LAG(expr, offset, default) |
| SBLR3_WIN_LEAD | 0x0A09 | LEAD(expr, offset, default) |

**Window Function Execution:**

```cpp
// Source: src/sblr/executor.cpp:839
void executeWindow(std::unique_ptr<ResultSet> input_result_set);

// Source: src/sblr/executor.h:941-972
struct WindowFunctionSpec {
    FuncType func_type;
    std::vector<Value> args;
    std::vector<size_t> partition_cols;
    std::vector<size_t> order_cols;
    bool has_frame;
    FrameMode frame_mode;
    // ...
};
```

**Execution Model:**
```
1. Identify partitions (PARTITION BY)
2. Sort rows within each partition (ORDER BY)
3. For each row:
   a. Determine frame (ROWS/RANGE/GROUPS)
   b. Compute function over frame
   c. Store result
4. Return results with window values
```

**Frame Boundaries:**
```
ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
  -> All rows from partition start to current

RANGE BETWEEN 2 PRECEDING AND 2 FOLLOWING
  -> Rows within 2 units of current value

ROWS BETWEEN CURRENT ROW AND UNBOUNDED FOLLOWING
  -> Current row through end of partition
```

### Invariants

1. **Type Safety**: Operations check type compatibility
2. **NULL Propagation**: Most operations return NULL if any operand is NULL
3. **Stack Balance**: Expression evaluation maintains stack integrity
4. **No Side Effects**: Expression evaluation does not modify database state

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `E_DIVISION_BY_ZERO` | Division by zero | Check divisor or use NULLIF |
| `E_NUMERIC_VALUE_OUT_OF_RANGE` | Overflow/underflow | Use larger type or bounds check |
| `E_INVALID_TEXT_REPRESENTATION` | Cast parse failure | Validate input format |
| `E_DATETIME_FIELD_OVERFLOW` | Date arithmetic overflow | Use valid date ranges |

## Related Specifications

- [opcodes_query.md](./opcodes_query.md) - Query structure
- [v3_payload_schemas.md](./v3_payload_schemas.md) - Encoding details

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
