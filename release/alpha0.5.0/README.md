# ScratchBird Alpha 0.5.0 Release

## About ScratchBird

ScratchBird is an advanced, PostgreSQL-compatible database management system based on Firebird with extensive enhancements for modern database applications.

## Key Features in Alpha 0.5.0

### 🔗 Hierarchical Schema Support
- **8-level deep schema nesting**: Create complex schema hierarchies like `finance.accounting.reports.table`
- **PostgreSQL-style syntax**: Full compatibility with PostgreSQL hierarchical schema patterns
- **Administrative functions**: Complete schema management with integrity validation

### 🌐 Schema-Aware Database Links  
- **5 resolution modes**: NONE, FIXED, CONTEXT_AWARE, HIERARCHICAL, MIRROR
- **Remote schema targeting**: Access specific schemas on remote databases
- **Context-aware resolution**: Support for CURRENT, HOME, USER schema references

### 🆔 UUID IDENTITY Columns
- **PostgreSQL compatibility**: Standard UUID data type with IDENTITY syntax
- **Automatic generation**: Built-in UUID generation for primary keys
- **Full SQL standard compliance**: Compatible with modern database standards

### 📊 Unsigned Integer Types
- **Complete type system**: USMALLINT, UINTEGER, UBIGINT, UINT128
- **Performance optimized**: Native unsigned integer arithmetic
- **Firebird extension**: Enhanced beyond standard Firebird capabilities

### 📝 Enhanced DDL Support
- **COMMENT ON SCHEMA**: Full commenting support for hierarchical schemas
- **Range types**: Parser infrastructure for PostgreSQL-style range types
- **Extended CREATE USER**: HOME SCHEMA assignment for users

## Release Contents

```
alpha0.5.0/
├── bin/                    # Executables and scripts
│   ├── sb_config          # Configuration utility
│   └── ScratchBirdUninstall.sh
├── lib/                    # Libraries (to be populated by build)
├── include/               # Header files for development
├── conf/                  # Configuration directory
├── doc/                   # Documentation
├── examples/              # Example code and SQL
├── scratchbird.conf       # Main configuration file
├── databases.conf         # Database configuration
├── plugins.conf           # Plugin configuration
├── sbtrace.conf          # Trace configuration
├── replication.conf       # Replication settings
└── VERSION               # Version and build information
```

## Installation

1. Extract the release package
2. Configure `scratchbird.conf` for your environment
3. Set up database paths in `databases.conf`
4. Run installation scripts in `bin/`

## Development Status

This is an alpha release focused on demonstrating the advanced schema and database link capabilities. Full tool compilation and packaging is in progress.

**Core Features**: ✅ Complete  
**Tool Build System**: 🔄 In Progress  
**Documentation**: 📚 Available in doc/  

## Advanced Schema Examples

```sql
-- Create hierarchical schema structure
CREATE SCHEMA finance;
CREATE SCHEMA finance.accounting;
CREATE SCHEMA finance.accounting.reports;

-- Create table in nested schema
CREATE TABLE finance.accounting.reports.monthly_summary (
    id UUID IDENTITY,
    month_name VARCHAR(20),
    total_amount DECIMAL(15,2)
);

-- Schema-aware database link
CREATE DATABASE LINK finance_link 
  TO 'server2:finance_db' 
  USER 'dbuser' PASSWORD 'pass'
  SCHEMA_MODE HIERARCHICAL
  REMOTE_SCHEMA 'accounting.reports';

-- Query via database link
SELECT * FROM monthly_summary@finance_link;
```

## Documentation

Complete documentation is available in the `doc/` directory covering:
- Hierarchical schema implementation
- Database link configuration
- UUID IDENTITY usage
- Migration from Firebird
- Advanced SQL features

## Support

This is an alpha release for testing and evaluation. For issues and feedback:
- Review documentation in `doc/`
- Check examples in `examples/`
- Submit feedback through project channels

---

**ScratchBird Alpha 0.5.0** - Advanced Database Management System  
Build Date: Mon 14 Jul 2025 10:41:50 PM EDT  
Platform: Linux x86_64