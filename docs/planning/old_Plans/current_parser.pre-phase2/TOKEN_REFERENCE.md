# ScratchBird Token Type Reference

**Last Updated:** December 4, 2025

This document defines all token types used by the ScratchBird SQL lexer.

---

## Token Categories Overview

| Category | Count | Description |
|----------|-------|-------------|
| Special | 2 | EOF, Error |
| Literals | 3 | Integer, Float, String |
| Identifiers | 2 | Identifier, Keyword (generic) |
| Operators | 25+ | Arithmetic, comparison, logical, special |
| Keywords | 160+ | SQL reserved words |

---

## Special Tokens

| Token | Value | Description |
|-------|-------|-------------|
| `END_OF_FILE` | 0 | End of input stream |
| `ERROR` | 1 | Lexical error |

---

## Literal Tokens

| Token | Description | Example |
|-------|-------------|---------|
| `INTEGER_LITERAL` | Integer number | `42`, `-1`, `1000` |
| `FLOAT_LITERAL` | Floating-point number | `3.14`, `-0.5`, `1.0e10` |
| `STRING_LITERAL` | Quoted string | `'hello'`, `'it''s'` |

---

## Identifier Token

| Token | Description |
|-------|-------------|
| `IDENTIFIER` | Unquoted or quoted identifier (table/column names) |
| `KEYWORD` | Generic keyword (detected by content) |

---

## Operator Tokens

### Arithmetic Operators

| Token | Symbol | Description |
|-------|--------|-------------|
| `PLUS` | `+` | Addition |
| `MINUS` | `-` | Subtraction |
| `STAR` | `*` | Multiplication |
| `SLASH` | `/` | Division |
| `PERCENT` | `%` | Modulo |

### Comparison Operators

| Token | Symbol | Description |
|-------|--------|-------------|
| `EQUAL` | `=` | Equals |
| `NOT_EQUAL` | `<>` | Not equals |
| `LESS_THAN` | `<` | Less than |
| `GREATER_THAN` | `>` | Greater than |
| `LESS_EQUAL` | `<=` | Less than or equal |
| `GREATER_EQUAL` | `>=` | Greater than or equal |

### Punctuation

| Token | Symbol | Description |
|-------|--------|-------------|
| `LEFT_PAREN` | `(` | Left parenthesis |
| `RIGHT_PAREN` | `)` | Right parenthesis |
| `LEFT_BRACKET` | `[` | Left bracket (arrays, ranges) |
| `RIGHT_BRACKET` | `]` | Right bracket (arrays, ranges) |
| `COMMA` | `,` | Comma separator |
| `SEMICOLON` | `;` | Statement terminator |
| `DOT` | `.` | Member access |
| `COLON` | `:` | Type cast prefix |
| `COLON_EQUALS` | `:=` | PL/SQL assignment |

### JSON Operators

| Token | Symbol | Description |
|-------|--------|-------------|
| `ARROW` | `->` | JSON field (returns JSON) |
| `DOUBLE_ARROW` | `->>` | JSON field (returns text) |
| `HASH_ARROW` | `#>` | JSON path (returns JSON) |
| `HASH_DOUBLE_ARROW` | `#>>` | JSON path (returns text) |

### Array Operators

| Token | Symbol | Description |
|-------|--------|-------------|
| `AMPERSAND_AMPERSAND` | `&&` | Array/range overlap |
| `AT_GREATER` | `@>` | Array/range contains |
| `LESS_AT` | `<@` | Array/range contained by |

### Range Operators

| Token | Symbol | Description |
|-------|--------|-------------|
| `SHIFT_LEFT` | `<<` | Strictly left of |
| `SHIFT_RIGHT` | `>>` | Strictly right of |
| `MINUS_PIPE_MINUS` | `-|-` | Adjacent to |

### Regex Operators

| Token | Symbol | Description |
|-------|--------|-------------|
| `TILDE` | `~` | Regex match (case-sensitive) |
| `TILDE_STAR` | `~*` | Regex match (case-insensitive) |
| `EXCLAIM_TILDE` | `!~` | Regex not match (case-sensitive) |
| `EXCLAIM_TILDE_STAR` | `!~*` | Regex not match (case-insensitive) |

---

## SQL Keywords

