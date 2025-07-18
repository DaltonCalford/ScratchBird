# ScratchBird v0.5.0 Release Notes
*Release Date: January 17, 2025*

## 🎉 Production Ready Release

ScratchBird v0.5.0 represents the first production-ready release of our PostgreSQL-compatible fork of FirebirdSQL. This release includes a complete, tested, and working implementation of hierarchical schemas, advanced data types, and a comprehensive database toolkit.

## ✅ What's Included

### Core Database Tools (All Working)
- **sb_isql** - Interactive SQL utility (version SB-T0.5.0.1)
- **sb_gbak** - Backup and restore utility
- **sb_gfix** - Database maintenance tool
- **sb_gsec** - Security management
- **sb_gstat** - Database statistics
- **sb_guard** - Process monitor
- **sb_svcmgr** - Service manager
- **sb_tracemgr** - Database tracing
- **sb_nbackup** - Incremental backup
- **sb_gssplit** - File splitting utility
- **sb_lock_print** - Lock analysis

### Libraries and Development
- **libsbclient.so** - Client library for applications
- **Development headers** - C/C++ integration headers
- **Example code** - Usage examples and integration samples

### Comprehensive Test Suite
- **8 Test Categories** - Complete validation of all features
- **Performance Benchmarks** - Operational speed validation
- **Automated Testing** - sb_isql-based test execution

## 🚀 Key Features

### Hierarchical Schema System
```sql
-- Create nested schemas (exceeds PostgreSQL capabilities)
CREATE SCHEMA company.division.department.team.project.environment.component.module;

-- Schema context switching
SET SCHEMA company.division.department;
SELECT * FROM team.project_table;
```

### PostgreSQL-Compatible Data Types
```sql
-- Network data types
CREATE TABLE servers (
    server_ip INET,
    network CIDR, 
    mac_address MACADDR
);

-- UUID with automatic generation
CREATE TABLE users (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    username VARCHAR(100)
);

-- Unsigned integers
CREATE TABLE counters (
    small_count USMALLINT,
    large_count UBIGINT,
    huge_count UINT128
);
```

### Schema-Aware Database Links
```sql
-- Create distributed database connections
CREATE DATABASE LINK finance_link 
  TO 'remote_server:finance_db' 
  SCHEMA_MODE HIERARCHICAL
  LOCAL_SCHEMA 'finance'
  REMOTE_SCHEMA 'accounting';

-- Use remote data with schema mapping
SELECT * FROM employees@finance_link;
```

## 📋 Installation Requirements

### System Requirements
- **Linux**: x86_64 architecture
- **RAM**: Minimum 512MB, recommended 2GB+
- **Disk**: 200MB for installation, additional space for databases
- **Network**: Port 4050 available (ScratchBird default)

### Dependencies
- **glibc**: 2.17 or later
- **libstdc++**: C++17 support
- **libm**: Math library

## 🔧 Quick Installation

### Option 1: Simple Installation
```bash
# Extract release package
tar -xzf scratchbird-v0.5.0-linux-x86_64.tar.gz
cd scratchbird-v0.5.0-linux-x86_64

# Run installer (creates /opt/scratchbird)
sudo ./install.sh

# Add to PATH
echo 'export PATH="/opt/scratchbird/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Verify installation
sb_isql -z
```

### Option 2: Custom Installation
```bash
# Manual installation to custom directory
mkdir -p /your/custom/path/scratchbird
cp -r bin lib include /your/custom/path/scratchbird/

# Set environment variables
export SCRATCHBIRD_HOME="/your/custom/path/scratchbird"
export PATH="$SCRATCHBIRD_HOME/bin:$PATH"
export LD_LIBRARY_PATH="$SCRATCHBIRD_HOME/lib:$LD_LIBRARY_PATH"
```

## 🧪 Testing Your Installation

### Basic Functionality Test
```bash
# Test tool versions
sb_isql -z
sb_gbak -z
sb_gstat -z

# Create test database
sb_isql -user SYSDBA -password masterkey
SQL> CREATE DATABASE 'test.fdb';
SQL> QUIT;

# Test hierarchical schemas
sb_isql -user SYSDBA -password masterkey test.fdb
SQL> CREATE SCHEMA company.hr.employees;
SQL> CREATE TABLE company.hr.employees.staff (id INTEGER, name VARCHAR(100));
SQL> INSERT INTO company.hr.employees.staff VALUES (1, 'Alice');
SQL> SELECT * FROM company.hr.employees.staff;
SQL> QUIT;
```

### Run Comprehensive Test Suite
```bash
# Navigate to tests directory
cd tests

# Run all tests (requires database creation privileges)
chmod +x run_all_tests.sh
./run_all_tests.sh

# View test results
cat test_results.txt
```

## 🚀 Getting Started

### Create Your First Hierarchical Database
```sql
-- Connect and create database
sb_isql -user SYSDBA -password masterkey

-- Create hierarchical schema structure
CREATE DATABASE 'myapp.fdb';
CREATE SCHEMA app;
CREATE SCHEMA app.users;
CREATE SCHEMA app.products;
CREATE SCHEMA app.orders;

-- Create tables with PostgreSQL-compatible types
CREATE TABLE app.users.accounts (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    username VARCHAR(100) NOT NULL,
    email VARCHAR(200) UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE app.products.inventory (
    id INTEGER PRIMARY KEY,
    name VARCHAR(200),
    price DECIMAL(10,2),
    stock_count UINTEGER,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Use schema context switching
SET SCHEMA app.users;
INSERT INTO accounts (username, email) VALUES ('alice', 'alice@example.com');

SET SCHEMA app.products;
INSERT INTO inventory (id, name, price, stock_count) VALUES (1, 'Widget', 19.99, 100);

-- Query across schemas
SELECT 
    u.username,
    p.name as product_name,
    p.price
FROM app.users.accounts u
CROSS JOIN app.products.inventory p
WHERE p.id = 1;
```

## 🔍 Troubleshooting

### Common Issues

**Port Conflict (3050 already in use)**
- ScratchBird uses port 4050 by default to avoid Firebird conflicts
- Check `firebird.conf` if you need to change the port

**Permission Denied**
- Ensure user has write access to database directory
- Run with appropriate privileges for system installation

**Library Not Found**
- Set `LD_LIBRARY_PATH` to include ScratchBird lib directory
- Verify libsbclient.so is in the library path

**Database Creation Fails**
- Check SYSDBA password (default: masterkey)
- Ensure database directory exists and is writable
- Verify no firewall blocking port 4050

### Getting Help

**Test Suite Issues**
- Run individual test scripts to isolate problems
- Check test_results.txt for detailed error information
- Ensure sufficient privileges for database creation

**Performance Issues**
- Review performance benchmark results in test suite
- Monitor with sb_gstat for database statistics
- Use sb_tracemgr for detailed performance analysis

## 📞 Support & Resources

- **GitHub Issues**: https://github.com/dcalford/ScratchBird/issues
- **Documentation**: See `doc/` directory for complete documentation
- **Examples**: Check `examples/` directory for integration samples
- **Test Suite**: Use `tests/` directory for comprehensive validation

## ⚠️ Important Notes

- **Backup Strategy**: Always maintain database backups before major operations
- **Testing**: Thoroughly test in development before production use
- **Compatibility**: Some features require SQL Dialect 4
- **Performance**: Monitor performance with large datasets using advanced data types

## 🎯 What's Next

ScratchBird v0.6.0 development is already underway with enhanced array types, full-text search, and spatial data support. See our roadmap for detailed plans.

**Thank you for trying ScratchBird v0.5.0!**

---
*ScratchBird Development Team - January 17, 2025*