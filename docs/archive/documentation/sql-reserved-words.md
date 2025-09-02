### Reserved Words and Keywords

**What it is**

Keywords are special words recognized by the SQL lexer that have predefined meanings in the language. ScratchBird maintains both a generated keyword list and a fallback set for core SQL functionality. Understanding which words are reserved helps you write valid SQL and know when identifiers need quoting.

**Why it matters**

- **Identifier Conflicts**: Keywords cannot be used as unquoted identifiers without causing parse errors
- **Portability**: Different SQL dialects reserve different words; knowing ScratchBird's set aids migration
- **Query Writing**: Proper quoting of reserved words prevents syntax errors
- **Future Compatibility**: New keywords may be added as features expand

**How to use it**

Check the keyword lists below before naming tables, columns, or other database objects. If you must use a reserved word as an identifier, enclose it in double quotes. Keywords are case-insensitive in SQL but are normalized to uppercase internally.

## Keyword Recognition System

The lexer (`src/engine/lexer.cpp`) uses a two-tier keyword system:

1. **Primary Source**: Generated keywords from `include/scratchbird/engine/keywords_generated.h`
2. **Fallback Set**: Hard-coded keywords used when generated list is unavailable

This dual approach ensures core SQL functionality always works while allowing for dynamic keyword expansion.

## Core Fallback Keywords

These keywords are always recognized (`src/engine/lexer.cpp`, lines 320-334):

### Query Keywords
```sql
SELECT, FROM, WHERE, GROUP, HAVING, ORDER, BY, 
JOIN, LEFT, RIGHT, FULL, CROSS, NATURAL, ON, USING, LATERAL,
AS, WITH, RECURSIVE, UNION, ALL, INTERSECT, EXCEPT, DISTINCT,
OVER, PARTITION, ROWS, RANGE, FIRST, SKIP, FETCH, OFFSET, PLAN
```

### DML Keywords
```sql
INSERT, INTO, VALUES, UPDATE, SET, DELETE, DEFAULT
```

### DDL Keywords
```sql
CREATE, ALTER, DROP, TABLE, INDEX, CONSTRAINT, TRIGGER,
PRIMARY, KEY, FOREIGN, REFERENCES, UNIQUE, CHECK,
NOT, NULL, DEFERRABLE, INITIALLY, IMMEDIATE, DEFERRED
```

### Trigger Keywords
```sql
BEFORE, AFTER, FOR, EACH, ROW, STATEMENT, WHEN, ACTIVE, INACTIVE
```

### Logical and Comparison Keywords
```sql
AND, OR, IN, EXISTS, BETWEEN, LIKE, IS, TRUE, FALSE
```

## Extended Keyword Categories

Based on the parser implementation, additional keywords are recognized for specific features:

### Transaction and Session Keywords
```sql
BEGIN, COMMIT, ROLLBACK, SAVEPOINT, RELEASE,
START, TRANSACTION, WORK, READ, WRITE, ONLY,
ISOLATION, LEVEL, SERIALIZABLE, REPEATABLE, COMMITTED, UNCOMMITTED,
SET, NAMES, ROLE, DIALECT, CONNECT, DISCONNECT
```

### Data Type Keywords
```sql
INTEGER, BIGINT, SMALLINT, NUMERIC, DECIMAL, FLOAT, DOUBLE, PRECISION,
VARCHAR, CHAR, CHARACTER, TEXT, CITEXT,
DATE, TIME, TIMESTAMP, WITH, WITHOUT, ZONE,
BOOLEAN, BLOB, UUID, JSON, JSONB,
ARRAY, VECTOR, INET, CIDR, MACADDR
```

### PSQL/Procedural Keywords
```sql
EXECUTE, BLOCK, DECLARE, VARIABLE, BEGIN, END,
IF, THEN, ELSE, ELSIF, WHILE, FOR, DO, LOOP,
CONTINUE, LEAVE, EXIT, RETURN, RETURNS, SUSPEND,
EXCEPTION, WHEN, ANY, SQLCODE, GDSCODE,
CURSOR, OPEN, FETCH, CLOSE, NEXT, PRIOR,
PROCEDURE, FUNCTION, PACKAGE, BODY, LANGUAGE
```

### Advanced DDL Keywords
```sql
TABLESPACE, LOCATION, FILE, OWNER,
SEQUENCE, GENERATOR, INCREMENT, START, MINVALUE, MAXVALUE, CYCLE,
DOMAIN, COLLATE, COLLATION, CHARSET,
VIEW, MATERIALIZED, REFRESH, CONCURRENTLY,
FOREIGN, DATA, WRAPPER, SERVER, MAPPING, OPTIONS,
PUBLICATION, SUBSCRIPTION, REPLICA, IDENTITY,
POLICY, USING, PERMISSIVE, RESTRICTIVE,
CLUSTER, NODE, SERVICE, MASTER, SLAVE,
GRANT, REVOKE, PRIVILEGES, USAGE, EXECUTE, ADMIN, OPTION
```

### Window Function Keywords
```sql
WINDOW, OVER, PARTITION, ORDER, ROWS, RANGE, GROUPS,
UNBOUNDED, PRECEDING, FOLLOWING, CURRENT,
RANK, DENSE_RANK, ROW_NUMBER, NTILE, LAG, LEAD,
FIRST_VALUE, LAST_VALUE, NTH_VALUE
```

