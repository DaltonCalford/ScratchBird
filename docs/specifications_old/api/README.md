# API Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains client API specifications for ScratchBird.

## Overview

Specifications for client library APIs, connection pooling, and programmatic database access.

## Specifications in this Directory

- **Client library API design** now lives in ScratchBird-driver
  (`docs/specifications/api/CLIENT_LIBRARY_API_SPECIFICATION.md`)
- **[CONNECTION_POOLING_SPECIFICATION.md](CONNECTION_POOLING_SPECIFICATION.md)** - Connection pooling architecture

## Key Features

### Client Library API

The client library API provides:

- **Connection management** - Connect, disconnect, connection pooling
- **Query execution** - Execute SQL queries, fetch results
- **Prepared statements** - Prepare and execute parameterized queries
- **Transaction control** - BEGIN, COMMIT, ROLLBACK
- **Result handling** - Iterate over result sets, fetch columns
- **Error handling** - Exception handling and error reporting
- **Type mapping** - Map database types to language types

### Connection Pooling

Connection pooling features:

- **Pool management** - Create, maintain connection pools
- **Connection reuse** - Reuse connections across requests
- **Health checking** - Validate connections before reuse
- **Configurable limits** - Min/max connections, idle timeout
- **Load balancing** - Distribute connections across nodes (Beta)

## Related Specifications

- [Drivers](../drivers/) - Database driver implementations
- [Network Layer](../network/) - Network protocol layer
- [Wire Protocols](../wire_protocols/) - Wire protocol specifications
- [Security](../Security%20Design%20Specification/) - Authentication and encryption

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
