# ScratchBird SQL Quick Reference Card

## Essential Commands - Quick Lookup

### Database & Schema
```sql
CREATE DATABASE db_name [PAGE_SIZE = 16K];
CREATE SCHEMA schema_name;
SET SCHEMA schema_name;
SET SEARCH_PATH TO schema1, schema2;
```

### Tables
```sql
CREATE TABLE table_name (
  id INT128 PRIMARY KEY,
  name VARCHAR(100) NOT NULL,
  created TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

ALTER TABLE table_name ADD COLUMN new_col TYPE;
DROP TABLE [IF EXISTS] table_name [CASCADE];
```

### Basic DML
```sql
-- Insert
INSERT INTO table (col1, col2) VALUES (val1, val2);
INSERT INTO table VALUES (row1), (row2), (row3);  -- Bulk

-- Update  
UPDATE table SET col = value WHERE condition;

-- Delete
DELETE FROM table WHERE condition;

-- Select
SELECT * FROM table WHERE condition ORDER BY col LIMIT 10;
```

### Joins
```sql
SELECT * FROM t1 
  INNER JOIN t2 ON t1.id = t2.t1_id
  LEFT JOIN t3 ON t2.id = t3.t2_id;
```

### Transactions
```sql
BEGIN;
  -- statements
COMMIT;  -- or ROLLBACK;

SAVEPOINT sp1;
ROLLBACK TO sp1;
```

### Indexes
```sql
CREATE INDEX idx_name ON table(column);
CREATE UNIQUE INDEX idx_name ON table(col1, col2);
DROP INDEX idx_name;
```

### Views
```sql
CREATE VIEW view_name AS SELECT ...;
CREATE MATERIALIZED VIEW mv_name AS SELECT ...;
REFRESH MATERIALIZED VIEW mv_name;
```

### Functions/Procedures
```sql
CREATE FUNCTION func_name(param TYPE) 
RETURNS TYPE AS $$
BEGIN
  RETURN value;
END;
$$ LANGUAGE plpgsql;

CREATE PROCEDURE proc_name(param TYPE) AS $$
BEGIN
  -- statements
END;
$$ LANGUAGE plpgsql;

CALL proc_name(args);
```

### Triggers
```sql
CREATE TRIGGER trig_name
  BEFORE INSERT ON table
  FOR EACH ROW
  EXECUTE FUNCTION func_name();
```

### Users & Permissions
```sql
CREATE USER username PASSWORD 'password';
GRANT SELECT, INSERT ON table TO user;
REVOKE ALL ON table FROM user;
```

### System Commands
```sql
SHOW TABLES;
DESCRIBE table_name;
EXPLAIN ANALYZE SELECT ...;
SHOW SEARCH_PATH;
SET parameter = value;
```

### CTEs & Window Functions
```sql
WITH cte AS (SELECT ...)
SELECT * FROM cte;

SELECT *, ROW_NUMBER() OVER (PARTITION BY col ORDER BY col2) AS rn
FROM table;
```

### Special Types
```sql
UUID         -- UUID type
INT128       -- 128-bit integer  
JSON/JSONB   -- JSON storage
ARRAY        -- Arrays: INT[]
INTERVAL     -- Time intervals
```

### PSQL Control Flow
```sql
IF condition THEN ... END IF;
CASE WHEN condition THEN ... END CASE;
FOR i IN 1..10 LOOP ... END LOOP;
WHILE condition LOOP ... END LOOP;
```

### Error Handling
```sql
BEGIN
  -- statements
EXCEPTION
  WHEN division_by_zero THEN ...
  WHEN OTHERS THEN ...
END;
```

### Debugging
```sql
RAISE NOTICE 'Value: %', variable;
RAISE DEBUG 'Debug info';
ASSERT condition, 'message';
```

### Compatibility Shortcuts

**PostgreSQL Style:**
```sql
value::type              -- Casting
$$string$$              -- Dollar quotes
table.*                 -- All columns
```

**MySQL Style:**
```sql
`identifier`            -- Backticks
LIMIT 10                -- Simple limit
SHOW TABLES;            -- Show command
```

**Firebird Style:**
```sql
EXECUTE BLOCK AS BEGIN ... END
LIST(column, ',')       -- Aggregate list
GEN_ID(gen, 1)         -- Generator
```

**MSSQL Style:**
```sql
[identifier]            -- Square brackets
TOP 10                  -- Top clause
IDENTITY(1,1)          -- Auto-increment
```

## Data Types Quick Reference

| Category | Types |
|----------|-------|
| **Integers** | SMALLINT, INT, BIGINT, INT128, UINT8-64 |
| **Decimal** | DECIMAL(p,s), NUMERIC(p,s), MONEY |
| **Float** | REAL, DOUBLE PRECISION |
| **String** | CHAR(n), VARCHAR(n), TEXT |
| **Binary** | BYTEA, BLOB, VARBINARY(n) |
| **Date/Time** | DATE, TIME, TIMESTAMP, INTERVAL |
| **Boolean** | BOOLEAN, BOOL |
| **Special** | UUID, JSON, JSONB, XML, ARRAY |

## Page Sizes
- 8K, 16K, 32K, 64K, 128K

## Isolation Levels
- READ COMMITTED (default)
- REPEATABLE READ
- SERIALIZABLE

## Index Types
- BTREE (default)
- HASH
- GIN
- BITMAP
- RTREE
- LSM
- COLUMNSTORE

## Join Types
- INNER JOIN
- LEFT/RIGHT/FULL OUTER JOIN
- CROSS JOIN
- LATERAL JOIN

## Aggregate Functions
- COUNT, SUM, AVG, MIN, MAX
- STRING_AGG, ARRAY_AGG, JSON_AGG
- LIST (Firebird-style)

## Window Functions
- ROW_NUMBER(), RANK(), DENSE_RANK()
- LAG(), LEAD()
- FIRST_VALUE(), LAST_VALUE()

## Common Patterns

### Upsert
```sql
INSERT INTO table VALUES (...) 
ON CONFLICT (id) DO UPDATE SET ...;
```

### Recursive CTE
```sql
WITH RECURSIVE cte AS (
  SELECT ... -- anchor
  UNION ALL
  SELECT ... FROM cte -- recursive
)
SELECT * FROM cte;
```

### Pivot
```sql
SELECT * FROM crosstab(...) AS ct(...);
```

### Bulk Operations
```sql
COPY table FROM 'file.csv' CSV HEADER;
COPY table TO 'file.csv' CSV HEADER;
```

---

**Remember**: 
- Context-aware parsing minimizes reserved words
- All objects internally use UUIDs
- Multi-protocol support via Y-Valve
- 128-bit integer support native
- MGA provides MVCC without read locks