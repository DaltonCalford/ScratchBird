# ScratchBird
An experimental, refactored fork of FirebirdSQL—built to scratch an itch.

[![Build Status](https://img.shields.io/badge/build-in%20progress-orange)](https://github.com/dcalford/ScratchBird) [![License](https://img.shields.io/badge/license-IDPL-blue)](LICENSE) [![Version](https://img.shields.io/badge/version-0.5--alpha-red)](CHANGELOG.md)

## What is ScratchBird?

ScratchBird is a fork of the powerful and mature FirebirdSQL database. This project involves a significant refactoring of the original codebase with the goal of cleaning up internals, modernizing components, and implementing new features. It's a passion project driven by curiosity and the desire to experiment.

**Important Note on Contributions**: ScratchBird is a divergent fork and is not intended to be merged back into the main FirebirdSQL codebase. The project's experimental nature means that core components are changing daily, and the architecture has drifted significantly from the original.

While we are grateful for the Firebird foundation, the goal of ScratchBird is to be a separate, exploratory project. We encourage contributions within the context of ScratchBird's unique direction.

## The "Itch to Scratch" Philosophy 🤔

The name ScratchBird was chosen for two main reasons:

**To Scratch an Itch**: This fork was started because I have a personal 'itch' to explore database internals, implement modern features, and refactor a large, established codebase. It's a journey of learning and experimentation.

**To Avoid Confusion**: To respect the official FirebirdSQL project and its community, a distinct name and branding are essential. ScratchBird is not Firebird. By using a different name, we ensure that users and testers don't confuse this experimental version with the stable, production-ready releases of Firebird.

## Key Features & Goals 🚀

### 📊 Advanced Datatype System (v0.5)
- **Range Types**: INT4RANGE, INT8RANGE, NUMRANGE, TSRANGE, TSTZRANGE, DATERANGE with PostgreSQL-compatible operators
- **Case-Insensitive Text**: CITEXT datatype with automatic lowercase indexing
- **Advanced Arrays**: Multi-dimensional arrays with slicing operations and PostgreSQL-style operators
- **Full-Text Search**: TSVECTOR and TSQUERY with text processing, ranking, and highlighting
- **Unsigned Integers**: USMALLINT, UINTEGER, UBIGINT, UINT128 datatypes with complete arithmetic support *(COMPLETED)*
- **Enhanced VARCHAR**: 128KB UTF-8 support (vs 32KB limit) *(COMPLETED)*
- **Network Types**: INET, CIDR, MACADDR datatypes with PostgreSQL-compatible functions *(COMPLETED)*

### 🌳 Hierarchical Schema System (COMPLETED)
- **8-level deep schema nesting**: `finance.accounting.reports.table`
- **PostgreSQL-style qualified names**: Exceeds PostgreSQL's flat schema capabilities
- **Schema-aware database links**: 5 resolution modes for distributed database scenarios
- **Administrative functions**: Complete management and validation system

### 🔤 PascalCase Object Identifiers (COMPLETED)
- **Database-level case sensitivity control**: SQL Server-style identifier behavior
- **CREATE DATABASE syntax**: `PASCAL CASE IDENTIFIERS` clause for case-insensitive mode
- **Runtime detection**: `CURRENT_PASCAL_CASE_MODE` system context variable
- **Flexible identifier matching**: Choose between case-sensitive (default) or case-insensitive modes

### 🎯 SQL Dialect 4 Enhancements (COMPLETED)
- **FROM-less SELECT statements**: `SELECT GEN_UUID();`
- **Multi-row INSERT VALUES**: `INSERT INTO table VALUES (1,2,3),(4,5,6);`
- **UUID IDENTITY columns**: PostgreSQL-style automatic UUID generation with `GENERATED AS IDENTITY (GENERATOR GEN_UUID(version))`
- **Comprehensive SYNONYM support**: Schema-aware object aliasing

### 🔧 Modernized Tooling (COMPLETED)
- **Prefixed tools**: All command-line tools renamed with `sb_` prefix to prevent conflicts
- **Enhanced ISQL**: Schema commands, hierarchical navigation
- **Updated GBAK**: Schema-aware backup and restore
- **Complete tool suite**: `sb_isql`, `sb_gbak`, `sb_gfix`, `sb_gsec`, `sb_gstat`, etc.

### 🔐 Enhanced Security Features (COMPLETED)
- **Trusted authentication**: GSSAPI/Kerberos integration
- **Advanced authentication plugins**: Multi-factor authentication support
- **Schema-level security**: Granular access control for hierarchical schemas

## Getting Started (Build Instructions)

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake libedit-dev libicu-dev

# CentOS/RHEL/Fedora
sudo yum install gcc-c++ cmake libedit-devel libicu-devel
```

### Build Process
```bash
# Clone the repository
git clone https://github.com/dcalford/ScratchBird.git
cd ScratchBird

# Configure the build
./configure --enable-superserver --with-system-editline

# Compile (parallel build recommended)
make TARGET=Release -j$(nproc)

# Test the build
./verify_build.sh
```

### Quick Usage Examples
```bash
# Start interactive shell
./gen/Release/scratchbird/bin/sb_isql

# Create database with PascalCase identifiers (case-insensitive)
CREATE DATABASE "myapp.fdb" PASCAL CASE IDENTIFIERS;

# Create hierarchical schema (SQL Dialect 4)
CREATE SCHEMA finance;
CREATE SCHEMA finance.accounting;
CREATE SCHEMA finance.accounting.reports;

# Use new datatypes including unsigned integers, network types, and UUID identity
CREATE TABLE finance.accounting.reports.monthly_data (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    user_id UINTEGER,
    server_ip INET,
    network_block CIDR,
    mac_address MACADDR,
    date_range DATERANGE,
    description CITEXT,
    tags INT4RANGE,
    search_data TSVECTOR,
    big_number UBIGINT,
    huge_number UINT128
);

# PascalCase mode: these are all equivalent when enabled
CREATE TABLE MyTable (ID INTEGER, ColumnName VARCHAR(50));
INSERT INTO mytable (id, columnname) VALUES (1, 'test');
SELECT ID, COLUMNNAME FROM MYTABLE;

# Check PascalCase mode status
SELECT CURRENT_PASCAL_CASE_MODE FROM RDB$DATABASE;

# FROM-less SELECT statements
SELECT GEN_UUID();
SELECT CURRENT_TIMESTAMP;

# Multi-row INSERT with unsigned integers
INSERT INTO mytable(id, name) VALUES (1,'Alice'), (2,'Bob'), (3,'Carol');

# Unsigned integer operations
CREATE TABLE demo (
    small_id USMALLINT,     -- 16-bit: 0 to 65,535
    regular_id UINTEGER,    -- 32-bit: 0 to 4,294,967,295
    big_id UBIGINT,         -- 64-bit: 0 to 18,446,744,073,709,551,615
    huge_id UINT128         -- 128-bit: 0 to 340,282,366,920,938,463,463,374,607,431,768,211,455
);

INSERT INTO demo VALUES (65535, 4294967295, 18446744073709551615, 340282366920938463463374607431768211455);

# Network address types with PostgreSQL-compatible functions
CREATE TABLE network_infrastructure (
    id INTEGER,
    server_ip INET,
    network_block CIDR,
    mac_address MACADDR
);

INSERT INTO network_infrastructure VALUES (
    1,
    '192.168.1.100',
    '192.168.1.0/24',
    '00:11:22:33:44:55'
);

# Use PostgreSQL-compatible network functions
SELECT 
    HOST(server_ip) as host_address,
    MASKLEN(network_block) as prefix_length,
    BROADCAST(network_block) as broadcast_addr,
    FAMILY(server_ip) as address_family
FROM network_infrastructure;

# Network comparisons and indexing
CREATE INDEX idx_server_ip ON network_infrastructure (server_ip);
SELECT * FROM network_infrastructure ORDER BY server_ip, network_block;

# UUID IDENTITY columns for automatic UUID generation
CREATE TABLE users (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    name VARCHAR(50),
    email VARCHAR(100)
);

# Insert without specifying UUID - automatically generated
INSERT INTO users (name, email) VALUES ('John Doe', 'john@example.com');
INSERT INTO users (name, email) VALUES ('Jane Smith', 'jane@example.com');

# Alternative UUID versions and GENERATED BY DEFAULT
CREATE TABLE sessions (
    session_id UUID GENERATED BY DEFAULT AS IDENTITY (GENERATOR GEN_UUID(4)),
    user_id UUID,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

# UUID column stores as CHAR(16) CHARACTER SET OCTETS
SELECT CHAR_TO_UUID(id) as readable_uuid, name FROM users;
```

## Current Development Status

### ✅ Completed Features (v0.5)
- **Core Infrastructure**: Hierarchical schemas, SQL Dialect 4, tool modernization
- **Range Types**: Complete PostgreSQL-compatible range system with 6 range types
- **CITEXT**: Case-insensitive text with indexing support
- **Advanced Arrays**: Multi-dimensional arrays with slicing operations
- **Full-Text Search**: TSVECTOR/TSQUERY with text processing capabilities
- **Unsigned Integers**: Complete USMALLINT, UINTEGER, UBIGINT, UINT128 with arithmetic operations
- **Enhanced VARCHAR**: 128KB UTF-8 support (4x larger than standard Firebird)
- **Database Links**: Schema-aware distributed database connectivity
- **PascalCase Identifiers**: SQL Server-style case-insensitive object identifier support
- **Network Types**: Complete INET, CIDR, MACADDR with PostgreSQL-compatible functions
- **UUID IDENTITY**: PostgreSQL-style automatic UUID generation with versioning support *(NEW)*
- **Build System**: Complete ScratchBird v0.5 toolchain compilation and branding

### 🔄 Current Status (v0.5)
- **Implementation Phase**: Major features complete, entering organizational improvements
- **Build Status**: All core tools successfully compiled and functional
- **Testing**: UUID IDENTITY and hierarchical schema systems validated
- **Documentation**: Comprehensive technical documentation and usage examples complete

### 🎯 Next Development Phase (v0.6 Planning)
- **Hierarchical Schema Reorganization**: Enterprise-grade schema organization system
  - `SYSTEM.INFORMATION_SCHEMA`: SQL standard compliant metadata views
  - `DATABASE.MONITORING`: Enhanced monitoring and diagnostic capabilities  
  - `DATABASE.PROGRAMMING`: Common procedures, functions, and packages
  - `USERS.*`: Optional user-specific schema creation
  - `LINKS`: Centralized database link management
- **SQL Standards Compliance**: Full INFORMATION_SCHEMA implementation
- **Performance Optimizations**: Enhanced indexing for specialized datatypes
- **JSON Functions**: Advanced JSON manipulation and indexing capabilities

## Relationship to FirebirdSQL

ScratchBird would not be possible without the decades of incredible work done by the FirebirdSQL team. This project is a direct fork of their work and gratefully retains the original Mozilla Public License (MPL) and InterBase Public License (IDPL).

**Key Acknowledgments**:
- Based on Firebird 6.0.0.929 (f90eae0)
- Preserves all original licensing and attribution
- Maintains compatibility with Firebird's proven architecture
- Extends rather than replaces core functionality

## Development & Methodology

This project leverages modern AI assistants, including Anthropic's Claude, to accelerate development. Their role is primarily focused on performing systematic work, such as:

- **Code Generation**: Creating comprehensive datatype implementations following established patterns
- **Compilation Issues**: Resolving build errors and template specialization challenges
- **Research & Documentation**: Analyzing PostgreSQL specifications for compatibility features
- **Testing & Validation**: Identifying edge cases and integration issues

The core architectural decisions, feature design, and project direction for ScratchBird are human-led. AI is used as a sophisticated tool to handle systematic implementation tasks, allowing development to focus on high-level database engineering challenges.

**For full transparency**: AI tools are an integral part of this project's development workflow. We respect all viewpoints on this topic, but this is a core aspect of our methodology.

## Documentation

- [Advanced Datatype System](doc/README.advanced_datatypes.md) *(planned)*
- [Hierarchical Schemas Guide](doc/README.hierarchical_schemas.md)
- [Hierarchical Schema Reorganization Plan](doc/HIERARCHICAL_SCHEMA_REORGANIZATION_PLAN.md) *(NEW)*
- [SQL Dialect 4 Reference](doc/README.sql_dialect_4.md)
- [PascalCase Identifiers Guide](doc/README.pascalcase_identifiers.md) *(planned)*
- [Build Instructions](doc/README.build.posix.html)
- [Migration Guide](MIGRATION_GUIDE.md)

## PascalCase Object Identifiers Technical Details

### Overview
ScratchBird v0.5 introduces SQL Server-style case-insensitive object identifier support, controllable at the database level. This feature allows developers to choose between traditional case-sensitive Firebird behavior or case-insensitive SQL Server-like behavior.

### Usage
```sql
-- Create database with case-insensitive identifiers
CREATE DATABASE "application.fdb" PASCAL CASE IDENTIFIERS;

-- Now these are all equivalent:
CREATE TABLE MyTable (ColumnName VARCHAR(50), ID INTEGER);
INSERT INTO mytable (columnname, id) VALUES ('test', 1);
SELECT COLUMNNAME, id FROM MYTABLE;

-- Runtime detection
SELECT CURRENT_PASCAL_CASE_MODE FROM RDB$DATABASE;  -- Returns TRUE
```

### Implementation Architecture

**Database Header Flag**
- Stored as `hdr_pascal_case_identifiers = 0x80` in database header (ODS level)
- Runtime flag: `DBB_pascal_case_identifiers = 0x200L` in Database class
- Persistent across database sessions and restarts

**Identifier Resolution Engine**
- Modified `MetaName::compare()` method with conditional logic
- Case-sensitive mode (default): Uses fast `memcmp()` comparison
- Case-insensitive mode: Uses `strncasecmp()` for SQL Server behavior
- Consistent across all database objects: tables, columns, procedures, functions

**System Integration**
- `CURRENT_PASCAL_CASE_MODE` context variable for runtime detection
- Integrated with Dictionary-based identifier caching system
- Minimal performance overhead with conditional branching

## Network Address Types Technical Details

### Overview
ScratchBird v0.5 introduces complete PostgreSQL-compatible network address types, enabling storage and manipulation of IPv4, IPv6, network blocks, and MAC addresses with full indexing and sorting support.

### Supported Network Types

**INET Type**
- IPv4 and IPv6 addresses: `'192.168.1.1'`, `'2001:db8::1'`
- Storage format: 1 byte family + 16 bytes address (17 bytes total)
- Supports both IPv4 (4-byte) and IPv6 (16-byte) addresses
- Automatic validation and conversion from text

**CIDR Type**  
- Network blocks with prefix notation: `'192.168.1.0/24'`, `'2001:db8::/32'`
- Storage format: 1 byte family + 16 bytes address + 1 byte prefix (18 bytes total)
- Supports subnet calculations and network operations
- Compatible with both IPv4 and IPv6 networks

**MACADDR Type**
- MAC addresses: `'00:11:22:33:44:55'`, `'00-11-22-33-44-55'`
- Storage format: 6 bytes (48 bits)
- Standard format validation and normalization
- Supports various input formats (colon, hyphen, or no separators)

### PostgreSQL-Compatible Functions

**Address Functions**
```sql
HOST(inet)          -- Extract host address from INET/CIDR
MASKLEN(cidr)       -- Get prefix length from CIDR
NETMASK(cidr)       -- Get network mask from CIDR  
BROADCAST(cidr)     -- Get broadcast address from CIDR
ABBREV(inet)        -- Abbreviated form of network address
FAMILY(inet)        -- Address family (4 for IPv4, 6 for IPv6)
```

**Comparison Functions**
```sql
INET_SAME_FAMILY(inet1, inet2)  -- Check if same address family
INET_MERGE(inet1, inet2)        -- Merge network addresses
```

### Usage Examples

**Basic Operations**
```sql
-- Create table with network types
CREATE TABLE servers (
    id INTEGER,
    ip_address INET,
    network CIDR,
    mac MACADDR
);

-- Insert various network formats
INSERT INTO servers VALUES 
(1, '192.168.1.100', '192.168.1.0/24', '00:11:22:33:44:55'),
(2, '2001:db8::1', '2001:db8::/32', '00-AA-BB-CC-DD-EE'),
(3, '10.0.0.1', '10.0.0.0/8', '001122334455');
```

**Function Usage**
```sql
-- Extract network information
SELECT 
    ip_address,
    HOST(ip_address) as host_only,
    FAMILY(ip_address) as ip_version,
    MASKLEN(network) as prefix_length,
    BROADCAST(network) as broadcast_addr
FROM servers;

-- Network comparisons
SELECT * FROM servers 
WHERE INET_SAME_FAMILY(ip_address, '192.168.1.1'::INET);

-- Sorting and indexing
CREATE INDEX idx_ip ON servers (ip_address);
SELECT * FROM servers ORDER BY ip_address, network;
```

### Implementation Architecture

**Storage Engine Integration**
- Binary storage format optimized for comparison and indexing
- Integrated with ScratchBird's type system (dtype_inet=36, dtype_cidr=37, dtype_macaddr=38)
- Full sorting and comparison support with binary tree indexing

**Conversion System**
- Complete text ↔ binary conversion infrastructure
- Automatic validation with comprehensive error handling
- Support for multiple input formats (standard and alternative notations)

**Database Engine Integration**
- Integrated with CVT (conversion) system for type coercion
- Full comparison and sorting support in CVT2 comparison engine
- Sort key descriptors (SKD) for optimal query performance
- Compatible with database links and distributed queries

## UUID IDENTITY Columns Technical Details

### Overview
ScratchBird v0.5 introduces PostgreSQL-style UUID IDENTITY columns, enabling automatic UUID generation with version control. This feature extends ScratchBird's existing IDENTITY column system to support UUID types with comprehensive versioning support.

### Supported Syntax

**Standard IDENTITY Syntax**
```sql
-- GENERATED ALWAYS - cannot override during INSERT
CREATE TABLE users (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    name VARCHAR(50)
);

-- GENERATED BY DEFAULT - can override with explicit values
CREATE TABLE sessions (
    session_id UUID GENERATED BY DEFAULT AS IDENTITY (GENERATOR GEN_UUID(4)),
    data TEXT
);
```

**UUID Version Support**
- **GEN_UUID()**: Default UUID generation (no version specified)
- **GEN_UUID(1)**: Time-based UUID
- **GEN_UUID(4)**: Random UUID (default)
- **GEN_UUID(6)**: Time-ordered UUID  
- **GEN_UUID(7)**: Unix time-based UUID (recommended for databases)
- **GEN_UUID(8)**: Custom UUID format

### Usage Examples

**Basic UUID IDENTITY Tables**
```sql
-- Create table with UUID primary key
CREATE TABLE customers (
    customer_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    name VARCHAR(100),
    email VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert records without specifying UUID - automatically generated
INSERT INTO customers (name, email) VALUES 
('Alice Johnson', 'alice@example.com'),
('Bob Wilson', 'bob@example.com');

-- Query with readable UUID conversion
SELECT 
    CHAR_TO_UUID(customer_id) as readable_id,
    name,
    email
FROM customers;
```

**Advanced UUID Operations**
```sql
-- Create related tables with UUID foreign keys
CREATE TABLE orders (
    order_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    customer_id UUID NOT NULL,
    order_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    total_amount DECIMAL(10,2)
);

-- UUID-based relationships
ALTER TABLE orders ADD CONSTRAINT fk_customer 
    FOREIGN KEY (customer_id) REFERENCES customers(customer_id);

-- Efficient UUID indexing
CREATE INDEX idx_orders_customer ON orders(customer_id);
CREATE INDEX idx_orders_date ON orders(order_date);
```

### Implementation Architecture

**Database Schema Integration**
- New `RDB$UUID_GENERATOR` field in `RDB$RELATION_FIELDS` system table
- UUID generator metadata stored alongside traditional identity column information
- Full integration with existing DDL processing and metadata system

**Storage Format**
- UUID columns must be defined as `CHAR(16) CHARACTER SET OCTETS`
- Binary storage format for optimal performance and indexing
- Compatible with existing UUID functions: `GEN_UUID()`, `CHAR_TO_UUID()`, `UUID_TO_CHAR()`

**Runtime Generation**
- Automatic UUID generation during INSERT operations
- Leverages ScratchBird's existing `GEN_UUID()` function infrastructure
- Support for all UUID versions (1, 4, 6, 7, 8) with parameter validation
- No sequence generators required - function-based generation

**Performance Characteristics**
- Binary UUID storage for efficient indexing and comparison
- Time-ordered UUIDs (version 7) provide optimal B-tree performance
- No sequence bottlenecks - each INSERT generates independent UUID
- Compatible with distributed database scenarios

### Comparison with Traditional IDENTITY

**Traditional Sequence-Based IDENTITY**
```sql
CREATE TABLE orders_old (
    id INTEGER GENERATED ALWAYS AS IDENTITY,  -- Uses sequence generator
    order_number VARCHAR(20)
);
```

**UUID IDENTITY Advantages**
```sql
CREATE TABLE orders_new (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),  -- Function-based
    order_number VARCHAR(20)
);
```

**Key Differences**:
- **Global Uniqueness**: UUIDs are globally unique across databases and systems
- **No Sequence Conflicts**: Function-based generation eliminates sequence bottlenecks
- **Distributed Friendly**: Ideal for database links and replication scenarios
- **Time Ordering**: Version 7 UUIDs provide time-based ordering for optimal performance
- **Collision Resistance**: Cryptographically strong uniqueness guarantees

### Integration with Existing Features

**Hierarchical Schema Support**
```sql
-- UUID IDENTITY works with hierarchical schemas
CREATE SCHEMA company.hr.employees;

CREATE TABLE company.hr.employees.staff (
    employee_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    name VARCHAR(100),
    hire_date DATE
);
```

**Database Links Compatibility**
```sql
-- UUID IDENTITY columns work across database links
SELECT e.employee_id, e.name, d.department_name
FROM company.hr.employees.staff e
JOIN departments@remote_db d ON e.dept_id = d.dept_id;
```

## Contributing

We welcome contributions that align with ScratchBird's experimental direction! Please note:

- **Focus Areas**: Advanced datatypes, PostgreSQL compatibility, performance optimizations
- **Code Style**: Follow established ScratchBird patterns (see examples in `/src/common/`)
- **Testing**: All new features should include comprehensive test cases
- **Documentation**: Update relevant documentation for user-facing changes

## ⚠️ Disclaimer

**Please be aware that ScratchBird is an experimental project and is not recommended for production environments.** It is intended for learning, testing, and development purposes. Use at your own risk.

**Key Considerations**:
- **Alpha Software**: Expect breaking changes and instability
- **No Production Support**: This is a research and learning project
- **Experimental Features**: New datatypes and features are still under development
- **Backup Strategy**: Always maintain backups when testing database features

## License

ScratchBird is released under the Initial Developer's Public License (IDPL), maintaining compatibility with the original Firebird licensing.