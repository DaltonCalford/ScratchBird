# User-Defined Resources (UDR) Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains specifications for ScratchBird's User-Defined Resource (UDR) system.

## Overview

User-Defined Resources (UDRs) allow external code to be called from SQL, enabling integration with external libraries, remote databases, file systems, and custom business logic.

## Specifications in this Directory

- **[10-UDR-System-Specification.md](10-UDR-System-Specification.md)** - Complete UDR system specification
- **[UDR_PSQL_EXTENSION_LIBRARY.md](UDR_PSQL_EXTENSION_LIBRARY.md)** - UDR extension packs for scientific/financial PSQL features

## Key Features

### UDR Types

- **External Functions** - Functions implemented in external languages (C++, Rust, etc.)
- **External Procedures** - Stored procedures in external languages
- **External Triggers** - Triggers implemented externally
- **External Aggregates** - Custom aggregate functions

### UDR Connectors

ScratchBird provides built-in UDR connectors:

- **Remote Database UDR** - Connect to PostgreSQL, MySQL, Firebird, MSSQL (Beta)
- **Local Files UDR** - Read/write local files
- **Local Scripts UDR** - Execute local shell scripts
- **HTTP UDR** - Make HTTP/REST API calls

See [UDR Connectors](../udr_connectors/) and
[Remote Database UDR](../Alpha%20Phase%202/11-Remote-Database-UDR-Specification.md) for detailed connector specifications.

### Example UDR

```sql
-- Define external function
CREATE FUNCTION my_external_func(x INTEGER, y INTEGER)
RETURNS INTEGER
EXTERNAL NAME 'my_library.so:calculate'
ENGINE UDR;

-- Use it in queries
SELECT my_external_func(price, quantity) FROM orders;
```

### Security Model

- **Sandboxing** - UDRs execute in sandboxed environment
- **Resource limits** - CPU, memory, execution time limits
- **Privilege requirements** - Special privileges required to create UDRs
- **Audit logging** - All UDR executions logged

## Related Specifications

- [UDR Connectors](../udr_connectors/) - Built-in UDR connector specifications
- [Remote Database UDR](../Alpha%20Phase%202/11-Remote-Database-UDR-Specification.md) - Remote database adapter specification
- [DDL UDR](../ddl/DDL_USER_DEFINED_RESOURCES.md) - CREATE FUNCTION EXTERNAL syntax
- [Security](../Security%20Design%20Specification/) - UDR security model

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Related:** [UDR Connectors](../udr_connectors/README.md)
- **Related:** [Remote Database UDR](../Alpha%20Phase%202/11-Remote-Database-UDR-Specification.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
