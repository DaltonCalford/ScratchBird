# Testing Specifications

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**[← Back to Specifications Index](../README.md)**

This directory contains testing plans and specifications for ScratchBird.

## Overview

Comprehensive testing specifications covering Alpha and Beta test plans, compatibility testing, and quality assurance procedures.

## Specifications in this Directory

### Test Infrastructure

- **[Test Server](test_server/)** - Complete test server documentation
  - **[README.md](test_server/README.md)** - Test server specification and configuration
  - **[OPERATIONS.md](test_server/OPERATIONS.md)** - Operational procedures and management
  - **[SECURITY_TESTING.md](test_server/SECURITY_TESTING.md)** - Security compliance testing
  
### Test Plans

- **[ALPHA3_TEST_PLAN.md](ALPHA3_TEST_PLAN.md)** (727 lines) - Alpha 3 test plan and procedures
- **[DIALECT_CONFORMANCE_ASSERTIONS.md](DIALECT_CONFORMANCE_ASSERTIONS.md)** - Required assertions per dialect

## Test Categories

### Unit Tests

- **Core Components** - Storage engine, transaction manager, catalog
- **Parser** - SQL parsing for all supported dialects
- **Optimizer** - Query optimization and plan generation
- **SBLR** - Bytecode generation and execution
- **Indexes** - Index implementations and operations

### Integration Tests

- **End-to-End** - Complete query execution pipeline
- **Multi-Dialect** - Cross-dialect compatibility testing
- **Transaction** - ACID compliance and isolation level testing
- **Replication** - Replication correctness (Beta)
- **Cluster** - Distributed operation testing (Beta)

### Compatibility Tests

- **PostgreSQL Compatibility** - psql, libpq, pg_dump compatibility
- **MySQL Compatibility** - mysql client, mysqldump compatibility
- **Firebird Compatibility** - isql, flamerobin compatibility
- **ORM Compatibility** - SQLAlchemy, Hibernate, etc. (Beta)
- **Tool Compatibility** - DBeaver, pgAdmin, etc. (Beta)

### Performance Tests

- **Benchmark Suite** - TPC-H, TPC-C style benchmarks
- **Stress Testing** - High-load scenario testing
- **Scalability** - Multi-core and cluster scalability (Beta)

## Test Infrastructure

### Test Server

The **ScratchBird Test Server** provides a dedicated instance for comprehensive testing:

- **Bootstrap Authentication** - Initial setup and connectivity testing
- **SCRAM-SHA-256** - Production-like security compliance testing
- **Multi-Protocol** - Native, PostgreSQL, Firebird protocols
- **HBA Rules** - Host-based authentication testing
- **Rate Limiting** - Brute force protection validation

**Quick Links:**
- [Test Server Documentation](test_server/README.md)
- [Operational Procedures](test_server/OPERATIONS.md)
- [Security Testing](test_server/SECURITY_TESTING.md)

**Quick Start:**
```bash
# Setup
./scripts/test-server-user.sh setup

# Start
./scripts/test-server-user.sh start

# Connect
scratchbird://anyuser:anypass@127.0.0.1:3092/testdb
```

### Automated Testing

- **Automated CI/CD** - GitHub Actions integration
- **Coverage Tracking** - Code coverage measurement
- **Regression Testing** - Automated regression test suite
- **Fuzzing** - SQL fuzzing for parser robustness

## Related Specifications

- [Implementation Standards](/docs/specifications/parser/v3/IMPLEMENTATION_STANDARDS.md) - Testing requirements for all features
- [Beta Requirements](../beta_requirements/) - Compatibility testing specifications

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