### Aggregate Function Keywords
```sql
COUNT, SUM, AVG, MIN, MAX, STDDEV, VARIANCE,
STRING_AGG, ARRAY_AGG, JSON_AGG, XMLAGG,
GROUPING, CUBE, ROLLUP, SETS
```

## Using Keywords as Identifiers

To use a reserved word as an identifier, you must quote it with double quotes:

### Examples of Quoted Keywords
```sql
-- CREATE is reserved, so quote it as a column name
CREATE TABLE user_actions (
    id INTEGER PRIMARY KEY,
    "CREATE" TIMESTAMP,     -- CREATE as column name
    "USER" VARCHAR(100),    -- USER as column name
    "SELECT" BOOLEAN        -- SELECT as column name
);

-- Using reserved words in queries
SELECT "CREATE", "USER", "SELECT"
FROM user_actions
WHERE "SELECT" = true;

-- Alias with reserved word
SELECT 
    ua.id,
    ua."CREATE" AS "WHEN",  -- Both CREATE and WHEN are reserved
    ua."USER" AS "BY"       -- Both USER and BY are reserved
FROM user_actions ua;
```

### Case Sensitivity with Quoted Identifiers
```sql
-- Quoted identifiers preserve case
CREATE TABLE "MyTable" (
    "ColumnName" INTEGER,
    "UPPERCASE" VARCHAR(50),
    "lowercase" TEXT,
    "MixedCase" BOOLEAN
);

-- Must use exact case when referencing
SELECT "ColumnName", "MixedCase"  -- Correct
FROM "MyTable";

-- These would fail (case mismatch):
-- SELECT ColumnName FROM MyTable;  -- Error: unquoted = uppercase
-- SELECT "columnname" FROM "mytable";  -- Error: wrong case
```

## Keyword Detection Algorithm

The lexer identifies keywords through this process (`src/engine/lexer.cpp::lex_ident_or_kw`):

1. Read identifier characters (letters, digits, underscore, dollar)
2. Convert to uppercase for comparison
3. Check against generated keyword table (if available)
4. Fall back to hard-coded keyword set
5. If not a keyword, treat as identifier (preserving original case)

```cpp
// Simplified keyword detection logic
std::string upper = to_upper(identifier);
if (is_in_generated_keywords(upper) || is_in_fallback_keywords(upper)) {
    return Token{TokenKind::Keyword, upper};
} else {
    return Token{TokenKind::Identifier, identifier};  // Original case
}
```

## Best Practices

### Naming Conventions
1. **Avoid keywords**: Choose descriptive names that don't conflict
2. **Use prefixes**: Add prefixes to avoid conflicts (e.g., `user_name` instead of `USER`)
3. **Consistent style**: Pick snake_case, camelCase, or PascalCase and stick to it

### Safe Identifier Examples
```sql
-- Good: Descriptive, non-conflicting names
CREATE TABLE customer_orders (
    order_id BIGINT PRIMARY KEY,
    customer_name VARCHAR(100),
    order_status VARCHAR(20),
    created_at TIMESTAMP
);

-- Avoid: Using keywords (requires quoting)
CREATE TABLE "ORDER" (
    "SELECT" INTEGER,
    "FROM" VARCHAR(50),
    "WHERE" BOOLEAN
);
```

### Migration Tips

When migrating from other databases:

```sql
-- PostgreSQL keywords that might not be reserved in ScratchBird
-- Still good practice to check or quote them
CREATE TABLE migrations (
    "VACUUM" TIMESTAMP,    -- VACUUM is PostgreSQL-specific
    "ANALYZE" BOOLEAN,     -- ANALYZE might be reserved
    "EXPLAIN" TEXT         -- EXPLAIN might be reserved
);

-- MySQL keywords that might differ
CREATE TABLE mysql_compat (
    "LIMIT" INTEGER,       -- LIMIT handling differs
    "REGEXP" VARCHAR(100), -- REGEXP operator syntax
    "DUAL" BOOLEAN         -- DUAL table concept
);
```

## Dynamic Keyword Expansion

The generated keywords file (`include/scratchbird/engine/keywords_generated.h`) can be regenerated to add new keywords as features are implemented. This allows the language to evolve while maintaining backward compatibility through the fallback mechanism.

## Implementation Details

**Source Files**:
- Keyword recognition: `src/engine/lexer.cpp` (lex_ident_or_kw function)
- Fallback list: `src/engine/lexer.cpp` (kFallbackKeywords, lines 320-334)
- Generated keywords: `include/scratchbird/engine/keywords_generated.h` (when available)

**Thread Safety**: Keyword lookup is read-only after initialization, making it thread-safe for concurrent parsing.

**Performance**: Keywords are stored in an unordered_set for O(1) average lookup time.

## See also

- [Lexical Structure](./sql-lexical.md) - Token types and identifier rules
- [Operators](./sql-operators.md) - Operator keywords and precedence
- [SQL Overview](./sql-overview.md) - Statement types and keywords
- [Data Types](./sql-data-types.md) - Type-related keywords