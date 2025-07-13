# ScratchBird Database v0.5

ScratchBird is an advanced open-source relational database management system based on Firebird 6.0 with significant enhancements for modern development. It provides PostgreSQL-compatible features while maintaining Firebird's proven reliability and performance.

## 🚀 Major Features

### 📊 **Advanced Datatype System**
- **Unsigned Integers**: USMALLINT, UINTEGER, UBIGINT, UINT128 (MySQL-compatible)
- **Large VARCHAR**: 128KB UTF-8 support (vs 32KB limit)
- **Network Types**: INET, CIDR, MACADDR (PostgreSQL-compatible)
- **Range Types**: INT4RANGE, INT8RANGE, NUMRANGE, TSRANGE, DATERANGE
- **Case-Insensitive Text**: CITEXT with automatic indexing
- **Advanced Arrays**: Multi-dimensional arrays with slicing operations
- **Full-Text Search**: TSVECTOR, TSQUERY with ranking and highlighting

### 🌳 **Hierarchical Schema System**
- **8-level deep schema nesting**: `finance.accounting.reports.table`
- **PostgreSQL-style qualified names**: Exceeds PostgreSQL capabilities  
- **Schema-aware database links**: Distributed database scenarios

### 🎯 **SQL Dialect 4 Enhancements**
- **FROM-less SELECT statements**: `SELECT GEN_UUID();`
- **Multi-row INSERT VALUES**: `INSERT INTO table VALUES (1,2,3),(4,5,6);`
- **Comprehensive SYNONYM support**: Schema-aware object aliasing

### 🔐 **Enhanced Security**
- **Trusted authentication**: GSSAPI/Kerberos integration
- **Advanced authentication plugins**: Multi-factor support
- **Schema-level security**: Granular access control

### 🔗 **Database Links**
- **5 schema resolution modes**: NONE, FIXED, CONTEXT_AWARE, HIERARCHICAL, MIRROR
- **Remote schema targeting**: Seamless distributed queries
- **Context-aware resolution**: CURRENT, HOME, USER schema references

## Tools

All ScratchBird tools are prefixed with `sb_` to avoid conflicts:

- `sb_isql` - Interactive SQL shell with schema commands
- `sb_gbak` - Database backup and restore
- `sb_gfix` - Database repair and validation
- `sb_gsec` - User and security management
- `sb_gstat` - Database statistics and analysis
- `sb_nbackup` - Online backup utility
- `sb_svcmgr` - Service manager
- `sb_tracemgr` - Trace and monitoring

## Quick Start

```bash
# Build ScratchBird
cd builds/cmake
cmake ..
make -j$(nproc)

# Start interactive shell
./src/isql/sb_isql

# Create hierarchical schema
CREATE SCHEMA finance;
CREATE SCHEMA finance.accounting;
CREATE SCHEMA finance.accounting.reports;

# FROM-less SELECT (SQL Dialect 4)
SELECT GEN_UUID();
SELECT CURRENT_TIMESTAMP;

# Multi-row INSERT (SQL Dialect 4)
INSERT INTO mytable(id, name) VALUES (1,'Alice'), (2,'Bob'), (3,'Carol');

# Create synonym
CREATE SYNONYM emp_data FOR hr.employees;
```

## Documentation

- [SQL Dialect 4 Reference](doc/README.sql_dialect_4.md)
- [Hierarchical Schemas Guide](doc/README.hierarchical_schemas.md)
- [Build Instructions](doc/README.build.posix.html)

## License

ScratchBird is released under the Initial Developer's Public License (IDPL).

## Contributing

ScratchBird is an active fork maintaining compatibility with Firebird while adding modern database features.
