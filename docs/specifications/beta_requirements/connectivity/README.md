# Standard Connectivity

**[← Back to Beta Requirements](../README.md)** | **[← Back to Specifications Index](../../README.md)**

This directory contains specifications for standard database connectivity protocols.

## Overview

ScratchBird provides full compatibility with industry-standard database connectivity protocols (ODBC and JDBC), enabling integration with thousands of existing applications, tools, and frameworks.

## Specifications

### ODBC (Open Database Connectivity)

**[odbc/](odbc/)** - ODBC Driver Specification
- ODBC 3.x compliance
- SQLAllocHandle, SQLConnect, SQLExecute, etc.
- Windows, Linux, macOS support
- Unicode support
- **Market Share:** Universal Windows standard, widely used on Linux/macOS
- Draft spec: [odbc/SPECIFICATION.md](odbc/SPECIFICATION.md)

### JDBC (Java Database Connectivity)

**[jdbc/](jdbc/)** - JDBC Driver Specification
- JDBC 4.2+ compliance
- Type 4 (pure Java) driver
- Connection pooling
- Prepared statements
- **Market Share:** Universal Java standard

## Features

### ODBC Driver

- **Full ODBC 3.x API** - Complete API implementation
- **Multi-platform** - Windows, Linux, macOS
- **Unicode support** - UTF-8/UTF-16 support
- **Connection pooling** - Built-in connection pooling
- **Transaction support** - Full transaction management
- **Metadata support** - Complete catalog metadata

### JDBC Driver

- **JDBC 4.2+** - Modern JDBC standard
- **Type 4 driver** - Pure Java, no native code
- **PreparedStatement** - Parameterized queries
- **CallableStatement** - Stored procedure calls
- **ResultSetMetaData** - Complete metadata support
- **Connection pooling** - Standard pooling support

## Testing Requirements

Both drivers must pass:

- **Connection tests** - Connect, disconnect, reconnect
- **Query tests** - SELECT, INSERT, UPDATE, DELETE
- **Transaction tests** - COMMIT, ROLLBACK, savepoints
- **Metadata tests** - Schema, table, column metadata
- **Compatibility tests** - Work with tools and applications
- **Performance tests** - Benchmark against native drivers

## Compatible Tools & Applications

With ODBC/JDBC support, ScratchBird works with:

- **BI Tools:** Tableau, Power BI, Qlik, Business Objects
- **ETL Tools:** Informatica, Talend, Pentaho
- **Reporting:** Crystal Reports, JasperReports
- **Database Tools:** DBeaver, DataGrip, SQuirreL SQL
- **Applications:** Microsoft Access, Excel, LibreOffice Base
- **Frameworks:** JDBC-based Java frameworks

## Related Specifications

- [Drivers](../drivers/) - Native language drivers
- [Tools](../tools/) - Database management tools
- [ORMs](../orms-frameworks/) - ORM framework support
- [Drivers (Core)](../../drivers/) - Core driver specifications

## Navigation

- **Parent Directory:** [Beta Requirements](../README.md)
- **Specifications Index:** [Specifications Index](../../README.md)
- **Project Root:** [ScratchBird Home](../../../../README.md)

---

**Last Updated:** January 2026