### DDL Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_CREATE` | CREATE | Create object |
| `KW_ALTER` | ALTER | Modify object |
| `KW_DROP` | DROP | Delete object |
| `KW_TRUNCATE` | TRUNCATE | Truncate table |
| `KW_TABLE` | TABLE | Table object |
| `KW_INDEX` | INDEX | Index object |
| `KW_UNIQUE` | UNIQUE | Unique constraint/index |
| `KW_VIEW` | VIEW | View object |
| `KW_MATERIALIZED` | MATERIALIZED | Materialized view |
| `KW_REFRESH` | REFRESH | Refresh materialized view |
| `KW_CONCURRENTLY` | CONCURRENTLY | Concurrent refresh |
| `KW_SEQUENCE` | SEQUENCE | Sequence object |
| `KW_TRIGGER` | TRIGGER | Trigger object |
| `KW_TABLESPACE` | TABLESPACE | Tablespace object |
| `KW_CONSTRAINT` | CONSTRAINT | Constraint definition |
| `KW_CONSTRAINTS` | CONSTRAINTS | Multiple constraints |
| `KW_PRIMARY` | PRIMARY | Primary key |
| `KW_FOREIGN` | FOREIGN | Foreign key |
| `KW_KEY` | KEY | Key constraint |
| `KW_REFERENCES` | REFERENCES | Foreign key reference |
| `KW_CASCADE` | CASCADE | Cascade action |
| `KW_RESTRICT` | RESTRICT | Restrict action |
| `KW_ADD` | ADD | Add column/constraint |
| `KW_RENAME` | RENAME | Rename object |
| `KW_TYPE` | TYPE | Type specification |
| `KW_REPLACE` | REPLACE | CREATE OR REPLACE |

### DML Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_SELECT` | SELECT | Select statement |
| `KW_INSERT` | INSERT | Insert statement |
| `KW_UPDATE` | UPDATE | Update statement |
| `KW_DELETE` | DELETE | Delete statement |
| `KW_MERGE` | MERGE | Merge statement |
| `KW_INTO` | INTO | Insert/merge target |
| `KW_VALUES` | VALUES | Value list |
| `KW_FROM` | FROM | From clause |
| `KW_WHERE` | WHERE | Where clause |
| `KW_SET` | SET | Update assignments |
| `KW_RETURNING` | RETURNING | Return modified rows |
| `KW_MATCHED` | MATCHED | MERGE WHEN MATCHED |
| `KW_SOURCE` | SOURCE | MERGE source |
| `KW_TARGET` | TARGET | MERGE target |

### JOIN Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_JOIN` | JOIN | Join clause |
| `KW_INNER` | INNER | Inner join |
| `KW_LEFT` | LEFT | Left outer join |
| `KW_RIGHT` | RIGHT | Right outer join |
| `KW_FULL` | FULL | Full outer join |
| `KW_OUTER` | OUTER | Outer join modifier |
| `KW_CROSS` | CROSS | Cross join |
| `KW_NATURAL` | NATURAL | Natural join |
| `KW_USING` | USING | Join using columns |
| `KW_ON` | ON | Join condition |

### Grouping & Ordering Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_GROUP` | GROUP | Group by clause |
| `KW_BY` | BY | Group/order by |
| `KW_HAVING` | HAVING | Having clause |
| `KW_ORDER` | ORDER | Order by clause |
| `KW_ASC` | ASC | Ascending order |
| `KW_DESC` | DESC | Descending order |
| `KW_NULLS` | NULLS | Nulls ordering |
| `KW_FIRST` | FIRST | NULLS FIRST |
| `KW_LAST` | LAST | NULLS LAST |
| `KW_LIMIT` | LIMIT | Result limit |
| `KW_OFFSET` | OFFSET | Result offset |
| `KW_DISTINCT` | DISTINCT | Distinct values |
| `KW_ALL` | ALL | All values |

### Advanced Grouping Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_ROLLUP` | ROLLUP | Rollup grouping |
| `KW_CUBE` | CUBE | Cube grouping |
| `KW_GROUPING` | GROUPING | Grouping function |
| `KW_SETS` | SETS | Grouping sets |

### Set Operation Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_UNION` | UNION | Union set operation |
| `KW_INTERSECT` | INTERSECT | Intersect set operation |
| `KW_EXCEPT` | EXCEPT | Except set operation |

