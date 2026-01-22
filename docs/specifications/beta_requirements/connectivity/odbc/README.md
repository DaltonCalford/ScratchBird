# ODBC Driver Specification

**Priority:** P0 (Critical - Beta Required)
**Status:** Draft
**Target Market:** Business intelligence tools, enterprise reporting, Microsoft ecosystem
**Use Cases:** Tableau, Power BI, Excel, Access, SSRS, Crystal Reports, QlikView, general ODBC applications

---

## Overview

ODBC (Open Database Connectivity) is the industry-standard database access method for Windows and cross-platform applications. ODBC support is absolutely critical for ScratchBird adoption in business intelligence, enterprise reporting, and Microsoft Office integration. ScratchBird uses its native ODBC driver, which connects to the ScratchBird network listener (default port 3092) using the ScratchBird wire protocol.

**Scope Note:** SQL Server ODBC comparisons and SSRS references are post-gold; MSSQL/TDS emulation is not part of current scope.

**Key Requirements:**
- ODBC 3.8 specification compliance
- Windows and Linux support (unixODBC)
- 32-bit and 64-bit drivers
- Unicode support (UTF-8, UTF-16)
- SQL-92 and SQL-99 compliance
- Cursor library support
- Connection pooling
- Native ScratchBird wire protocol via network listener (no direct engine access)
- Installer packages (MSI for Windows, packages for Linux)
- Signed drivers for Windows

---

## Specification Documents

### Required Documents

- [x] **[SPECIFICATION.md](SPECIFICATION.md)** - Detailed technical specification (draft)
  - ODBC 3.8 API compliance matrix
  - Function implementation (SQLConnect, SQLPrepare, SQLExecute, etc.)
  - Type mappings (ODBC SQL types ↔ ScratchBird types)
  - Connection string format
  - Error handling and diagnostics
  - Transaction support
  - Cursor types and scrollability
  - Metadata functions (SQLTables, SQLColumns, etc.)

- [ ] **IMPLEMENTATION_PLAN.md** - Development roadmap
  - Milestone 1: Core ODBC 3.8 functions (Connect, Execute, Fetch)
  - Milestone 2: Advanced features (Prepared statements, transactions)
  - Milestone 3: Metadata and catalog functions
  - Milestone 4: Windows installer and DSN configuration UI
  - Milestone 5: BI tool certification (Tableau, Power BI, Excel)
  - Resource requirements
  - Timeline estimates

- [ ] **TESTING_CRITERIA.md** - Test requirements
  - ODBC compliance test suite
  - Microsoft ODBC Test (odbcte32)
  - unixODBC test suite
  - Tableau connectivity tests
  - Power BI connectivity tests
  - Excel connectivity tests
  - Performance benchmarks vs PostgreSQL ODBC
  - Concurrent connection tests

- [ ] **COMPATIBILITY_MATRIX.md** - Version support
  - ODBC versions (3.5, 3.8)
  - Windows versions (10, 11, Server 2016+)
  - Linux distributions (Ubuntu, RHEL, Debian, etc.)
  - BI tools (Tableau 2021+, Power BI Desktop, Excel 2016+)
  - Reporting tools (SSRS, Crystal Reports, JasperReports)
  - Driver managers (Windows ODBC, unixODBC, iODBC)

- [ ] **MIGRATION_GUIDE.md** - Migration from other ODBC drivers
  - From PostgreSQL ODBC (psqlODBC)
  - From MySQL ODBC (Connector/ODBC)
  - From SQL Server ODBC (post-gold reference)
  - From Firebird ODBC (OdbcFb)
  - DSN migration
  - Connection string migration
  - Application compatibility notes

- [ ] **API_REFERENCE.md** - Complete API documentation
  - ODBC function reference
  - Connection attributes
  - Statement attributes
  - Error codes and SQLSTATE values
  - Driver-specific extensions

### Examples Directory

