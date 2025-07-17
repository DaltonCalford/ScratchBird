# ScratchBird v0.5.0 - Examples and Documentation

## Overview

This directory contains comprehensive examples demonstrating ScratchBird's advanced features, including hierarchical schemas, PostgreSQL-compatible data types, and modern C++ interfaces.

## Quick Start

Before running examples, ensure ScratchBird is installed and running:

```bash
# Start ScratchBird server
scratchbird

# Set environment variables
export SB_USER=SYSDBA
export SB_PASSWORD=masterkey
export SB_DATABASE=localhost:employee.fdb
export SB_PORT=4050
```

## Example Categories

### 1. Hierarchical Schemas (`schemas/`)
Advanced schema management examples showcasing ScratchBird's 8-level schema hierarchy:
- **schema_basic.sql** - Basic schema creation and usage
- **schema_hierarchy.sql** - Deep nested schema structures
- **schema_navigation.sql** - Schema context switching
- **schema_management.sql** - Administrative functions
- **schema_migration.sql** - Schema evolution patterns

### 2. PostgreSQL-Compatible Data Types (`datatypes/`)
Examples of ScratchBird's advanced data type support:
- **network_types.sql** - INET, CIDR, MACADDR usage
- **uuid_types.sql** - UUID IDENTITY and generation
- **unsigned_integers.sql** - USMALLINT, UINTEGER, UBIGINT
- **range_types.sql** - Date, time, and numeric ranges
- **enhanced_varchar.sql** - 128KB UTF-8 text storage

### 3. Database Links (`database_links/`)
Schema-aware database link examples:
- **basic_links.sql** - Simple database link setup
- **schema_aware_links.sql** - Advanced schema targeting
- **link_resolution_modes.sql** - 5 resolution modes
- **distributed_queries.sql** - Cross-database operations

### 4. C++ API Interface (`api/`)
Modern C++ interface examples:
- **sb_api_01_create.cpp** - Database creation
- **sb_api_02_schemas.cpp** - Schema operations
- **sb_api_03_datatypes.cpp** - Advanced data types
- **sb_api_04_transactions.cpp** - Transaction management
- **sb_api_05_prepared.cpp** - Prepared statements
- **sb_api_06_blobs.cpp** - BLOB handling
- **sb_api_07_events.cpp** - Event handling
- **sb_api_08_services.cpp** - Service API usage

### 5. Modern C++ Interfaces (`interfaces/`)
Object-oriented API examples:
- **01.create_database.cpp** - Database creation with schemas
- **02.hierarchical_schemas.cpp** - Schema hierarchy management
- **03.advanced_datatypes.cpp** - PostgreSQL-compatible types
- **04.database_links.cpp** - Remote database connections
- **05.batch_operations.cpp** - Bulk operations
- **06.event_handling.cpp** - Asynchronous events
- **07.service_management.cpp** - Service operations

### 6. SQL Examples (`sql/`)
Comprehensive SQL examples:
- **basic_operations.sql** - CRUD operations with schemas
- **advanced_queries.sql** - Complex queries with hierarchical data
- **stored_procedures.sql** - PSQL with schema awareness
- **triggers.sql** - Schema-aware triggers
- **functions.sql** - User-defined functions

### 7. Utilities Examples (`utilities/`)
Examples for ScratchBird utilities:
- **backup_restore.sh** - sb_gbak with schema preservation
- **incremental_backup.sh** - sb_nbackup examples
- **monitoring.sh** - sb_lock_print and sb_tracemgr
- **service_management.sh** - sb_svcmgr operations

### 8. Configuration (`config/`)
Configuration examples:
- **scratchbird.conf** - Main server configuration
- **databases.conf** - Database aliases with schemas
- **schema_security.conf** - Schema-level security
- **database_links.conf** - Link configuration

## Environment Setup

### Required Environment Variables
```bash
# ScratchBird connection parameters
export SB_USER=SYSDBA
export SB_PASSWORD=masterkey
export SB_DATABASE=localhost:employee.fdb
export SB_PORT=4050

# Schema context (optional)
export SB_SCHEMA=company.finance.accounting
export SB_HOME_SCHEMA=company.finance
```

### Database Setup
```bash
# Create example database
sb_isql -user $SB_USER -password $SB_PASSWORD -q << EOF
CREATE DATABASE '$SB_DATABASE' 
  DEFAULT CHARACTER SET UTF8 
  SQL DIALECT 4;
EOF

# Create hierarchical schema structure
sb_isql -user $SB_USER -password $SB_PASSWORD -database $SB_DATABASE -input schemas/schema_hierarchy.sql
```

## Key Features Demonstrated

### 1. Hierarchical Schema System
- **8-level deep nesting**: `company.division.department.team.project.environment.component.module`
- **PostgreSQL-compatible syntax**: `CREATE SCHEMA finance.accounting`
- **Schema context management**: `SET SCHEMA`, `SET HOME SCHEMA`
- **Administrative functions**: Schema validation and management