### Aggregate Function Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_COUNT` | COUNT | Count aggregate |
| `KW_SUM` | SUM | Sum aggregate |
| `KW_AVG` | AVG | Average aggregate |
| `KW_MIN` | MIN | Minimum aggregate |
| `KW_MAX` | MAX | Maximum aggregate |
| `KW_ARRAY_AGG` | ARRAY_AGG | Array aggregate |

### Window Function Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_OVER` | OVER | Window specification |
| `KW_PARTITION` | PARTITION | Partition by |
| `KW_ROW_NUMBER` | ROW_NUMBER | Row number function |
| `KW_RANK` | RANK | Rank function |
| `KW_DENSE_RANK` | DENSE_RANK | Dense rank function |
| `KW_LAG` | LAG | Lag function |
| `KW_LEAD` | LEAD | Lead function |
| `KW_FIRST_VALUE` | FIRST_VALUE | First value function |
| `KW_LAST_VALUE` | LAST_VALUE | Last value function |
| `KW_NTH_VALUE` | NTH_VALUE | Nth value function |
| `KW_CUME_DIST` | CUME_DIST | Cumulative distribution |
| `KW_PERCENT_RANK` | PERCENT_RANK | Percent rank |

### Window Frame Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_ROWS` | ROWS | Rows frame mode |
| `KW_RANGE` | RANGE | Range frame mode |
| `KW_GROUPS` | GROUPS | Groups frame mode |
| `KW_BETWEEN` | BETWEEN | Frame boundary |
| `KW_UNBOUNDED` | UNBOUNDED | Unbounded frame |
| `KW_PRECEDING` | PRECEDING | Preceding rows |
| `KW_FOLLOWING` | FOLLOWING | Following rows |
| `KW_CURRENT` | CURRENT | Current row |
| `KW_ROW` | ROW | Row specifier |

### Logical Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_AND` | AND | Logical AND |
| `KW_OR` | OR | Logical OR |
| `KW_NOT` | NOT | Logical NOT |
| `KW_IN` | IN | In list/subquery |
| `KW_EXISTS` | EXISTS | Exists subquery |
| `KW_NULL` | NULL | Null value |
| `KW_LIKE` | LIKE | Pattern match |
| `KW_ILIKE` | ILIKE | Case-insensitive pattern |

### Type Keywords - Numeric

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_INT` | INT | Integer |
| `KW_INTEGER` | INTEGER | Integer |
| `KW_SMALLINT` | SMALLINT | Small integer |
| `KW_BIGINT` | BIGINT | Big integer |
| `KW_TINYINT` | TINYINT | Tiny integer |
| `KW_INT128` | INT128 | 128-bit integer |
| `KW_UINT8` | UINT8 | Unsigned 8-bit |
| `KW_UINT16` | UINT16 | Unsigned 16-bit |
| `KW_UINT32` | UINT32 | Unsigned 32-bit |
| `KW_UINT64` | UINT64 | Unsigned 64-bit |
| `KW_REAL` | REAL | Single-precision float |
| `KW_FLOAT` | FLOAT | Floating point |
| `KW_DOUBLE` | DOUBLE | Double precision |
| `KW_DECIMAL` | DECIMAL | Exact decimal |
| `KW_NUMERIC` | NUMERIC | Numeric type |
| `KW_MONEY` | MONEY | Currency type |

### Type Keywords - String

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_CHAR` | CHAR | Fixed-length string |
| `KW_CHARACTER` | CHARACTER | Character type |
| `KW_VARCHAR` | VARCHAR | Variable-length string |
| `KW_TEXT` | TEXT | Unlimited text |

### Type Keywords - Binary

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_BINARY` | BINARY | Fixed-length binary |
| `KW_VARBINARY` | VARBINARY | Variable-length binary |
| `KW_BLOB` | BLOB | Binary large object |
| `KW_BYTEA` | BYTEA | Byte array |

### Type Keywords - Date/Time

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_DATE` | DATE | Date type |
| `KW_TIME` | TIME | Time type |
| `KW_TIMESTAMP` | TIMESTAMP | Timestamp type |
| `KW_INTERVAL` | INTERVAL | Interval type |
| `KW_ZONE` | ZONE | Time zone |

