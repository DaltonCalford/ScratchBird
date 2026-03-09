# PostgreSQL Emulation

[Emulation README](../README.md)

## Synopsis

ScratchBird's PostgreSQL emulation provides wire-protocol and SQL compatibility with PostgreSQL 15.

## Connection

### Wire Protocol

| Parameter | Value |
|-----------|-------|
| Protocol | PostgreSQL Frontend/Backend |
| Default Port | 5432 |
| SSL Support | Yes (TLS 1.3) |
| Authentication | MD5, SCRAM-SHA-256, trust, reject |

### Connection String Examples

```
# libpq format
host=localhost port=5432 dbname=mydb user=myuser password=secret

# JDBC
jdbc:postgresql://localhost:5432/mydb?user=myuser&password=secret

# Python psycopg2
postgresql://myuser:secret@localhost:5432/mydb
```

## SQL Compatibility

### Supported Statements

| Category | Support Level |
|----------|---------------|
| SELECT, INSERT, UPDATE, DELETE | Full |
| JOINs (all types) | Full |
| CTEs (WITH, RECURSIVE) | Full |
| Window Functions | Full |
| Transactions | Full |
| DDL (CREATE, ALTER, DROP) | Full |
| Functions and Procedures | Full |
| Triggers | Full |
| Views | Full |
| Materialized Views | Full |

### PostgreSQL-Specific Features

| Feature | Status | Notes |
|---------|--------|-------|
| `pg_typeof()` | ✅ Supported | Returns SB native type |
| `current_schema()` | ✅ Supported | Maps to SB schema |
| `pg_stat_activity` | ✅ Supported | SB-compatible view |
| `information_schema` | ✅ Supported | Filtered view |
| `pg_catalog` | ✅ Supported | Emulated catalog |
| Custom types | ⚠️ Limited | Map to SB types |
| Extensions | ⚠️ Limited | Check compatibility |

### System Catalog Mapping

| PostgreSQL View | SB Implementation |
|-----------------|-------------------|
| `pg_tables` | Filtered view of SB tables |
| `pg_indexes` | Maps to SB indexes |
| `pg_attribute` | Column metadata |
| `pg_class` | Table/index metadata |
| `pg_type` | Type system mapping |
| `pg_database` | Environment databases |

## Type Mapping

| PostgreSQL Type | SB Type | Notes |
|-----------------|---------|-------|
| `integer` | `INTEGER` | Direct mapping |
| `bigint` | `BIGINT` | Direct mapping |
| `text` | `TEXT` | Direct mapping |
| `varchar(n)` | `VARCHAR(n)` | Direct mapping |
| `timestamp` | `TIMESTAMP` | Direct mapping |
| `timestamptz` | `TIMESTAMPTZ` | Direct mapping |
| `json` | `JSON` | Direct mapping |
| `jsonb` | `JSONB` | Direct mapping |
| `uuid` | `UUID` | SB uses v7 |
| `serial` | `SERIAL` | Same behavior |
| `bytea` | `BYTEA` | Direct mapping |

## Functions

### Supported Functions

All standard PostgreSQL functions are supported through SB implementation:
- String functions
- Numeric functions
- Date/time functions
- JSON functions
- Array functions

### System Functions

| Function | Behavior |
|----------|----------|
| `version()` | Returns SB version info |
| `current_setting()` | Maps to SB settings |
| `set_config()` | Maps to SB configuration |

## Limitations

1. **Extensions**: Some PostgreSQL extensions may not be available
2. **Native C functions**: Cannot load external PostgreSQL extensions
3. **WAL-level features**: MGA architecture differs from PostgreSQL WAL

## Testing Compatibility

```bash
# Run PostgreSQL regression tests
./test-scratchbird.sh --suite=postgresql

# Test specific features
sb_pg_isql -c "SELECT version();"
sb_pg_isql -c "SHOW server_version;"
```

## See Also

- [MySQL Emulation](03_mysql_emulation_surface.md)
- [Firebird Emulation](04_firebird_emulation_surface.md)
- [Protocol Frame Boundaries](05_protocol_frame_boundaries.md)
