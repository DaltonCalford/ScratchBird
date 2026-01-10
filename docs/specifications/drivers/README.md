# Database Driver Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains specifications for database drivers and client library emulation layers.

## Overview

ScratchBird implements driver emulation layers for PostgreSQL (libpq), MySQL (libmysqlclient), and Firebird (fbclient). MSSQL/TDS (FreeTDS) is post-gold, and is documented for future implementation.

## Specifications in this Directory

### Core Driver Specifications

- **[JDBC_DRIVER_SPECIFICATION.md](JDBC_DRIVER_SPECIFICATION.md)** - JDBC driver specification
- **[ODBC_DRIVER_SPECIFICATION.md](ODBC_DRIVER_SPECIFICATION.md)** - ODBC driver specification

### Protocol-Specific Drivers

- **[postgresql_spec.md](postgresql_spec.md)** - PostgreSQL driver emulation
- **[postgresql_technical.md](postgresql_technical.md)** - PostgreSQL technical details
- **[mysql_mariadb_spec.md](mysql_mariadb_spec.md)** - MySQL/MariaDB driver emulation
- **[mssql_spec.md](mssql_spec.md)** - MSSQL/TDS driver emulation (post-gold)
- **[firebird_spec.md](firebird_spec.md)** - Firebird driver emulation

### Generic Specifications

- **[odbc_generic_spec.md](odbc_generic_spec.md)** - Generic ODBC specification
- **[jdbc_jni_spec.md](jdbc_jni_spec.md)** - JDBC JNI integration
- **[unified_interface_spec.md](unified_interface_spec.md)** - Unified driver interface

### Tool Integrations

- **[FlameRobin_Specification_for_AI.md](FlameRobin_Specification_for_AI.md)** (442 lines) - FlameRobin IDE integration

## Key Concepts

### Driver Emulation

ScratchBird emulates multiple database drivers:

1. **Wire Protocol Compatibility** - Speak PostgreSQL/MySQL/Firebird protocols
2. **Library Emulation** - Provide compatible client libraries (libpq, libmysqlclient, etc.)
3. **Transparent Operation** - Applications connect without modification

### Supported Drivers

- **PostgreSQL** - libpq compatible
- **MySQL/MariaDB** - libmysqlclient compatible
- **Firebird** - fbclient compatible
- **MSSQL** - FreeTDS compatible (post-gold)
- **JDBC** - Type 4 JDBC driver
- **ODBC** - ODBC 3.x driver

## Language-Specific Drivers (Beta)

For comprehensive language driver specifications, see [Beta Requirements - Drivers](../beta_requirements/drivers/):

- Python (psycopg2, mysql-connector-python)
- Node.js/TypeScript (pg, mysql2)
- Java (JDBC)
- .NET/C# (Npgsql, MySql.Data)
- Ruby (pg gem)
- PHP (PDO)
- Go (pgx, mysql)
- Rust (tokio-postgres)
- And more...

## Related Specifications

- [Wire Protocols](../wire_protocols/) - Wire protocol specifications
- [Network Layer](../network/) - Network layer and Y-Valve
- [Beta Requirements - Drivers](../beta_requirements/drivers/) - Language-specific driver specifications
- [API](../api/) - Client library API

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