### Type Keywords - Special

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_BOOLEAN` | BOOLEAN | Boolean type |
| `KW_BOOL` | BOOL | Boolean alias |
| `KW_UUID` | UUID | UUID type |
| `KW_JSON` | JSON | JSON type |
| `KW_JSONB` | JSONB | Binary JSON type |
| `KW_XML` | XML | XML type |
| `KW_VECTOR` | VECTOR | Vector embeddings |
| `KW_ARRAY` | ARRAY | Array type |

### Type Keywords - Spatial

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_POINT` | POINT | 2D point |
| `KW_LINESTRING` | LINESTRING | Line |
| `KW_POLYGON` | POLYGON | Polygon |
| `KW_MULTIPOINT` | MULTIPOINT | Point collection |
| `KW_MULTILINESTRING` | MULTILINESTRING | Line collection |
| `KW_MULTIPOLYGON` | MULTIPOLYGON | Polygon collection |
| `KW_GEOMETRYCOLLECTION` | GEOMETRYCOLLECTION | Mixed geometry |

### Type Keywords - Range

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_INT4RANGE` | INT4RANGE | Integer range |
| `KW_INT8RANGE` | INT8RANGE | Bigint range |
| `KW_NUMRANGE` | NUMRANGE | Numeric range |
| `KW_DATERANGE` | DATERANGE | Date range |
| `KW_TSRANGE` | TSRANGE | Timestamp range |
| `KW_TSTZRANGE` | TSTZRANGE | Timestamptz range |

### JSON Function Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_JSON_EXTRACT` | JSON_EXTRACT | Extract JSON value |
| `KW_JSON_OBJECT` | JSON_OBJECT | Create JSON object |
| `KW_JSON_ARRAY` | JSON_ARRAY | Create JSON array |
| `KW_JSON_SET` | JSON_SET | Set JSON value |
| `KW_JSON_INSERT` | JSON_INSERT | Insert JSON value |
| `KW_JSON_REMOVE` | JSON_REMOVE | Remove JSON value |
| `KW_JSONB_EXTRACT_PATH` | JSONB_EXTRACT_PATH | Extract JSONB path |
| `KW_JSONB_BUILD_OBJECT` | JSONB_BUILD_OBJECT | Build JSONB object |
| `KW_JSONB_BUILD_ARRAY` | JSONB_BUILD_ARRAY | Build JSONB array |
| `KW_JSONB_SET` | JSONB_SET | Set JSONB value |

### Conditional Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_COALESCE` | COALESCE | First non-null |
| `KW_NULLIF` | NULLIF | Return null if equal |
| `KW_CASE` | CASE | Case expression |
| `KW_WHEN` | WHEN | Case when clause |
| `KW_THEN` | THEN | Case then clause |
| `KW_ELSE` | ELSE | Case else clause |
| `KW_END` | END | Case/block end |

### Array Function Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_ARRAY_TO_STRING` | ARRAY_TO_STRING | Array to string |
| `KW_STRING_TO_ARRAY` | STRING_TO_ARRAY | String to array |
| `KW_ARRAY_APPEND` | ARRAY_APPEND | Append element |
| `KW_ARRAY_PREPEND` | ARRAY_PREPEND | Prepend element |
| `KW_ARRAY_CAT` | ARRAY_CAT | Concatenate arrays |
| `KW_ARRAY_REMOVE` | ARRAY_REMOVE | Remove element |
| `KW_ARRAY_REPLACE` | ARRAY_REPLACE | Replace element |
| `KW_ARRAY_LENGTH` | ARRAY_LENGTH | Array length |
| `KW_ARRAY_DIMS` | ARRAY_DIMS | Array dimensions |
| `KW_ARRAY_UPPER` | ARRAY_UPPER | Upper bound |
| `KW_ARRAY_LOWER` | ARRAY_LOWER | Lower bound |
| `KW_UNNEST` | UNNEST | Expand array to rows |

### Type Conversion Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_CAST` | CAST | Type cast |
| `KW_TRY_CAST` | TRY_CAST | Safe type cast |
| `KW_AS` | AS | Alias/cast target |

### Expression Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_EXTRACT` | EXTRACT | Extract field |
| `KW_POSITION` | POSITION | Find substring |
| `KW_OVERLAY` | OVERLAY | Replace substring |
| `KW_PLACING` | PLACING | Overlay placement |