- [ ] **examples/basic_connection.c** - Simple C/C++ ODBC connection
- [ ] **examples/prepared_statement.c** - Parameterized queries
- [ ] **examples/transactions.c** - Transaction management
- [ ] **examples/metadata.c** - Catalog functions
- [ ] **examples/excel/** - Excel connection examples
  - **connection_steps.txt** - Step-by-step Excel connection
  - **sample_queries.txt** - Example queries
  - **screenshots/** - UI screenshots
- [ ] **examples/power-bi/** - Power BI Desktop integration
  - **connection_guide.md** - Connection setup
  - **sample_report.pbix** - Sample Power BI report
- [ ] **examples/tableau/** - Tableau integration
  - **connection_guide.md** - Tableau connection setup
  - **sample_workbook.twb** - Sample Tableau workbook
- [ ] **examples/python-pyodbc/** - Python pyodbc usage
- [ ] **examples/dotnet-odbc/** - C# System.Data.Odbc usage
- [ ] **examples/dsn-configuration/** - DSN setup examples
  - **windows_dsn.md** - Windows DSN configuration
  - **linux_dsn.md** - unixODBC configuration

---

## Key Integration Points

### Microsoft Excel Integration

**Critical**: Excel is ubiquitous in business environments.

Requirements:
- Excel Data Connection (.odc files)
- Microsoft Query support
- PivotTable data source
- Power Query integration
- Refresh capabilities

Example Excel connection:
```
1. Open Excel
2. Data tab → Get Data → From Other Sources → From ODBC
3. Select ScratchBird DSN or enter connection string:
   DSN=ScratchBird;UID=myuser;PWD=mypassword;Database=mydb
4. Select tables or enter custom SQL
5. Load data to worksheet or PivotTable
```

### Power BI Desktop Integration

**Critical**: Power BI is Microsoft's leading BI platform.

Requirements:
- Power BI Desktop connector (via ODBC)
- DirectQuery support (live connection)
- Import mode support
- Query folding optimization
- Incremental refresh support

Example Power BI connection:
```
1. Open Power BI Desktop
2. Get Data → ODBC
3. Enter Data Source Name (DSN) or connection string:
   Driver={ScratchBird ODBC Driver};Server=localhost;Port=3092;Database=mydb;UID=user;PWD=pass
4. Choose DirectQuery or Import mode
5. Select tables or write DAX queries
```

### Tableau Integration

**Critical**: Tableau is a leading data visualization platform.

Requirements:
- Tableau connector (Generic ODBC or custom connector)
- Live connection support
- Extract optimization
- Custom SQL support
- Tableau Server compatibility

Example Tableau connection:
```
1. Open Tableau Desktop
2. Connect → To a Server → Other Databases (ODBC)
3. Select DSN: ScratchBird
   Or enter connection string
4. Choose Live or Extract
5. Drag tables to canvas or write custom SQL
```

### Windows DSN Configuration

Requirements:
- ODBC Data Source Administrator integration
- User DSN and System DSN support
- GUI configuration dialog
- Connection testing
- Advanced options (SSL, timeout, etc.)

Example Windows DSN:
```
1. Open ODBC Data Source Administrator (odbcad32.exe)
   - 32-bit: C:\Windows\SysWOW64\odbcad32.exe
   - 64-bit: C:\Windows\System32\odbcad32.exe
2. User DSN or System DSN tab → Add
3. Select "ScratchBird ODBC Driver"
4. Configure:
   Data Source Name: ScratchBird_Production
   Description: Production ScratchBird Database
   Server: db.example.com
   Port: 3092
   Database: production
   Username: readonly_user
   SSL Mode: require
5. Test Connection
6. OK to save
```

### Linux unixODBC Configuration

Requirements:
- unixODBC driver registration
- odbcinst.ini configuration
- odbc.ini DSN configuration
- isql command-line testing

Example unixODBC configuration:

**/etc/odbcinst.ini:**
```ini
[ScratchBird]
Description = ScratchBird ODBC Driver
Driver = /usr/lib/x86_64-linux-gnu/odbc/libscratchbird_odbc.so
Setup = /usr/lib/x86_64-linux-gnu/odbc/libscratchbird_odbc.so
UsageCount = 1
Threading = 2
```

**/etc/odbc.ini or ~/.odbc.ini:**
```ini
[ScratchBird_Production]
Description = Production ScratchBird Database
Driver = ScratchBird
Server = db.example.com
Port = 3092
Database = production
UID = myuser
PWD = mypassword
SSLMode = require
ReadOnly = 0
AutoCommit = 1
```

