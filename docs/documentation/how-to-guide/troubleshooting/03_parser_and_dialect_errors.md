# Troubleshooting Parser and Dialect Errors

[Troubleshooting README](../README.md)

## Syntax Errors

### "syntax error at or near"

**Symptoms:**
- `ERROR: syntax error at or near "xxx"`

**Common Causes:**
1. **Reserved word as identifier**
   ```sql
   -- Wrong
   CREATE TABLE user (id INT);
   
   -- Correct
   CREATE TABLE "user" (id INT);
   -- Or use different name
   CREATE TABLE app_user (id INT);
   ```

2. **Missing comma**
   ```sql
   -- Wrong
   SELECT id name FROM users;
   
   -- Correct
   SELECT id, name FROM users;
   ```

3. **Unclosed quotes**
   ```sql
   -- Wrong
   SELECT * FROM users WHERE name = 'John;
   
   -- Correct
   SELECT * FROM users WHERE name = 'John';
   ```

## Dialect-Specific Issues

### PostgreSQL Mode

**Issue:** `ERROR: function xxx does not exist`
- Some PostgreSQL-specific functions may not be available
- Check function catalog for SB equivalents

**Resolution:**
```sql
-- Check available functions
SELECT proname FROM pg_proc 
WHERE proname LIKE '%my_function%';

-- Use SB native equivalent
```

### MySQL Mode

**Issue:** `ERROR: column "xxx" does not exist`
- MySQL is case-insensitive for column names by default
- SB (in MySQL mode) preserves case but requires exact match

**Resolution:**
```sql
-- Quote column names with special characters
SELECT `Column Name` FROM mytable;

-- Or use correct case
SELECT column_name FROM mytable;
```

**Issue:** `LIMIT` syntax
```sql
-- MySQL style (works in SB MySQL mode)
SELECT * FROM users LIMIT 10, 20;  -- Offset 10, limit 20

-- Standard SQL style (works in all modes)
SELECT * FROM users LIMIT 20 OFFSET 10;
```

### Firebird Mode

**Issue:** `GENERATOR` not found
```sql
-- Firebird syntax
SELECT GEN_ID(my_gen, 1) FROM RDB$DATABASE;

-- SB equivalent
SELECT nextval('my_gen');
```

## Path Resolution Errors

### "invalid path syntax"

**Symptoms:**
- `ERROR: invalid path syntax`

**Common Mistakes:**
```sql
-- Wrong: Missing colon
SELECT * FROM !prod.mydb.table;

-- Correct
SELECT * FROM !:prod.mydb.table;

-- Wrong: Too many dots
SELECT * FROM !:prod.mydb.schema.table.extra;

-- Correct
SELECT * FROM !:prod.mydb.schema.table;
```

### "schema does not exist"

**Diagnosis:**
```sql
-- Check available schemas
SELECT schema_name FROM information_schema.schemata;

-- Check current search path
SHOW search_path;
```

**Resolution:**
```sql
-- Use fully qualified path
SELECT * FROM !:prod.mydb.public.table;

-- Or set search path
SET search_path TO myschema, public;
```

## Emulation-Specific Limitations

### PostgreSQL Extensions

**Issue:** Extension not available
```sql
-- This may fail
CREATE EXTENSION postgis;

-- Check available extensions
SELECT * FROM pg_available_extensions;
```

**Resolution:**
- Check SB extension list
- Use FDW for external functionality
- Implement as UDR

### MySQL Storage Engines

**Issue:** `ENGINE=InnoDB` not recognized
- SB uses MGA storage engine exclusively
- Ignore or remove ENGINE clauses

## Debugging Parser Issues

```sql
-- Check which parser is active
SHOW parser_mode;

-- Get detailed error
-- Run with sb_isql -v for verbose output

-- Test with native parser
SET parser_mode = 'native';
```

## Migration Syntax Issues

### From PostgreSQL

| PostgreSQL | ScratchBird |
|------------|-------------|
| `::type` cast | Works ✅ |
| `TEXT[]` arrays | Works ✅ |
| `ROW()` constructor | Works ✅ |
| `COPY FROM PROGRAM` | Use FDW |

### From MySQL

| MySQL | ScratchBird |
|-------|-------------|
| `INSERT INTO ... ON DUPLICATE KEY` | Use `ON CONFLICT` |
| `GROUP_CONCAT()` | Use `STRING_AGG()` |
| `FIND_IN_SET()` | Use array operators |

### From Firebird

| Firebird | ScratchBird |
|----------|-------------|
| `EXECUTE BLOCK` | Use `DO $$ ... $$` |
| `PLAN` clause | Ignored by optimizer |
| `ROWS n` | Use `LIMIT n` |

## See Also

- [Error Codes Reference](../../../error_and_diagnostics_reference/error_model_and_contracts/02_error_identifiers_and_code_system.md)
- [Syntax Guide](../../../language_reference/syntax_guide/README.md)