### Transaction Control Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_START` | START | Start transaction |
| `KW_TRANSACTION` | TRANSACTION | Transaction |
| `KW_COMMIT` | COMMIT | Commit transaction |
| `KW_ROLLBACK` | ROLLBACK | Rollback transaction |
| `KW_READ` | READ | Read mode |
| `KW_WRITE` | WRITE | Write mode |
| `KW_ONLY` | ONLY | Read only modifier |
| `KW_WAIT` | WAIT | Wait for lock |
| `KW_ISOLATION` | ISOLATION | Isolation level |
| `KW_LEVEL` | LEVEL | Level specifier |
| `KW_COMMITTED` | COMMITTED | Read committed |
| `KW_SNAPSHOT` | SNAPSHOT | Snapshot isolation |
| `KW_STABILITY` | STABILITY | Table stability |
| `KW_RESERVING` | RESERVING | Table reservation |
| `KW_SHARED` | SHARED | Shared lock |
| `KW_PROTECTED` | PROTECTED | Protected lock |
| `KW_FOR` | FOR | For clause |
| `KW_LOCK` | LOCK | Lock clause |
| `KW_TIMEOUT` | TIMEOUT | Lock timeout |

### Database Maintenance Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_SWEEP` | SWEEP | Garbage collection |
| `KW_DATABASE` | DATABASE | Database object |
| `KW_ANALYZE` | ANALYZE | Collect statistics |
| `KW_EXPLAIN` | EXPLAIN | Query plan |
| `KW_COLUMN` | COLUMN | Column specifier |
| `KW_SAMPLE` | SAMPLE | Statistics sample |

### Tablespace Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_LOCATION` | LOCATION | Tablespace location |
| `KW_AUTOEXTEND` | AUTOEXTEND | Auto extend |
| `KW_AUTOEXTEND_SIZE` | AUTOEXTEND_SIZE | Auto extend size |
| `KW_MAXSIZE` | MAXSIZE | Maximum size |
| `KW_UNLIMITED` | UNLIMITED | Unlimited size |
| `KW_PREALLOC` | PREALLOC | Pre-allocate |
| `KW_ONLINE` | ONLINE | Online operation |
| `KW_ATTACH` | ATTACH | Attach tablespace |
| `KW_DETACH` | DETACH | Detach tablespace |

### Sequence Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_INCREMENT` | INCREMENT | Sequence increment |
| `KW_MINVALUE` | MINVALUE | Minimum value |
| `KW_MAXVALUE` | MAXVALUE | Maximum value |
| `KW_NO` | NO | No modifier |
| `KW_CACHE` | CACHE | Cache size |
| `KW_CYCLE` | CYCLE | Cycle sequence |
| `KW_RESTART` | RESTART | Restart sequence |
| `KW_NEXTVAL` | NEXTVAL | Next value |
| `KW_CURRVAL` | CURRVAL | Current value |
| `KW_SETVAL` | SETVAL | Set value |

### Generated Column Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_IDENTITY` | IDENTITY | Identity column |
| `KW_GENERATED` | GENERATED | Generated column |
| `KW_ALWAYS` | ALWAYS | Always generated |
| `KW_STORED` | STORED | Stored computed |
| `KW_VIRTUAL` | VIRTUAL | Virtual computed |

### Constraint Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_DEFAULT` | DEFAULT | Default value |
| `KW_CHECK` | CHECK | Check constraint |
| `KW_OPTION` | OPTION | View option |
| `KW_DEFERRABLE` | DEFERRABLE | Deferrable constraint |
| `KW_INITIALLY` | INITIALLY | Initial state |
| `KW_DEFERRED` | DEFERRED | Initially deferred |
| `KW_IMMEDIATE` | IMMEDIATE | Initially immediate |
| `KW_ENABLE` | ENABLE | Enable feature |
| `KW_DISABLE` | DISABLE | Disable feature |
| `KW_FORCE` | FORCE | Force operation |

### Trigger Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_BEFORE` | BEFORE | Before trigger |
| `KW_AFTER` | AFTER | After trigger |
| `KW_EXECUTE` | EXECUTE | Execute procedure |
| `KW_PROCEDURE` | PROCEDURE | Procedure |
| `KW_OLD` | OLD | Old row reference |
| `KW_NEW` | NEW | New row reference |