Test connection:
```bash
isql -v ScratchBird_Production
```

---

## ODBC API Implementation

### Core ODBC Functions

**Connection Functions:**
- `SQLAllocHandle` - Allocate handles (environment, connection, statement)
- `SQLConnect` - Connect using DSN
- `SQLDriverConnect` - Connect using connection string
- `SQLBrowseConnect` - Interactive connection building
- `SQLDisconnect` - Disconnect from database
- `SQLFreeHandle` - Free handles

**Statement Execution:**
- `SQLPrepare` - Prepare SQL statement
- `SQLExecute` - Execute prepared statement
- `SQLExecDirect` - Prepare and execute in one call
- `SQLNumResultCols` - Number of columns in result set
- `SQLRowCount` - Number of rows affected

**Data Retrieval:**
- `SQLFetch` - Fetch next row
- `SQLFetchScroll` - Fetch with scrolling (FORWARD, BACKWARD, ABSOLUTE, RELATIVE)
- `SQLGetData` - Retrieve column data
- `SQLBindCol` - Bind column to application buffer
- `SQLMoreResults` - Check for additional result sets

**Transaction Management:**
- `SQLEndTran` - Commit or rollback transaction
- `SQLSetConnectAttr` - Set auto-commit mode and isolation level

**Metadata Functions:**
- `SQLTables` - List tables in database
- `SQLColumns` - List columns in table
- `SQLPrimaryKeys` - List primary key columns
- `SQLForeignKeys` - List foreign key relationships
- `SQLStatistics` - List table indexes
- `SQLProcedures` - List stored procedures
- `SQLGetTypeInfo` - Get supported data types

**Diagnostic Functions:**
- `SQLGetDiagRec` - Retrieve diagnostic records (errors, warnings)
- `SQLGetDiagField` - Retrieve specific diagnostic field
- `SQLError` - Deprecated, but still supported for compatibility

### Connection String Format

```
// DSN-based connection
DSN=ScratchBird;UID=myuser;PWD=mypassword;

// DSN-less connection (full connection string)
Driver={ScratchBird ODBC Driver};Server=localhost;Port=3092;Database=mydb;UID=myuser;PWD=mypassword;

// Common parameters:
Driver       - ODBC driver name (DSN-less only; must match odbcinst entry)
DSN          - Data Source Name (DSN-based only)
Server       - Database host
Port         - Database port (default: 3092)
Database     - Database name
UID / User   - Username
PWD / Password - Password
SSLMode      - SSL mode (disable, allow, prefer, require, verify_ca, verify_full)
SSLCert      - Client certificate path
SSLKey       - Client key path
SSLRootCert  - CA certificate path
ReadOnly     - Read-only connection (0=no, 1=yes)
AutoCommit   - Autocommit (0=no, 1=yes)
Timeout      - Connection timeout in seconds
QueryTimeout - Query timeout in seconds
ApplicationName - Application name
Schema / CurrentSchema - Default schema
Charset / Encoding - Client encoding
PacketSize  - Packet size in bytes
Pooling     - Driver-manager pooling hint (0=no, 1=yes)

// Example connection strings:
Driver={ScratchBird ODBC Driver};Server=localhost;Port=3092;Database=mydb;UID=admin;PWD=secret;
Driver={ScratchBird ODBC Driver};Server=db.example.com;Port=3092;Database=production;UID=user;PWD=pass;SSLMode=require;ReadOnly=1;
```

### Type Mappings

