# ScratchBird Parser Token Reference

Complete reference of all token types, operators, keywords, and data types in the ScratchBird SQL parser.

**Generated:** 2025-12-06
**Source Files:**
- `/include/scratchbird/parser/token.h` (TokenType enum)
- `/src/parser/token.cpp` (Token type to string mappings)
- `/src/parser/lexer.cpp` (Keyword mappings and lexer rules)

---

## Table of Contents

1. [Special Tokens](#special-tokens)
2. [Literals](#literals)
3. [Identifiers](#identifiers)
4. [Arithmetic Operators](#arithmetic-operators)
5. [Comparison Operators](#comparison-operators)
6. [Punctuation](#punctuation)
7. [JSON Operators](#json-operators)
8. [Array Operators](#array-operators)
9. [Range Operators](#range-operators)
10. [Regex Operators](#regex-operators)
11. [SQL Command Keywords](#sql-command-keywords)
12. [DML Keywords](#dml-keywords)
13. [JOIN Keywords](#join-keywords)
14. [Aggregation Keywords](#aggregation-keywords)
15. [Window Function Keywords](#window-function-keywords)
16. [Aggregate Functions](#aggregate-functions)
17. [Window Functions](#window-functions)
18. [Data Type Keywords - Numeric](#data-type-keywords---numeric)
19. [Data Type Keywords - String](#data-type-keywords---string)
20. [Data Type Keywords - Binary](#data-type-keywords---binary)
21. [Data Type Keywords - Date/Time](#data-type-keywords---datetime)
22. [Data Type Keywords - Boolean](#data-type-keywords---boolean)
23. [Data Type Keywords - Special](#data-type-keywords---special)
24. [Data Type Keywords - Spatial](#data-type-keywords---spatial)
25. [Data Type Keywords - Range](#data-type-keywords---range)
26. [JSON Functions](#json-functions)
27. [Conditional Functions](#conditional-functions)
28. [Array Functions](#array-functions)
29. [Type Conversion](#type-conversion)
30. [Text Functions](#text-functions)
31. [Pattern Matching](#pattern-matching)
32. [Character Set and Collation](#character-set-and-collation)
33. [Timezone Keywords](#timezone-keywords)
34. [Transaction Control](#transaction-control)
35. [Database Maintenance](#database-maintenance)
36. [Tablespace Management](#tablespace-management)
37. [DDL Keywords](#ddl-keywords)
38. [Constraint Keywords](#constraint-keywords)
39. [Subquery Keywords](#subquery-keywords)
40. [Trigger Keywords](#trigger-keywords)
41. [Stored Procedure Keywords](#stored-procedure-keywords)
42. [Security Keywords](#security-keywords)
43. [SQL Engine Commands](#sql-engine-commands)
44. [UPSERT Keywords](#upsert-keywords)
45. [Advanced SQL Keywords](#advanced-sql-keywords)
46. [User Defined Types](#user-defined-types)
47. [Extended SHOW/SET Keywords](#extended-showset-keywords)
48. [Schema Navigation Keywords](#schema-navigation-keywords)
49. [Operator Precedence](#operator-precedence)
50. [Reserved vs Contextual Keywords](#reserved-vs-contextual-keywords)

---

## Special Tokens

| Token Type | String Representation | Category | Description |
|------------|----------------------|----------|-------------|
| `END_OF_FILE` | `"EOF"` | Special | End of input stream |
| `ERROR` | `"ERROR"` | Special | Lexer/parser error token |
| `KEYWORD` | `"KEYWORD"` | Special | Generic keyword (unused) |

---

## Literals

| Token Type | String Representation | Category | Description |
|------------|----------------------|----------|-------------|
| `INTEGER_LITERAL` | `"INTEGER"` | Literal | Integer numeric literal (64-bit signed) |
| `FLOAT_LITERAL` | `"FLOAT"` | Literal | Floating-point literal (double precision) |
| `STRING_LITERAL` | `"STRING"` | Literal | String literal in single quotes |

---

## Identifiers

| Token Type | String Representation | Category | Description |
|------------|----------------------|----------|-------------|
| `IDENTIFIER` | `"IDENTIFIER"` | Identifier | Unquoted or quoted identifier |

**Notes:**
- Unquoted identifiers: case-insensitive (stored as-is, compared UPPER)
- Quoted identifiers (double-quoted): case-sensitive, delimited
- Maximum length: 128 characters (not bytes)
- UTF-8 validated
- Can contain letters, digits, underscores (must start with letter or underscore)

---

## Arithmetic Operators

| Token Type | String Representation | Category | Precedence | Description |
|------------|----------------------|----------|------------|-------------|
| `PLUS` | `"+"` | Arithmetic | 4 | Addition |
| `MINUS` | `"-"` | Arithmetic | 4 | Subtraction (also unary negation) |
| `STAR` | `"*"` | Arithmetic | 5 | Multiplication (also SELECT *) |
| `SLASH` | `"/"` | Arithmetic | 5 | Division |
| `PERCENT` | `"%"` | Arithmetic | 5 | Modulo |

---

## Comparison Operators

| Token Type | String Representation | Category | Precedence | Description |
|------------|----------------------|----------|------------|-------------|
| `EQUAL` | `"="` | Comparison | 3 | Equality |
| `NOT_EQUAL` | `"<>"` | Comparison | 3 | Inequality |
| `LESS_THAN` | `"<"` | Comparison | 3 | Less than |
| `GREATER_THAN` | `">"` | Comparison | 3 | Greater than |
| `LESS_EQUAL` | `"<="` | Comparison | 3 | Less than or equal |
| `GREATER_EQUAL` | `">="` | Comparison | 3 | Greater than or equal |

---

## Punctuation

| Token Type | String Representation | Category | Description |
|------------|----------------------|----------|-------------|
| `LEFT_PAREN` | `"("` | Punctuation | Left parenthesis |
| `RIGHT_PAREN` | `")"` | Punctuation | Right parenthesis |
| `COMMA` | `","` | Punctuation | Comma separator |
| `SEMICOLON` | `";"` | Punctuation | Statement terminator |
| `DOT` | `"."` | Punctuation | Member access, schema qualification |
| `COLON` | `":"` | Punctuation | Type cast (::), parameter marker |
| `COLON_EQUALS` | `":="` | Punctuation | PL/SQL assignment operator (WP-6 PARSE-2) |

---

## JSON Operators

Phase 1 Task 7 - PostgreSQL-compatible JSON operators.

| Token Type | String Representation | Category | Precedence | Description |
|------------|----------------------|----------|------------|-------------|
| `ARROW` | `"->"` | JSON | 6 (postfix) | Extract JSON field as JSON |
| `DOUBLE_ARROW` | `"->>"` | JSON | 6 (postfix) | Extract JSON field as text |
| `HASH_ARROW` | `"#>"` | JSON | 6 (postfix) | Extract JSON path as JSON |
| `HASH_DOUBLE_ARROW` | `"#>>"` | JSON | 6 (postfix) | Extract JSON path as text |

**Notes:**
- JSON operators are handled as postfix operators in `parseFactor()`
- Applied left-to-right: `data->'field'->'nested'`

---

## Array Operators

Phase 2 Task 12 - PostgreSQL-compatible array operators.

| Token Type | String Representation | Category | Precedence | Description |
|------------|----------------------|----------|------------|-------------|
| `AMPERSAND_AMPERSAND` | `"&&"` | Array | 3 | Array overlap (also range overlap) |
| `AT_GREATER` | `"@>"` | Array | 3 | Array contains (also range contains) |
| `LESS_AT` | `"<@"` | Array | 3 | Array contained by (also range contained by) |
| `LEFT_BRACKET` | `"["` | Array | N/A | Array literal start, array subscript, range bound |
| `RIGHT_BRACKET` | `"]"` | Array | N/A | Array literal end, array subscript, range bound |

---

## Range Operators

Task 15 Phase 4 - PostgreSQL-compatible range operators.

| Token Type | String Representation | Category | Precedence | Description |
|------------|----------------------|----------|------------|-------------|
| `SHIFT_LEFT` | `"<<"` | Range | 3 | Strictly left of |
| `SHIFT_RIGHT` | `">>"` | Range | 3 | Strictly right of |
| `MINUS_PIPE_MINUS` | `"-\|-"` | Range | 3 | Adjacent ranges |

**Note:** `AMPERSAND_AMPERSAND`, `AT_GREATER`, and `LESS_AT` are shared between array and range types.

---

## Regex Operators

Phase 2 Task 13 - PostgreSQL-compatible regex matching operators.

| Token Type | String Representation | Category | Precedence | Description |
|------------|----------------------|----------|------------|-------------|
| `TILDE` | `"~"` | Regex | 3 | Regex match (case-sensitive) |
| `TILDE_STAR` | `"~*"` | Regex | 3 | Regex match (case-insensitive) |
| `EXCLAIM_TILDE` | `"!~"` | Regex | 3 | Regex not match (case-sensitive) |
| `EXCLAIM_TILDE_STAR` | `"!~*"` | Regex | 3 | Regex not match (case-insensitive) |

---

## SQL Command Keywords

Core SQL statement types.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_CREATE` | `"CREATE"` | Yes | Create database object |
| `KW_ALTER` | `"ALTER"` | Yes | Alter database object |
| `KW_DROP` | `"DROP"` | Yes | Drop database object |
| `KW_TRUNCATE` | `"TRUNCATE"` | Yes | Truncate table (ALPHA Phase 1) |
| `KW_SHOW` | `"SHOW"` | Yes | Show database information |
| `KW_DESCRIBE` | `"DESCRIBE"` | Yes | Describe table structure |
| `KW_EXPLAIN` | `"EXPLAIN"` | Yes | Show query execution plan |
| `KW_ANALYZE` | `"ANALYZE"` | Yes | Collect statistics |

---

## DML Keywords

Data manipulation language.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_SELECT` | `"SELECT"` | Yes | Select data |
| `KW_INSERT` | `"INSERT"` | Yes | Insert data |
| `KW_UPDATE` | `"UPDATE"` | Yes | Update data |
| `KW_DELETE` | `"DELETE"` | Yes | Delete data |
| `KW_MERGE` | `"MERGE"` | Yes | Merge (upsert) data (ALPHA) |
| `KW_INTO` | `"INTO"` | Yes | Target table |
| `KW_VALUES` | `"VALUES"` | Yes | Value list |
| `KW_FROM` | `"FROM"` | Yes | Source table |
| `KW_WHERE` | `"WHERE"` | Yes | Filter condition |
| `KW_SET` | `"SET"` | Yes | Update assignment |
| `KW_RETURNING` | `"RETURNING"` | Yes | Return modified rows (ALPHA) |

---

## JOIN Keywords

Phase 1 Task 3.1 - Join operations.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_JOIN` | `"JOIN"` | Yes | Join tables |
| `KW_INNER` | `"INNER"` | Yes | Inner join |
| `KW_LEFT` | `"LEFT"` | Yes | Left outer join |
| `KW_RIGHT` | `"RIGHT"` | Yes | Right outer join |
| `KW_FULL` | `"FULL"` | Yes | Full outer join |
| `KW_OUTER` | `"OUTER"` | Yes | Outer join modifier |
| `KW_CROSS` | `"CROSS"` | Yes | Cross join (cartesian product) |
| `KW_NATURAL` | `"NATURAL"` | Yes | Natural join |
| `KW_USING` | `"USING"` | Yes | Join column list |
| `KW_ON` | `"ON"` | Yes | Join condition |
| `KW_LATERAL` | `"LATERAL"` | Yes | Lateral join (correlated subquery) |

---

## Aggregation Keywords

Phase 1 Task 4.1 - Grouping and ordering.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_GROUP` | `"GROUP"` | Yes | Group by clause |
| `KW_BY` | `"BY"` | Yes | Group/order by modifier |
| `KW_HAVING` | `"HAVING"` | Yes | Filter grouped results |
| `KW_ORDER` | `"ORDER"` | Yes | Order by clause |
| `KW_ASC` | `"ASC"` | Yes | Ascending order |
| `KW_DESC` | `"DESC"` | Yes | Descending order |
| `KW_LIMIT` | `"LIMIT"` | Yes | Limit result count |
| `KW_OFFSET` | `"OFFSET"` | Yes | Skip initial rows |
| `KW_DISTINCT` | `"DISTINCT"` | Yes | Remove duplicates |
| `KW_ALL` | `"ALL"` | Yes | Include all (default) |
| `KW_ROLLUP` | `"ROLLUP"` | Yes | Hierarchical grouping |
| `KW_CUBE` | `"CUBE"` | Yes | Multi-dimensional grouping |
| `KW_GROUPING` | `"GROUPING"` | Yes | Grouping sets function |
| `KW_SETS` | `"SETS"` | Yes | Grouping sets |

---

## Window Function Keywords

Phase 1 Task 6 - Window functions and frames.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_OVER` | `"OVER"` | Yes | Window function clause |
| `KW_PARTITION` | `"PARTITION"` | Yes | Partition by clause |
| `KW_ROWS` | `"ROWS"` | Yes | Row-based window frame |
| `KW_RANGE` | `"RANGE"` | Yes | Range-based window frame |
| `KW_GROUPS` | `"GROUPS"` | Yes | Groups-based window frame (P2-9) |
| `KW_BETWEEN` | `"BETWEEN"` | Yes | Frame boundary |
| `KW_UNBOUNDED` | `"UNBOUNDED"` | Yes | Unbounded frame edge |
| `KW_PRECEDING` | `"PRECEDING"` | Yes | Rows before current |
| `KW_FOLLOWING` | `"FOLLOWING"` | Yes | Rows after current |
| `KW_CURRENT` | `"CURRENT"` | Yes | Current row |
| `KW_ROW` | `"ROW"` | Yes | Single row |
| `KW_NULLS` | `"NULLS"` | Yes | Nulls first/last |
| `KW_FIRST` | `"FIRST"` | Yes | First value/nulls first |
| `KW_LAST` | `"LAST"` | Yes | Last value/nulls last |

---

## Aggregate Functions

Phase 1 Task 4.1 - Standard aggregate functions.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_COUNT` | `"COUNT"` | No | Count rows |
| `KW_SUM` | `"SUM"` | No | Sum values |
| `KW_AVG` | `"AVG"` | No | Average values |
| `KW_MIN` | `"MIN"` | No | Minimum value |
| `KW_MAX` | `"MAX"` | No | Maximum value |
| `KW_ARRAY_AGG` | `"ARRAY_AGG"` | No | Aggregate into array (Phase 2 Task 12) |
| `KW_STRING_AGG` | `"STRING_AGG"` | No | Concatenate strings with delimiter |
| `KW_GROUP_CONCAT` | `"GROUP_CONCAT"` | No | MySQL alias for STRING_AGG |

**Additional Aggregate Keywords:**

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_FILTER` | `"FILTER"` | Yes | Filter clause for aggregates |
| `KW_WITHIN` | `"WITHIN"` | Yes | WITHIN GROUP clause |

---

## Window Functions

Phase 1 Task 6 - Window-specific functions.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_ROW_NUMBER` | `"ROW_NUMBER"` | No | Sequential row number |
| `KW_RANK` | `"RANK"` | No | Rank with gaps |
| `KW_DENSE_RANK` | `"DENSE_RANK"` | No | Rank without gaps |
| `KW_LAG` | `"LAG"` | No | Previous row value |
| `KW_LEAD` | `"LEAD"` | No | Next row value |
| `KW_FIRST_VALUE` | `"FIRST_VALUE"` | No | First value in frame |
| `KW_LAST_VALUE` | `"LAST_VALUE"` | No | Last value in frame |
| `KW_NTH_VALUE` | `"NTH_VALUE"` | No | Nth value in frame |
| `KW_CUME_DIST` | `"CUME_DIST"` | No | Cumulative distribution |
| `KW_PERCENT_RANK` | `"PERCENT_RANK"` | No | Relative rank |
| `KW_NTILE` | `"NTILE"` | No | Divide rows into buckets |

---

## Data Type Keywords - Numeric

SQL numeric data types.

| Token Type | String Representation | Reserved | SQL Type | Description |
|------------|----------------------|----------|----------|-------------|
| `KW_INT` | `"INT"` | No | INTEGER | 32-bit integer |
| `KW_INTEGER` | `"INTEGER"` | No | INTEGER | 32-bit integer |
| `KW_SMALLINT` | `"SMALLINT"` | No | SMALLINT | 16-bit integer |
| `KW_BIGINT` | `"BIGINT"` | No | BIGINT | 64-bit integer |
| `KW_TINYINT` | `"TINYINT"` | No | TINYINT | 8-bit integer |
| `KW_INT128` | `"INT128"` | No | INT128 | 128-bit integer |
| `KW_UINT8` | `"UINT8"` | No | UINT8 | 8-bit unsigned integer |
| `KW_UINT16` | `"UINT16"` | No | UINT16 | 16-bit unsigned integer |
| `KW_UINT32` | `"UINT32"` | No | UINT32 | 32-bit unsigned integer |
| `KW_UINT64` | `"UINT64"` | No | UINT64 | 64-bit unsigned integer |
| `KW_REAL` | `"REAL"` | No | REAL | Single precision float |
| `KW_FLOAT` | `"FLOAT"` | No | FLOAT | Floating point |
| `KW_DOUBLE` | `"DOUBLE"` | No | DOUBLE | Double precision float |
| `KW_DECIMAL` | `"DECIMAL"` | No | DECIMAL | Fixed-point decimal |
| `KW_NUMERIC` | `"NUMERIC"` | No | NUMERIC | Fixed-point numeric |
| `KW_MONEY` | `"MONEY"` | No | MONEY | Currency type |

---

## Data Type Keywords - String

SQL string and character data types.

| Token Type | String Representation | Reserved | SQL Type | Description |
|------------|----------------------|----------|----------|-------------|
| `KW_CHAR` | `"CHAR"` | No | CHAR | Fixed-length character |
| `KW_CHARACTER` | `"CHARACTER"` | No | CHARACTER | Fixed-length character |
| `KW_VARCHAR` | `"VARCHAR"` | No | VARCHAR | Variable-length character |
| `KW_TEXT` | `"TEXT"` | No | TEXT | Unlimited text |

---

## Data Type Keywords - Binary

SQL binary data types.

| Token Type | String Representation | Reserved | SQL Type | Description |
|------------|----------------------|----------|----------|-------------|
| `KW_BINARY` | `"BINARY"` | No | BINARY | Fixed-length binary |
| `KW_VARBINARY` | `"VARBINARY"` | No | VARBINARY | Variable-length binary |
| `KW_BLOB` | `"BLOB"` | No | BLOB | Binary large object |
| `KW_BYTEA` | `"BYTEA"` | No | BYTEA | Byte array (PostgreSQL) |

---

## Data Type Keywords - Date/Time

SQL temporal data types.

| Token Type | String Representation | Reserved | SQL Type | Description |
|------------|----------------------|----------|----------|-------------|
| `KW_DATE` | `"DATE"` | No | DATE | Calendar date |
| `KW_TIME` | `"TIME"` | No | TIME | Time of day |
| `KW_TIMESTAMP` | `"TIMESTAMP"` | No | TIMESTAMP | Date and time |
| `KW_INTERVAL` | `"INTERVAL"` | No | INTERVAL | Time interval |

---

## Data Type Keywords - Boolean

SQL boolean data type.

| Token Type | String Representation | Reserved | SQL Type | Description |
|------------|----------------------|----------|----------|-------------|
| `KW_BOOLEAN` | `"BOOLEAN"` | No | BOOLEAN | Boolean value |
| `KW_BOOL` | `"BOOL"` | No | BOOL | Boolean value (alias) |

---

## Data Type Keywords - Special

Special and semi-structured data types.

| Token Type | String Representation | Reserved | SQL Type | Description |
|------------|----------------------|----------|----------|-------------|
| `KW_UUID` | `"UUID"` | No | UUID | Universally unique identifier |
| `KW_JSON` | `"JSON"` | No | JSON | JSON document |
| `KW_JSONB` | `"JSONB"` | No | JSONB | Binary JSON (PostgreSQL) |
| `KW_XML` | `"XML"` | No | XML | XML document |
| `KW_VECTOR` | `"VECTOR"` | No | VECTOR | Vector embedding |
| `KW_ARRAY` | `"ARRAY"` | No | ARRAY | Array type (Phase 2 Task 12) |

---

## Data Type Keywords - Spatial

Spatial/geometry data types (Type Integration Phase 3).

| Token Type | String Representation | Reserved | SQL Type | Description |
|------------|----------------------|----------|----------|-------------|
| `KW_POINT` | `"POINT"` | No | POINT | Geometric point |
| `KW_LINESTRING` | `"LINESTRING"` | No | LINESTRING | Line segment |
| `KW_POLYGON` | `"POLYGON"` | No | POLYGON | Polygon |
| `KW_MULTIPOINT` | `"MULTIPOINT"` | No | MULTIPOINT | Multiple points |
| `KW_MULTILINESTRING` | `"MULTILINESTRING"` | No | MULTILINESTRING | Multiple line segments |
| `KW_MULTIPOLYGON` | `"MULTIPOLYGON"` | No | MULTIPOLYGON | Multiple polygons |
| `KW_GEOMETRYCOLLECTION` | `"GEOMETRYCOLLECTION"` | No | GEOMETRYCOLLECTION | Mixed geometry collection |

---

## Data Type Keywords - Range

Range data types (Task 15 Phase 4).

| Token Type | String Representation | Reserved | SQL Type | Description |
|------------|----------------------|----------|----------|-------------|
| `KW_INT4RANGE` | `"INT4RANGE"` | No | INT4RANGE | Integer range (32-bit) |
| `KW_INT8RANGE` | `"INT8RANGE"` | No | INT8RANGE | Integer range (64-bit) |
| `KW_NUMRANGE` | `"NUMRANGE"` | No | NUMRANGE | Numeric range |
| `KW_DATERANGE` | `"DATERANGE"` | No | DATERANGE | Date range |
| `KW_TSRANGE` | `"TSRANGE"` | No | TSRANGE | Timestamp range |
| `KW_TSTZRANGE` | `"TSTZRANGE"` | No | TSTZRANGE | Timestamp with timezone range |

---

## JSON Functions

Phase 1 Task 7 - JSON manipulation functions.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_JSON_EXTRACT` | `"JSON_EXTRACT"` | No | Extract JSON value |
| `KW_JSON_OBJECT` | `"JSON_OBJECT"` | No | Build JSON object |
| `KW_JSON_ARRAY` | `"JSON_ARRAY"` | No | Build JSON array |
| `KW_JSON_SET` | `"JSON_SET"` | No | Set JSON value |
| `KW_JSON_INSERT` | `"JSON_INSERT"` | No | Insert JSON value |
| `KW_JSON_REMOVE` | `"JSON_REMOVE"` | No | Remove JSON value |
| `KW_JSONB_EXTRACT_PATH` | `"JSONB_EXTRACT_PATH"` | No | Extract JSONB path |
| `KW_JSONB_BUILD_OBJECT` | `"JSONB_BUILD_OBJECT"` | No | Build JSONB object |
| `KW_JSONB_BUILD_ARRAY` | `"JSONB_BUILD_ARRAY"` | No | Build JSONB array |
| `KW_JSONB_SET` | `"JSONB_SET"` | No | Set JSONB value |

---

## Conditional Functions

Phase 1 Task 8 - Conditional expressions.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_COALESCE` | `"COALESCE"` | No | Return first non-null |
| `KW_NULLIF` | `"NULLIF"` | No | Return null if equal |
| `KW_CASE` | `"CASE"` | Yes | Case expression |
| `KW_WHEN` | `"WHEN"` | Yes | Case condition |
| `KW_THEN` | `"THEN"` | Yes | Case result |
| `KW_ELSE` | `"ELSE"` | Yes | Case default |
| `KW_END` | `"END"` | Yes | Case/block terminator |

---

## Array Functions

Phase 2 Task 12 - Array manipulation functions.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_ARRAY_TO_STRING` | `"ARRAY_TO_STRING"` | No | Convert array to string |
| `KW_STRING_TO_ARRAY` | `"STRING_TO_ARRAY"` | No | Convert string to array |
| `KW_ARRAY_APPEND` | `"ARRAY_APPEND"` | No | Append element |
| `KW_ARRAY_PREPEND` | `"ARRAY_PREPEND"` | No | Prepend element |
| `KW_ARRAY_CAT` | `"ARRAY_CAT"` | No | Concatenate arrays |
| `KW_ARRAY_REMOVE` | `"ARRAY_REMOVE"` | No | Remove element |
| `KW_ARRAY_REPLACE` | `"ARRAY_REPLACE"` | No | Replace element |
| `KW_ARRAY_LENGTH` | `"ARRAY_LENGTH"` | No | Get array length |
| `KW_ARRAY_DIMS` | `"ARRAY_DIMS"` | No | Get array dimensions |
| `KW_ARRAY_UPPER` | `"ARRAY_UPPER"` | No | Get upper bound |
| `KW_ARRAY_LOWER` | `"ARRAY_LOWER"` | No | Get lower bound |
| `KW_UNNEST` | `"UNNEST"` | No | Expand array to rows |

---

## Type Conversion

Type casting and conversion.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_CAST` | `"CAST"` | Yes | Type cast (CAST(x AS type)) |
| `KW_TRY_CAST` | `"TRY_CAST"` | Yes | Safe type cast (returns NULL on error) |
| `KW_AS` | `"AS"` | Yes | Cast/alias modifier |

---

## Text Functions

Text manipulation functions with special syntax.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_EXTRACT` | `"EXTRACT"` | Yes | Extract date/time field |
| `KW_POSITION` | `"POSITION"` | Yes | Find substring position (POSITION(x IN y)) |
| `KW_OVERLAY` | `"OVERLAY"` | Yes | Replace substring (OVERLAY(...PLACING...)) |
| `KW_PLACING` | `"PLACING"` | Yes | OVERLAY replacement clause |

---

## Pattern Matching

Pattern matching operators.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_LIKE` | `"LIKE"` | Yes | Case-sensitive pattern match |
| `KW_ILIKE` | `"ILIKE"` | Yes | Case-insensitive pattern match |

**Note:** See also [Regex Operators](#regex-operators) for regex-based pattern matching.

---

## Character Set and Collation

Character set and collation control.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_COLLATE` | `"COLLATE"` | Yes | Specify collation |
| `KW_COLLATION` | `"COLLATION"` | Yes | Collation object |
| `KW_DEFAULT` | `"DEFAULT"` | Yes | Default value |

---

## Timezone Keywords

Timezone handling.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_ZONE` | `"ZONE"` | Yes | Time zone |
| `KW_WITH` | `"WITH"` | Yes | WITH modifier (timezone, CTE, etc.) |
| `KW_RECURSIVE` | `"RECURSIVE"` | Yes | Recursive CTE |
| `KW_WITHOUT` | `"WITHOUT"` | Yes | WITHOUT modifier |
| `KW_AT` | `"AT"` | Yes | AT TIME ZONE |

---

## Transaction Control

Phase 2 Task 2.6, Phase 3 Task 3.6 - Transaction management (Firebird-style).

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_START` | `"START"` | Yes | Start transaction |
| `KW_TRANSACTION` | `"TRANSACTION"` | Yes | Transaction keyword |
| `KW_COMMIT` | `"COMMIT"` | Yes | Commit transaction |
| `KW_ROLLBACK` | `"ROLLBACK"` | Yes | Rollback transaction |
| `KW_READ` | `"READ"` | Yes | Read-only transaction |
| `KW_WRITE` | `"WRITE"` | Yes | Read-write transaction |
| `KW_ONLY` | `"ONLY"` | Yes | Read/write only modifier |
| `KW_WAIT` | `"WAIT"` | Yes | Wait on lock |
| `KW_ISOLATION` | `"ISOLATION"` | Yes | Isolation level |
| `KW_LEVEL` | `"LEVEL"` | Yes | Level modifier |
| `KW_COMMITTED` | `"COMMITTED"` | Yes | Read committed |
| `KW_SNAPSHOT` | `"SNAPSHOT"` | Yes | Snapshot isolation |
| `KW_STABILITY` | `"STABILITY"` | Yes | Snapshot table stability |
| `KW_RESERVING` | `"RESERVING"` | Yes | Table reservation |
| `KW_SHARED` | `"SHARED"` | Yes | Shared lock |
| `KW_PROTECTED` | `"PROTECTED"` | Yes | Protected lock |
| `KW_FOR` | `"FOR"` | Yes | FOR modifier |
| `KW_OUTSTANDING` | `"OUTSTANDING"` | Yes | Outstanding transactions |
| `KW_LOCK` | `"LOCK"` | Yes | Lock modifier |
| `KW_TIMEOUT` | `"TIMEOUT"` | Yes | Lock timeout |
| `KW_SAVEPOINT` | `"SAVEPOINT"` | Yes | Create savepoint |
| `KW_RELEASE` | `"RELEASE"` | Yes | Release savepoint |

---

## Database Maintenance

Phase 3 Task 3.3 - Database maintenance operations.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_SWEEP` | `"SWEEP"` | Yes | Garbage collection sweep |
| `KW_DATABASE` | `"DATABASE"` | Yes | Database object |

---

## Tablespace Management

Phase 2 Task 2.1, 2.2 - Tablespace operations.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_TABLESPACE` | `"TABLESPACE"` | Yes | Tablespace object |
| `KW_LOCATION` | `"LOCATION"` | Yes | File system location |
| `KW_AUTOEXTEND` | `"AUTOEXTEND"` | Yes | Auto-extend file |
| `KW_AUTOEXTEND_SIZE` | `"AUTOEXTEND_SIZE"` | Yes | Auto-extend increment |
| `KW_MAXSIZE` | `"MAXSIZE"` | Yes | Maximum size |
| `KW_UNLIMITED` | `"UNLIMITED"` | Yes | Unlimited size |
| `KW_PREALLOC` | `"PREALLOC"` | Yes | Preallocate space |
| `KW_FORCE` | `"FORCE"` | Yes | Force operation |
| `KW_ONLINE` | `"ONLINE"` | Yes | Online operation (Phase 4 Task 4.1.1) |
| `KW_ATTACH` | `"ATTACH"` | Yes | Attach tablespace (Phase 6 Task 6.1) |
| `KW_DETACH` | `"DETACH"` | Yes | Detach tablespace (Phase 6 Task 6.2) |

---

## DDL Keywords

Data definition language - table and schema modifications.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_TABLE` | `"TABLE"` | Yes | Table object |
| `KW_INDEX` | `"INDEX"` | Yes | Index object (Phase 2 Task 2.3) |
| `KW_UNIQUE` | `"UNIQUE"` | Yes | Unique constraint (Phase 2 Task 2.3) |
| `KW_COLUMN` | `"COLUMN"` | Yes | Column object |
| `KW_RENAME` | `"RENAME"` | Yes | Rename object (Phase 2 Task 2.2) |
| `KW_TO` | `"TO"` | Yes | Rename target |
| `KW_ADD` | `"ADD"` | Yes | Add column/constraint (ALPHA Phase 1) |
| `KW_TYPE` | `"TYPE"` | Yes | Alter column type (ALPHA Phase 1) |
| `KW_SEQUENCE` | `"SEQUENCE"` | Yes | Sequence object (ALPHA Phase 1) |
| `KW_INCREMENT` | `"INCREMENT"` | Yes | Sequence increment (ALPHA Phase 1) |
| `KW_MINVALUE` | `"MINVALUE"` | Yes | Sequence minimum (ALPHA Phase 1) |
| `KW_MAXVALUE` | `"MAXVALUE"` | Yes | Sequence maximum (ALPHA Phase 1) |
| `KW_NO` | `"NO"` | Yes | NO MINVALUE/MAXVALUE/CYCLE (ALPHA Phase 1) |
| `KW_CACHE` | `"CACHE"` | Yes | Sequence cache (ALPHA Phase 1) |
| `KW_CYCLE` | `"CYCLE"` | Yes | Sequence cycle (ALPHA Phase 1) |
| `KW_RESTART` | `"RESTART"` | Yes | Restart sequence (ALPHA Phase 1) |
| `KW_NEXTVAL` | `"NEXTVAL"` | No | Sequence next value function (ALPHA Phase 1) |
| `KW_CURRVAL` | `"CURRVAL"` | No | Sequence current value function (ALPHA Phase 1) |
| `KW_SETVAL` | `"SETVAL"` | No | Set sequence value function (ALPHA Phase 1) |
| `KW_VIEW` | `"VIEW"` | Yes | View object (ALPHA Phase 1) |
| `KW_REPLACE` | `"REPLACE"` | Yes | Create or replace (ALPHA Phase 1) |
| `KW_MATERIALIZED` | `"MATERIALIZED"` | Yes | Materialized view (ALPHA Phase 1) |
| `KW_REFRESH` | `"REFRESH"` | Yes | Refresh materialized view (ALPHA Phase 1) |
| `KW_CONCURRENTLY` | `"CONCURRENTLY"` | Yes | Non-blocking operation (ALPHA Phase 1) |
| `KW_CHECK` | `"CHECK"` | Yes | Check constraint/option (ALPHA Phase 1) |
| `KW_OPTION` | `"OPTION"` | Yes | WITH CHECK OPTION (ALPHA Phase 1) |
| `KW_ASYNC` | `"ASYNC"` | Yes | Asynchronous TRUNCATE (ALPHA Phase 1) |
| `KW_SYNC` | `"SYNC"` | Yes | Synchronous TRUNCATE (ALPHA Phase 1) |

---

## Constraint Keywords

Table and column constraints (ALPHA Phase C).

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_CONSTRAINT` | `"CONSTRAINT"` | Yes | Named constraint |
| `KW_CONSTRAINTS` | `"CONSTRAINTS"` | Yes | SET CONSTRAINTS (P2-7) |
| `KW_IDENTITY` | `"IDENTITY"` | Yes | Identity column (auto-increment) |
| `KW_GENERATED` | `"GENERATED"` | Yes | Generated column |
| `KW_ALWAYS` | `"ALWAYS"` | Yes | GENERATED ALWAYS |
| `KW_STORED` | `"STORED"` | Yes | Generated ... STORED |
| `KW_VIRTUAL` | `"VIRTUAL"` | Yes | Generated ... VIRTUAL |
| `KW_DEFERRABLE` | `"DEFERRABLE"` | Yes | Deferrable constraint |
| `KW_INITIALLY` | `"INITIALLY"` | Yes | INITIALLY DEFERRED/IMMEDIATE |
| `KW_DEFERRED` | `"DEFERRED"` | Yes | Deferred constraint checking |
| `KW_IMMEDIATE` | `"IMMEDIATE"` | Yes | Immediate constraint checking |
| `KW_FOREIGN` | `"FOREIGN"` | Yes | Foreign key |
| `KW_KEY` | `"KEY"` | Yes | Primary/foreign key |
| `KW_PRIMARY` | `"PRIMARY"` | Yes | Primary key |
| `KW_REFERENCES` | `"REFERENCES"` | Yes | Foreign key reference |
| `KW_CASCADE` | `"CASCADE"` | Yes | Cascade delete/update (ALPHA Phase 1) |
| `KW_RESTRICT` | `"RESTRICT"` | Yes | Restrict delete/update (ALPHA Phase 1) |

---

## Subquery Keywords

Phase 2 Wave 2 - Agent B - Subquery operations.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_IN` | `"IN"` | Yes | In list/subquery |
| `KW_EXISTS` | `"EXISTS"` | Yes | Exists subquery |
| `KW_UNION` | `"UNION"` | Yes | Union set operation |
| `KW_INTERSECT` | `"INTERSECT"` | Yes | Intersect set operation |

**Note:** `KW_EXCEPT` is defined in token.h but missing from lexer keyword table.

---

## Trigger Keywords

Phase 2 Wave 2 - Agent C - Trigger definitions.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_TRIGGER` | `"TRIGGER"` | Yes | Trigger object |
| `KW_BEFORE` | `"BEFORE"` | Yes | Before trigger |
| `KW_AFTER` | `"AFTER"` | Yes | After trigger |
| `KW_EXECUTE` | `"EXECUTE"` | Yes | Execute procedure |
| `KW_PROCEDURE` | `"PROCEDURE"` | Yes | Stored procedure |
| `KW_OLD` | `"OLD"` | Yes | OLD row reference |
| `KW_NEW` | `"NEW"` | Yes | NEW row reference |
| `KW_DISCONNECT` | `"DISCONNECT"` | Yes | Database trigger: ON DISCONNECT |
| `KW_ACTIVE` | `"ACTIVE"` | Yes | Active trigger state |
| `KW_INACTIVE` | `"INACTIVE"` | Yes | Inactive trigger state |

---

## Stored Procedure Keywords

Phase 2 Task 10.2 - Procedural SQL (PSQL).

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_FUNCTION` | `"FUNCTION"` | Yes | Function definition |
| `KW_RETURNS` | `"RETURNS"` | Yes | Return type |
| `KW_LANGUAGE` | `"LANGUAGE"` | Yes | Language specification |
| `KW_BEGIN` | `"BEGIN"` | Yes | Begin block |
| `KW_DECLARE` | `"DECLARE"` | Yes | Declare variable |
| `KW_RETURN` | `"RETURN"` | Yes | Return statement |
| `KW_IF` | `"IF"` | Yes | If statement |
| `KW_ELSIF` | `"ELSIF"` | Yes | Else if statement |
| `KW_ENDIF` | `"ENDIF"` | Yes | End if |
| `KW_LOOP` | `"LOOP"` | Yes | Loop statement |
| `KW_WHILE` | `"WHILE"` | Yes | While loop |
| `KW_ENDLOOP` | `"ENDLOOP"` | Yes | End loop |
| `KW_EXIT` | `"EXIT"` | Yes | Exit loop |
| `KW_RAISE` | `"RAISE"` | Yes | Raise exception |
| `KW_EXCEPTION` | `"EXCEPTION"` | Yes | Exception handler |
| `KW_TRY` | `"TRY"` | Yes | Try block |
| `KW_EXCEPT` | `"EXCEPT"` | Yes | Exception handler |
| `KW_CALL` | `"CALL"` | Yes | Call procedure |

---

## Security Keywords

ALPHA Phase 1 - Security System Phase 2, Phase 3.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_USER` | `"USER"` | Yes | User object |
| `KW_ROLE` | `"ROLE"` | Yes | Role object |
| `KW_GRANT` | `"GRANT"` | Yes | Grant privileges/role |
| `KW_REVOKE` | `"REVOKE"` | Yes | Revoke privileges/role |
| `KW_PRIVILEGES` | `"PRIVILEGES"` | Yes | Privileges keyword |
| `KW_PASSWORD` | `"PASSWORD"` | Yes | User password |
| `KW_SUPERUSER` | `"SUPERUSER"` | Yes | Superuser privilege |
| `KW_NOSUPERUSER` | `"NOSUPERUSER"` | Yes | No superuser privilege |
| `KW_SESSION` | `"SESSION"` | Yes | Session context |
| `KW_AUTHORIZATION` | `"AUTHORIZATION"` | Yes | Authorization context |
| `KW_RESET` | `"RESET"` | Yes | Reset role/session |
| `KW_PUBLIC` | `"PUBLIC"` | Yes | Public role |
| `KW_USAGE` | `"USAGE"` | Yes | Usage privilege |
| `KW_CONNECT` | `"CONNECT"` | Yes | Connect privilege |
| `KW_POLICY` | `"POLICY"` | Yes | Row-level security policy (Phase 3.4) |
| `KW_ENABLE` | `"ENABLE"` | Yes | Enable RLS (Phase 3.4) |
| `KW_DISABLE` | `"DISABLE"` | Yes | Disable RLS (Phase 3.4) |
| `KW_SQL` | `"SQL"` | Yes | SQL SECURITY (Phase 3.1) |
| `KW_SECURITY` | `"SECURITY"` | Yes | Security context (Phase 3.4) |
| `KW_DEFINER` | `"DEFINER"` | Yes | SQL SECURITY DEFINER (Phase 3.1) |
| `KW_INVOKER` | `"INVOKER"` | Yes | SQL SECURITY INVOKER (Phase 3.1) |
| `KW_ADMIN` | `"ADMIN"` | Yes | WITH ADMIN OPTION (WP-6 PARSE-L1) |

---

## SQL Engine Commands

ALPHA Phase 1 - Developer Experience - SHOW/DESCRIBE commands.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_TABLES` | `"TABLES"` | Yes | SHOW TABLES |
| `KW_DATABASES` | `"DATABASES"` | Yes | SHOW DATABASES |
| `KW_SCHEMAS` | `"SCHEMAS"` | Yes | SHOW SCHEMAS |
| `KW_COLUMNS` | `"COLUMNS"` | Yes | SHOW COLUMNS |
| `KW_INDEXES` | `"INDEXES"` | Yes | SHOW INDEXES |

---

## UPSERT Keywords

INSERT ... ON CONFLICT (upsert) operations.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_CONFLICT` | `"CONFLICT"` | Yes | ON CONFLICT clause |
| `KW_DO` | `"DO"` | Yes | DO UPDATE / DO NOTHING |
| `KW_NOTHING` | `"NOTHING"` | Yes | DO NOTHING action |
| `KW_EXCLUDED` | `"EXCLUDED"` | Yes | EXCLUDED pseudo-table |

---

## Advanced SQL Keywords

Advanced SQL features.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_MATCHED` | `"MATCHED"` | Yes | MERGE WHEN MATCHED (ALPHA) |
| `KW_SOURCE` | `"SOURCE"` | Yes | MERGE BY SOURCE (ALPHA) |
| `KW_TARGET` | `"TARGET"` | Yes | MERGE BY TARGET (ALPHA) |

---

## User Defined Types

User-defined type system.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_COMPOSITE` | `"COMPOSITE"` | Yes | Composite type |
| `KW_ENUM` | `"ENUM"` | Yes | Enumeration type |
| `KW_DOMAIN` | `"DOMAIN"` | Yes | Domain type |
| `KW_SUBTYPE` | `"SUBTYPE"` | Yes | Range subtype |

---

## Extended SHOW/SET Keywords

Firebird ISQL compatibility - extended SHOW and SET commands.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_DIALECT` | `"DIALECT"` | Yes | SQL dialect |
| `KW_GENERATOR` | `"GENERATOR"` | Yes | Generator (sequence alias) |
| `KW_DEPENDENCIES` | `"DEPENDENCIES"` | Yes | Object dependencies |
| `KW_COLLATIONS` | `"COLLATIONS"` | Yes | Show collations |
| `KW_COMMENTS` | `"COMMENTS"` | Yes | Show comments |
| `KW_CHECKS` | `"CHECKS"` | Yes | Show check constraints |
| `KW_GRANTS` | `"GRANTS"` | Yes | Show grants |
| `KW_SYSTEM` | `"SYSTEM"` | Yes | System objects |
| `KW_PACKAGE` | `"PACKAGE"` | Yes | Package object |
| `KW_NAMES` | `"NAMES"` | Yes | SET NAMES charset |
| `KW_LOCAL_TIMEOUT` | `"LOCAL_TIMEOUT"` | Yes | SET LOCAL_TIMEOUT |
| `KW_SCHEMA` | `"SCHEMA"` | Yes | Schema object |
| `KW_VERSION` | `"VERSION"` | Yes | SHOW VERSION |

---

## Schema Navigation Keywords

Multi-schema navigation and path resolution.

| Token Type | String Representation | Reserved | Description |
|------------|----------------------|----------|-------------|
| `KW_PATH` | `"PATH"` | No | SHOW SCHEMA PATH, SET SEARCH PATH |
| `KW_TREE` | `"TREE"` | No | SHOW SCHEMA TREE |
| `KW_DEPTH` | `"DEPTH"` | No | SHOW SCHEMA TREE DEPTH n |
| `KW_SEARCH` | `"SEARCH"` | No | SET SEARCH PATH |
| `KW_OF` | `"OF"` | Yes | SHOW LOCATION OF |
| `KW_RESOLVED` | `"RESOLVED"` | No | SHOW RESOLVED name |
| `KW_OBJECTS` | `"OBJECTS"` | No | SHOW OBJECTS |
| `KW_DETAIL` | `"DETAIL"` | No | IN DETAIL |
| `KW_HOME` | `"HOME"` | No | SET SCHEMA HOME |
| `KW_ROOT` | `"ROOT"` | No | SET SCHEMA ROOT |
| `KW_UP` | `"UP"` | No | SET SCHEMA UP |

**Note:** Schema navigation keywords (PATH, TREE, DEPTH, SEARCH, RESOLVED, OBJECTS, DETAIL, HOME, ROOT, UP) are NOT reserved keywords. They are handled contextually in the parser to avoid conflicts with common column names.

---

## Operator Precedence

Expression parsing precedence (highest to lowest):

| Level | Operators | Associativity | Description |
|-------|-----------|---------------|-------------|
| 6 | `->`, `->>`, `#>`, `#>>` | Left | JSON operators (postfix) |
| 5 | `*`, `/`, `%` | Left | Multiplicative |
| 4 | `+`, `-` | Left | Additive |
| 3 | `=`, `<>`, `<`, `>`, `<=`, `>=` | Left | Comparison |
| 3 | `LIKE`, `ILIKE` | Left | Pattern matching |
| 3 | `&&`, `@>`, `<@` | Left | Array/range operators |
| 3 | `<<`, `>>`, `-\|-` | Left | Range operators |
| 3 | `~`, `~*`, `!~`, `!~*` | Left | Regex operators |
| 3 | `IN`, `NOT IN` | Left | Set membership |
| 2 | `AND` | Left | Logical AND |
| 1 | `OR` | Left | Logical OR |

**Expression Parsing Functions:**

```
parseExpression()      → parseOr()
parseOr()              → parseAnd()
parseAnd()             → parseComparison()
parseComparison()      → parseTerm()
parseTerm()            → parseFactor()
parseFactor()          → parsePrimary() + JSON/multiplicative operators
parsePrimary()         → Literals, identifiers, function calls, etc.
```

**Notes:**
- Unary operators (`NOT`, `-`) are handled at the primary level
- JSON operators are postfix operators handled in `parseFactor()`
- All binary operators are left-associative
- Parentheses can override precedence

---

## Reserved vs Contextual Keywords

### Fully Reserved Keywords

These keywords **cannot** be used as unquoted identifiers:

- **SQL Commands:** `CREATE`, `ALTER`, `DROP`, `TRUNCATE`, `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `MERGE`
- **Clauses:** `FROM`, `WHERE`, `JOIN`, `ON`, `USING`, `GROUP`, `HAVING`, `ORDER`, `LIMIT`, `OFFSET`
- **Data Types:** All type keywords are contextually reserved in type position
- **Operators:** `AND`, `OR`, `NOT`, `IN`, `EXISTS`, `LIKE`, `ILIKE`, `BETWEEN`, `IS`
- **Conditional:** `CASE`, `WHEN`, `THEN`, `ELSE`, `END`
- **Transaction:** `START`, `COMMIT`, `ROLLBACK`, `TRANSACTION`
- **DDL:** `TABLE`, `INDEX`, `VIEW`, `SEQUENCE`, `CONSTRAINT`, etc.

### Contextual Keywords (NOT Reserved)

These keywords are handled contextually and **can** be used as identifiers:

- **Schema Navigation:** `PATH`, `TREE`, `DEPTH`, `SEARCH`, `RESOLVED`, `OBJECTS`, `DETAIL`, `HOME`, `ROOT`, `UP`
- **Functions:** `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`, `COALESCE`, `NULLIF`, etc.
- **Window Functions:** `ROW_NUMBER`, `RANK`, `DENSE_RANK`, `LAG`, `LEAD`, etc.
- **Sequence Functions:** `NEXTVAL`, `CURRVAL`, `SETVAL`

**Recommendation:** Avoid using any keyword as an identifier. If necessary, use double-quoted delimited identifiers: `"count"`, `"path"`, etc.

---

## Missing Token Mappings

The following token types are defined in `token.h` but **missing** from `token.cpp::tokenTypeToString()`:

**General Keywords:**
- `KW_NULL` (maps to "NULL" in lexer)
- `KW_NOT` (maps to "NOT" in lexer)
- `KW_ANALYZE` (maps to "ANALYZE" in lexer)
- `KW_EXPLAIN` (maps to "EXPLAIN" in lexer)
- `KW_COLUMN` (maps to "COLUMN" in lexer)
- `KW_SAMPLE` (maps to "SAMPLE" in lexer)

**JOIN Keywords:**
- All JOIN-related keywords except their string representations exist in lexer

**Aggregation:**
- Most aggregation keywords are in lexer but not in token.cpp string mappings

**Window Functions:**
- Most window function keywords exist in lexer but not in token.cpp

**Data Types:**
- Most type keywords exist in lexer but only a few are mapped in token.cpp
- Only `KW_INTEGER`, `KW_BIGINT`, `KW_DOUBLE`, `KW_VARCHAR` have string mappings

**Other Missing:**
- `KW_ADD`, `KW_TYPE`, `KW_CONSTRAINT`, and many others

This is likely intentional - `tokenTypeToString()` only maps frequently debugged tokens, not all keywords.

---

## Lexer Notes

### Comments

- **Line comments:** `-- comment`
- **Block comments:** `/* comment */`

### String Literals

- Single-quoted: `'string'`
- Escape sequences: `\'`, `\"`, `\n`, `\t`, `\r`, `\\`
- SQL-style escaped quote: `''` (two single quotes)

### Quoted Identifiers

- Double-quoted: `"Identifier"`
- Case-sensitive
- Escaped double-quote: `""` (two double quotes)

### Numeric Literals

- Integers: `123`, `-456`
- Floats: `123.45`, `1.23e10`, `1.23E-5`
- Trailing decimal allowed: `123.`

### UTF-8 Support

- All identifiers validated for UTF-8 encoding
- Maximum identifier length: 128 characters (not bytes)

---

## Token Enum Summary

**Total Token Types Defined:** 529 enum values (0-528)

**Breakdown by Category:**
- Special: 3
- Literals: 3
- Identifiers: 1
- Operators/Punctuation: 30
- Keywords: 492

**Keyword Categories:**
- SQL Commands: 8
- DML: 11
- JOIN: 11
- Aggregation: 14
- Window: 15
- Aggregate Functions: 8
- Window Functions: 11
- Data Types (Numeric): 16
- Data Types (String): 4
- Data Types (Binary): 4
- Data Types (Date/Time): 4
- Data Types (Boolean): 2
- Data Types (Special): 6
- Data Types (Spatial): 7
- Data Types (Range): 6
- JSON Functions: 10
- Conditional: 7
- Array Functions: 12
- Type Conversion: 3
- Text Functions: 3
- Pattern Matching: 2
- Character Set: 3
- Timezone: 5
- Transaction Control: 21
- Database Maintenance: 2
- Tablespace: 11
- DDL: 25
- Constraints: 17
- Subquery: 3
- Triggers: 10
- Stored Procedures: 18
- Security: 21
- SQL Engine Commands: 5
- UPSERT: 4
- Advanced SQL: 3
- User Defined Types: 4
- Extended SHOW/SET: 13
- Schema Navigation: 11

---

## End of Token Reference

This document provides a complete audit of all token types, operators, keywords, and data types in the ScratchBird parser as of 2025-12-06.