### Stored Procedure Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_FUNCTION` | FUNCTION | Function definition |
| `KW_RETURNS` | RETURNS | Return type |
| `KW_LANGUAGE` | LANGUAGE | Language |
| `KW_BEGIN` | BEGIN | Block begin |
| `KW_DECLARE` | DECLARE | Variable declaration |
| `KW_RETURN` | RETURN | Return statement |
| `KW_IF` | IF | If statement |
| `KW_ELSIF` | ELSIF | Else if |
| `KW_ENDIF` | ENDIF | End if |
| `KW_LOOP` | LOOP | Loop statement |
| `KW_WHILE` | WHILE | While loop |
| `KW_ENDLOOP` | ENDLOOP | End loop |
| `KW_EXIT` | EXIT | Exit loop |
| `KW_RAISE` | RAISE | Raise error |
| `KW_EXCEPTION` | EXCEPTION | Exception handler |
| `KW_TRY` | TRY | Try block |

### Security Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_USER` | USER | User object |
| `KW_ROLE` | ROLE | Role object |
| `KW_GRANT` | GRANT | Grant privilege |
| `KW_REVOKE` | REVOKE | Revoke privilege |
| `KW_PRIVILEGES` | PRIVILEGES | Privilege list |
| `KW_PASSWORD` | PASSWORD | User password |
| `KW_SUPERUSER` | SUPERUSER | Superuser flag |
| `KW_NOSUPERUSER` | NOSUPERUSER | Not superuser |
| `KW_SESSION` | SESSION | Session context |
| `KW_AUTHORIZATION` | AUTHORIZATION | Authorization |
| `KW_RESET` | RESET | Reset state |
| `KW_PUBLIC` | PUBLIC | Public role |
| `KW_USAGE` | USAGE | Usage privilege |
| `KW_CONNECT` | CONNECT | Connect privilege |
| `KW_ADMIN` | ADMIN | Admin option |

### Row Level Security Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_POLICY` | POLICY | RLS policy |
| `KW_SQL` | SQL | SQL security |
| `KW_SECURITY` | SECURITY | Security mode |
| `KW_DEFINER` | DEFINER | Definer rights |
| `KW_INVOKER` | INVOKER | Invoker rights |

### Utility Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_SHOW` | SHOW | Show information |
| `KW_DESCRIBE` | DESCRIBE | Describe object |
| `KW_TABLES` | TABLES | Show tables |
| `KW_DATABASES` | DATABASES | Show databases |
| `KW_SCHEMAS` | SCHEMAS | Show schemas |
| `KW_COLUMNS` | COLUMNS | Show columns |
| `KW_INDEXES` | INDEXES | Show indexes |

### Miscellaneous Keywords

| Token | SQL | Description |
|-------|-----|-------------|
| `KW_TO` | TO | Target specifier |
| `KW_WITH` | WITH | CTE/options |
| `KW_RECURSIVE` | RECURSIVE | Recursive CTE |
| `KW_WITHOUT` | WITHOUT | Without modifier |
| `KW_AT` | AT | At timezone |
| `KW_COLLATE` | COLLATE | Collation |
| `KW_COLLATION` | COLLATION | Collation name |
| `KW_OFF` | OFF | Off state |
| `KW_ASYNC` | ASYNC | Asynchronous |
| `KW_SYNC` | SYNC | Synchronous |
| `KW_OUTSTANDING` | OUTSTANDING | Outstanding locks |

---

## Token Structure

```cpp
struct Token {
    TokenType type;        // Token category
    SourceLocation location;  // Line, column, offset
    uint32_t length;       // Token text length

    union {
        int64_t int_value;              // INTEGER_LITERAL
        double float_value;             // FLOAT_LITERAL
        StringPool::StringId string_id; // IDENTIFIER, STRING_LITERAL
        uint8_t keyword_code;           // Specific keywords
    } value;
};
```

---

## SourceLocation Structure

```cpp
struct SourceLocation {
    uint32_t line;     // 1-indexed line number
    uint32_t column;   // 1-indexed column number
    uint32_t offset;   // Byte offset in source
};
```

---

## StringPool

All identifiers and string literals are interned in a `StringPool` for deduplication:

```cpp
class StringPool {
    StringId intern(std::string_view str);  // Get/create ID
    std::string_view get(StringId id) const; // Retrieve string
    void clear();                            // Reset pool
};
```

---

## Related Documentation

- [PARSER_OVERVIEW.md](PARSER_OVERVIEW.md) - Parser architecture
- [SQL_COMMANDS.md](SQL_COMMANDS.md) - SQL command reference
- [AST_NODES.md](AST_NODES.md) - AST node classes
