# Firebird Emulation

[Emulation README](../README.md)

## Synopsis

ScratchBird's Firebird emulation provides wire-protocol and SQL compatibility with Firebird 4.0.

## Connection

### Wire Protocol

| Parameter | Value |
|-----------|-------|
| Protocol | Firebird wire protocol |
| Default Port | 3050 |
| SSL Support | Yes |
| Authentication | SRP, Legacy_Auth |

### Connection Examples

```
# Firebird client
isql -h localhost -p 3050 -u myuser -p secret mydb

# JDBC
jdbc:firebirdsql://localhost:3050/mydb?user=myuser&password=secret

# Python fdb
import fdb
con = fdb.connect(host='localhost', port=3050, database='mydb', user='myuser', password='secret')
```

## SQL Compatibility

### Firebird-Specific Features

| Feature | Support | Notes |
|---------|---------|-------|
| `GENERATOR` | ✅ | Mapped to SEQUENCE |
| `GEN_ID(generator, n)` | ✅ | Mapped to NEXTVAL |
| `EXECUTE BLOCK` | ✅ | Mapped to anonymous DO block |
| `EXECUTE PROCEDURE` | ✅ | Fully supported |
| `SELECT FIRST n` | ✅ | Mapped to LIMIT |
| `SELECT SKIP n` | ✅ | Mapped to OFFSET |
| `SELECT FIRST n SKIP m` | ✅ | Mapped to LIMIT/OFFSET |
| `PLAN` clause | ⚠️ | Ignored (SB uses optimizer) |
| `COLLATE` | ✅ | Supported |
| `CONTAINING` | ✅ | Case-insensitive LIKE |
| `STARTING WITH` | ✅ | Prefix search |

### PSQL (Procedural SQL)

| Feature | Support |
|---------|---------|
| `CREATE PROCEDURE` | ✅ |
| `CREATE FUNCTION` | ✅ |
| `CREATE TRIGGER` | ✅ |
| `EXECUTE BLOCK` | ✅ |
| `FOR SELECT...DO` | ✅ |
| `IF...THEN...ELSE` | ✅ |
| `WHILE...DO` | ✅ |
| `FOR EXECUTE STATEMENT` | ✅ |
| `EXCEPTION` handling | ✅ |
| `SUSPEND` | ✅ (procedures) |

### System Tables

| Firebird Table | SB Implementation |
|----------------|-------------------|
| `RDB$DATABASE` | Database info |
| `RDB$RELATIONS` | Tables |
| `RDB$RELATION_FIELDS` | Columns |
| `RDB$INDEX_SEGMENTS` | Index columns |
| `RDB$INDICES` | Indexes |
| `RDB$TRIGGERS` | Triggers |
| `RDB$PROCEDURES` | Procedures |
| `RDB$FUNCTIONS` | Functions |

## Type Mapping

| Firebird Type | SB Type | Notes |
|---------------|---------|-------|
| `SMALLINT` | `SMALLINT` | Direct |
| `INTEGER` | `INTEGER` | Direct |
| `BIGINT` | `BIGINT` | Direct |
| `INT128` | `NUMERIC(38)` | Large integer |
| `FLOAT` | `REAL` | Direct |
| `DOUBLE PRECISION` | `DOUBLE PRECISION` | Direct |
| `DECFLOAT(16)` | `DECIMAL(16)` | Decimal float |
| `DECFLOAT(34)` | `DECIMAL(34)` | Decimal float |
| `NUMERIC(p,s)` | `NUMERIC(p,s)` | Direct |
| `DECIMAL(p,s)` | `DECIMAL(p,s)` | Direct |
| `DATE` | `DATE` | Direct |
| `TIME` | `TIME` | Direct |
| `TIMESTAMP` | `TIMESTAMP` | Direct |
| `TIME WITH TIME ZONE` | `TIME WITH TIME ZONE` | Direct |
| `TIMESTAMP WITH TIME ZONE` | `TIMESTAMPTZ` | Direct |
| `CHAR(n)` | `CHAR(n)` | Direct |
| `VARCHAR(n)` | `VARCHAR(n)` | Direct |
| `BLOB SUB_TYPE TEXT` | `TEXT` | Text blob |
| `BLOB SUB_TYPE BINARY` | `BYTEA` | Binary blob |
| `BOOLEAN` | `BOOLEAN` | Direct |

## Functions

### Firebird-Specific Functions

| Firebird Function | SB Equivalent |
|-------------------|---------------|
| `GEN_ID(gen, n)` | `nextval('gen')` |
| `CAST(val AS type)` | `CAST(val AS type)` |
| `UPPER(str)` | `UPPER(str)` |
| `LOWER(str)` | `LOWER(str)` |
| `SUBSTRING(str FROM pos [FOR len])` | Same |
| `TRIM(str)` | `TRIM(str)` |
| `COALESCE(val1, val2, ...)` | Same |
| `NULLIF(val1, val2)` | Same |
| `IIF(cond, true_val, false_val)` | `CASE WHEN...` |
| `DECODE(val, match1, res1, ..., default)` | `CASE val WHEN...` |

### Context Variables

| Firebird Variable | SB Equivalent |
|-------------------|---------------|
| `CURRENT_USER` | `CURRENT_USER` |
| `CURRENT_ROLE` | `current_role()` |
| `CURRENT_DATE` | `CURRENT_DATE` |
| `CURRENT_TIME` | `CURRENT_TIME` |
| `CURRENT_TIMESTAMP` | `CURRENT_TIMESTAMP` |
| `ROW_COUNT` | `GET DIAGNOSTICS row_count` |

## Generators

Firebird generators map to SB sequences:

```sql
-- Firebird
CREATE GENERATOR my_gen;
SET GENERATOR my_gen TO 1000;
SELECT GEN_ID(my_gen, 1) FROM RDB$DATABASE;

-- SB equivalent
CREATE SEQUENCE my_gen START 1000;
SELECT nextval('my_gen');
```

## Limitations

1. **External tables**: Not supported (use FDW)
2. **UDFs**: Not supported (use SB native functions)
3. **Shadows**: Not supported (use SB replication)

## Migration

```bash
# Extract metadata
isql -x mydb.fdb > mydb.sql

# Convert and load
sb_fb_isql -f mydb.sql -d newdb

# Or use migration tool
sb_migrate --from=firebird --to=scratchbird --source=mydb.fdb --target=!:prod.newdb
```

## See Also

- [PostgreSQL Emulation](02_postgresql_emulation_surface.md)
- [MySQL Emulation](03_mysql_emulation_surface.md)
