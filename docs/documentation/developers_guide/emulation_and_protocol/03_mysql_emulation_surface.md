# MySQL Emulation

[Emulation README](../README.md)

## Synopsis

ScratchBird's MySQL emulation provides wire-protocol and SQL compatibility with MySQL 8.0.

## Connection

### Wire Protocol

| Parameter | Value |
|-----------|-------|
| Protocol | MySQL Client/Server |
| Default Port | 3306 |
| SSL Support | Yes |
| Authentication | mysql_native_password, caching_sha2_password |

### Connection Examples

```
# MySQL client
mysql -h localhost -P 3306 -u myuser -p mydb

# JDBC
jdbc:mysql://localhost:3306/mydb?user=myuser&password=secret

# Python mysql-connector
mysql+mysqlconnector://myuser:secret@localhost:3306/mydb
```

## SQL Compatibility

### MySQL-Specific Syntax

| Feature | Support | Notes |
|---------|---------|-------|
| `LIMIT n` | ✅ | Fully supported |
| `LIMIT offset, count` | ✅ | Fully supported |
| `AUTO_INCREMENT` | ✅ | Maps to SERIAL |
| `INT(n)` display width | ✅ | Parsed but ignored |
| `ENUM` type | ⚠️ | Map to CHECK constraint |
| `SET` type | ⚠️ | Map to array + CHECK |
| `TINYINT(1)` as boolean | ✅ | Mapped to BOOLEAN |
| `IFNULL()` | ✅ | Same as COALESCE |
| `CONCAT()` (variadic) | ✅ | Supported |

### System Variables

| Variable | Behavior |
|----------|----------|
| `@@version` | Returns SB version |
| `@@max_connections` | Maps to SB setting |
| `@@character_set_server` | Returns 'UTF8MB4' |
| `@@sql_mode` | Emulated modes |

### Information Schema

| MySQL View | SB Implementation |
|------------|-------------------|
| `information_schema.tables` | Filtered SB tables |
| `information_schema.columns` | Column metadata |
| `information_schema.indexes` | Index metadata |
| `information_schema.statistics` | Stats mapping |

## Type Mapping

| MySQL Type | SB Type | Notes |
|------------|---------|-------|
| `TINYINT` | `SMALLINT` | Maps to 2-byte |
| `TINYINT(1)` | `BOOLEAN` | Boolean mapping |
| `SMALLINT` | `SMALLINT` | Direct |
| `MEDIUMINT` | `INTEGER` | Maps to 4-byte |
| `INT` | `INTEGER` | Direct |
| `BIGINT` | `BIGINT` | Direct |
| `FLOAT` | `REAL` | Direct |
| `DOUBLE` | `DOUBLE PRECISION` | Direct |
| `DECIMAL(p,s)` | `NUMERIC(p,s)` | Direct |
| `CHAR(n)` | `CHAR(n)` | Direct |
| `VARCHAR(n)` | `VARCHAR(n)` | Direct |
| `TEXT` | `TEXT` | Direct |
| `DATETIME` | `TIMESTAMP` | No timezone |
| `TIMESTAMP` | `TIMESTAMPTZ` | With timezone |
| `JSON` | `JSONB` | Binary JSON |

## Functions

### String Functions

| MySQL Function | SB Equivalent |
|----------------|---------------|
| `CONCAT(str1, str2, ...)` | `\|\|` or `CONCAT()` |
| `SUBSTRING(str, pos, len)` | `SUBSTRING(str FROM pos FOR len)` |
| `LENGTH(str)` | `LENGTH(str)` |
| `CHAR_LENGTH(str)` | `CHAR_LENGTH(str)` |
| `LOWER(str)` | `LOWER(str)` |
| `UPPER(str)` | `UPPER(str)` |

### Date Functions

| MySQL Function | SB Equivalent |
|----------------|---------------|
| `NOW()` | `NOW()` |
| `CURDATE()` | `CURRENT_DATE` |
| `CURTIME()` | `CURRENT_TIME` |
| `DATE_ADD(date, INTERVAL n unit)` | `date + INTERVAL 'n' unit` |
| `DATEDIFF(d1, d2)` | `d1 - d2` |

## Limitations

1. **Storage Engines**: Only MGA (no MyISAM, InnoDB emulation)
2. **Replication**: Use SB native replication
3. **Stored Procedures**: May need syntax adjustments

## Testing

```bash
# Test with mysql client
mysql -h localhost -P 3306 -e "SELECT VERSION();"

# Run compatibility tests
./test-scratchbird.sh --suite=mysql
```

## Migration Notes

1. **Dump and Restore**: Use `mysqldump` → import via sb_my_isql
2. **Schema Conversion**: Check ENUM, SET types
3. **Auto Increment**: SB uses SERIAL or IDENTITY
4. **Timestamps**: MySQL TIMESTAMP behavior differs

## See Also

- [PostgreSQL Emulation](02_postgresql_emulation_surface.md)
- [Firebird Emulation](04_firebird_emulation_surface.md)