| ODBC SQL Type | ScratchBird Type | C Type |
|---------------|------------------|--------|
| SQL_CHAR | CHAR | SQL_C_CHAR |
| SQL_VARCHAR | VARCHAR | SQL_C_CHAR |
| SQL_LONGVARCHAR | TEXT | SQL_C_CHAR |
| SQL_WCHAR | CHAR | SQL_C_WCHAR |
| SQL_WVARCHAR | VARCHAR | SQL_C_WCHAR |
| SQL_WLONGVARCHAR | TEXT | SQL_C_WCHAR |
| SQL_SMALLINT | SMALLINT | SQL_C_SHORT |
| SQL_INTEGER | INTEGER | SQL_C_LONG |
| SQL_BIGINT | BIGINT | SQL_C_SBIGINT |
| SQL_REAL | REAL | SQL_C_FLOAT |
| SQL_FLOAT | DOUBLE PRECISION | SQL_C_DOUBLE |
| SQL_DOUBLE | DOUBLE PRECISION | SQL_C_DOUBLE |
| SQL_DECIMAL | DECIMAL | SQL_C_CHAR |
| SQL_NUMERIC | NUMERIC | SQL_C_NUMERIC |
| SQL_BIT | BOOLEAN | SQL_C_BIT |
| SQL_BINARY | BYTEA | SQL_C_BINARY |
| SQL_VARBINARY | BYTEA | SQL_C_BINARY |
| SQL_LONGVARBINARY | BYTEA | SQL_C_BINARY |
| SQL_TYPE_DATE | DATE | SQL_C_TYPE_DATE |
| SQL_TYPE_TIME | TIME | SQL_C_TYPE_TIME |
| SQL_TYPE_TIMESTAMP | TIMESTAMP | SQL_C_TYPE_TIMESTAMP |
| SQL_GUID | UUID | SQL_C_GUID |

---

## Performance Targets

Benchmark against psqlODBC (PostgreSQL ODBC driver):

| Operation | Target Performance |
|-----------|-------------------|
| Connection establishment | Within 10% of psqlODBC |
| Simple SELECT (1 row) | Within 10% of psqlODBC |
| Bulk SELECT (10,000 rows) | Within 15% of psqlODBC |
| Prepared statement execute | Within 10% of psqlODBC |
| Bulk INSERT (1,000 rows) | Within 15% of psqlODBC |
| SQLTables metadata call | Within 15% of psqlODBC |
| Excel data refresh (1,000 rows) | Within 20% of MySQL ODBC |
| Power BI DirectQuery | Within 20% of SQL Server ODBC (post-gold reference) |

---

## Dependencies

### Windows Build Dependencies

- Visual Studio 2019 or later
- Windows SDK
- ODBC SDK (included in Windows SDK)
- WiX Toolset (for MSI installer)

### Linux Build Dependencies

- GCC or Clang compiler
- unixODBC development files (`unixodbc-dev`)
- OpenSSL development files
- CMake or Autotools

### Runtime Dependencies

- **Windows**: Windows ODBC Driver Manager (built into Windows)
- **Linux**: unixODBC or iODBC
- **macOS**: iODBC or unixODBC

---

## Installation

### Windows Installation

```
1. Download ScratchBirdODBC-1.0.0-win64.msi
2. Run installer (requires Administrator rights)
3. Follow installation wizard
4. Driver automatically registers with Windows ODBC
5. Configure DSN using ODBC Data Source Administrator
```

### Linux Installation (Debian/Ubuntu)

```bash
# Install from package
sudo dpkg -i scratchbird-odbc_1.0.0_amd64.deb

# Or install from repository
sudo apt-get install scratchbird-odbc

# Configure odbcinst.ini (usually automatic)
sudo odbcinst -i -d -f /etc/scratchbird-odbc/odbcinst.ini

# Create DSN in /etc/odbc.ini or ~/.odbc.ini
```

### Linux Installation (RHEL/Fedora)

```bash
# Install from package
sudo rpm -i scratchbird-odbc-1.0.0-1.x86_64.rpm

# Or install from repository
sudo dnf install scratchbird-odbc

# Configure
sudo odbcinst -i -d -f /etc/scratchbird-odbc/odbcinst.ini
```

---

## Documentation Requirements

### User Documentation

- [ ] Quick start guide (DSN configuration)
- [ ] Installation instructions (Windows, Linux, macOS)
- [ ] Connection string reference
- [ ] DSN configuration guide (GUI and command-line)
- [ ] Excel integration guide
- [ ] Power BI integration guide
- [ ] Tableau integration guide
- [ ] Troubleshooting guide (connection issues, performance)
- [ ] Performance tuning tips

### Administrator Documentation

- [ ] Deployment guide (enterprise)
- [ ] Silent installation (MSI command-line)
- [ ] Group Policy deployment (Windows)
- [ ] Security best practices
- [ ] SSL/TLS configuration
- [ ] Connection pooling configuration