### 2. PostgreSQL-Compatible Data Types
- **Network Types**: INET, CIDR, MACADDR with full operator support
- **UUID Types**: UUID IDENTITY with automatic generation (v1, v4, v6, v7, v8)
- **Unsigned Integers**: USMALLINT, UINTEGER, UBIGINT, UINT128
- **Range Types**: INT4RANGE, TSRANGE, DATERANGE with operators
- **Enhanced Text**: VARCHAR(128KB) with UTF-8 support

### 3. Schema-Aware Database Links
- **5 Resolution Modes**: NONE, FIXED, CONTEXT_AWARE, HIERARCHICAL, MIRROR
- **Context References**: CURRENT, HOME, USER schema resolution
- **Hierarchical Mapping**: Local to remote schema path mapping
- **Remote Validation**: Schema existence checking before connection

### 4. Modern C++17 Implementation
- **GPRE-Free**: No preprocessor dependencies
- **Exception Handling**: Modern C++ exception patterns
- **Template Usage**: Type-safe operations
- **STL Integration**: Standard library compatibility

## Building Examples

### C++ Examples
```bash
# Build all C++ examples
cd examples
make all

# Build specific category
make api
make interfaces

# Build with debug information
make debug
```

### Compilation Commands
```bash
# Basic C++ example
g++ -std=c++17 -I../include -o sb_api_01 api/sb_api_01_create.cpp -lsbclient

# With schema support
g++ -std=c++17 -I../include -DSB_SCHEMA_SUPPORT -o sb_api_02 api/sb_api_02_schemas.cpp -lsbclient

# Windows cross-compilation
x86_64-w64-mingw32-g++ -std=c++17 -I../include -o sb_api_01.exe api/sb_api_01_create.cpp -lsbclient
```

## Running Examples

### SQL Examples
```bash
# Run schema examples
sb_isql -user $SB_USER -password $SB_PASSWORD -database $SB_DATABASE -input schemas/schema_basic.sql

# Run with specific schema context
sb_isql -user $SB_USER -password $SB_PASSWORD -database $SB_DATABASE -schema finance.accounting -input sql/basic_operations.sql
```

### C++ Examples
```bash
# Run API examples
cd api
./sb_api_01_create
./sb_api_02_schemas

# Run with environment variables
SB_SCHEMA=finance.accounting ./sb_api_03_datatypes
```

### Utility Examples
```bash
# Run utility examples
cd utilities
./backup_restore.sh
./monitoring.sh
```

## Common Patterns

### Schema Management
```sql
-- Create hierarchical schema
CREATE SCHEMA company.finance.accounting;

-- Set working schema
SET SCHEMA 'company.finance.accounting';

-- Create objects in current schema
CREATE TABLE monthly_reports (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    report_date DATE,
    amount DECIMAL(15,2)
);
```

### Database Links
```sql
-- Create schema-aware database link
CREATE DATABASE LINK finance_link
  TO 'server2:finance_db'
  USER 'dbuser' PASSWORD 'pass'
  SCHEMA_MODE HIERARCHICAL
  LOCAL_SCHEMA 'finance'
  REMOTE_SCHEMA 'accounting';

-- Use link with automatic schema resolution
SELECT * FROM employees@finance_link;
```

### Advanced Data Types
```sql
-- Network types
CREATE TABLE servers (
    id UUID GENERATED ALWAYS AS IDENTITY,
    ip_address INET,
    network CIDR,
    mac_address MACADDR
);

-- Range types
CREATE TABLE schedules (
    id UUID GENERATED ALWAYS AS IDENTITY,
    time_slot TSRANGE,
    date_range DATERANGE
);
```

## Troubleshooting

### Common Issues
1. **Connection Refused**: Ensure ScratchBird server is running on port 4050
2. **Schema Not Found**: Check schema hierarchy and spelling
3. **Data Type Errors**: Verify SQL Dialect 4 is enabled
4. **Link Failures**: Validate remote schema existence

### Debug Information
```bash
# Enable verbose output
export SB_DEBUG=1

# Check server status
scratchbird -status

# Verify schema structure
sb_isql -user $SB_USER -password $SB_PASSWORD -database $SB_DATABASE -q << EOF
SELECT * FROM RDB$SCHEMAS ORDER BY RDB$SCHEMA_LEVEL, RDB$SCHEMA_NAME;
EOF
```

## Performance Tips

1. **Use schema-specific queries** to reduce search scope
2. **Leverage prepared statements** for repeated operations
3. **Implement connection pooling** for high-concurrency applications
4. **Monitor schema access patterns** with sb_lock_print
5. **Use incremental backups** with sb_nbackup for large databases

## Contributing

To add new examples:
1. Follow the existing directory structure
2. Include comprehensive comments and documentation
3. Test examples on multiple platforms
4. Update this README with new examples
5. Follow ScratchBird coding standards

## License

All examples are released under the Initial Developer's Public License (IDPL), maintaining compatibility with ScratchBird's licensing.

---

**ScratchBird Examples v0.5.0**
**Build Version**: SB-T0.5.0.1 ScratchBird 0.5 f90eae0
**Documentation Date**: $(date)