### API Documentation

- [ ] Complete ODBC function reference
- [ ] Driver-specific extensions
- [ ] Error code reference
- [ ] Connection attribute reference
- [ ] Statement attribute reference

### Integration Guides

- [ ] Microsoft Excel integration
- [ ] Power BI Desktop integration
- [ ] Power BI Service integration (gateway)
- [ ] Tableau Desktop integration
- [ ] Tableau Server integration
- [ ] SQL Server Reporting Services (SSRS) (post-gold reference)
- [ ] Crystal Reports
- [ ] QlikView / Qlik Sense
- [ ] Python pyodbc usage
- [ ] C# System.Data.Odbc usage

---

## Release Criteria

### Functional Completeness

- [ ] ODBC 3.8 core functions implemented
- [ ] Connection and disconnection
- [ ] Statement preparation and execution
- [ ] Result set fetching (forward and scrollable)
- [ ] Transactions and auto-commit
- [ ] Metadata functions (SQLTables, SQLColumns, etc.)
- [ ] All ODBC SQL types supported
- [ ] Error handling and diagnostics
- [ ] SSL/TLS support

### Quality Metrics

- [ ] >90% ODBC compliance (ODBC Test Suite)
- [ ] Microsoft ODBC Test (odbcte32) passing
- [ ] Excel connection successful
- [ ] Power BI DirectQuery successful
- [ ] Tableau live connection successful
- [ ] Performance benchmarks met (within 10-20% of psqlODBC)
- [ ] Concurrent connection tests passing
- [ ] Memory leak tests passing

### Certification

- [ ] Microsoft WHQL signed driver (Windows)
- [ ] Tableau connector certification (if applicable)
- [ ] Power BI certification (if applicable)

### Documentation

- [ ] Complete user guide
- [ ] Administrator deployment guide
- [ ] 10+ integration examples
- [ ] Troubleshooting guide
- [ ] Migration guide from PostgreSQL/MySQL ODBC

### Packaging

- [ ] Windows MSI installer (32-bit and 64-bit)
- [ ] Signed Windows driver (.cat file)
- [ ] Linux DEB packages (Ubuntu, Debian)
- [ ] Linux RPM packages (RHEL, Fedora, CentOS)
- [ ] macOS package (Homebrew or DMG)
- [ ] Automated CI/CD for all platforms

---

## Community and Support

### Download Locations

- Windows Installer: https://scratchbird.dev/downloads/odbc-windows/
- Linux Packages: https://scratchbird.dev/downloads/odbc-linux/
- macOS Package: https://scratchbird.dev/downloads/odbc-macos/
- GitHub: https://github.com/scratchbird/scratchbird-odbc
- Documentation: https://scratchbird.dev/docs/odbc/

### Issue Tracking

- GitHub Issues: https://github.com/scratchbird/scratchbird-odbc/issues
- Response time target: <3 days
- Bug fix SLA: Critical <7 days, High <14 days

### Community Channels

- Discord: ScratchBird #odbc-driver
- Stack Overflow: Tag `scratchbird-odbc` or `scratchbird`
- Enterprise support: support@scratchbird.dev

---

## References

1. **ODBC 3.8 Specification**
   https://docs.microsoft.com/en-us/sql/odbc/reference/

2. **psqlODBC** (PostgreSQL ODBC driver - reference implementation)
   https://odbc.postgresql.org/

3. **unixODBC Documentation**
   http://www.unixodbc.org/

4. **Microsoft ODBC Documentation**
   https://docs.microsoft.com/en-us/sql/connect/odbc/

5. **Tableau ODBC Requirements**
   https://www.tableau.com/support/drivers

---

## Next Steps

1. **Expand SPECIFICATION.md** with complete ODBC 3.8 API mapping
2. **Develop IMPLEMENTATION_PLAN.md** with milestones and timeline
3. **Write TESTING_CRITERIA.md** with ODBC compliance test requirements
4. **Begin prototype** with core connection and query functions
5. **Windows installer development** (MSI with GUI configuration)
6. **BI tool certification** (Tableau, Power BI testing)

---

**Document Version:** 1.0 (Template)
**Last Updated:** 2026-01-03
**Status:** Draft
**Assigned To:** TBD
