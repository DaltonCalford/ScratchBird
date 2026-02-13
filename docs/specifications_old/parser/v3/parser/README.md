# Parser Specifications

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.



**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


**[← Back to Specifications Index](../README.md)**

This directory contains SQL parsing and grammar specifications for ScratchBird's multi-dialect parser architecture.

## Overview

ScratchBird implements a unique multi-dialect SQL parser that supports native ScratchBird SQL, PostgreSQL, MySQL, and Firebird dialects (MSSQL optional extension). This directory contains the complete grammar specifications, parser implementation details, and emulation layer designs.

## Specifications in this Directory

### Core Grammar

- **[SCRATCHBIRD_SQL_COMPLETE_BNF.md](SCRATCHBIRD_SQL_COMPLETE_BNF.md)** (1,527 lines) - Complete BNF grammar for ScratchBird SQL
- **[ScratchBird Master Grammar Specification v2.0.md](ScratchBird Master Grammar Specification v2.0.md)** - Master grammar specification (Version 2)
- **[ScratchBird SQL Language Specification - Master Document.md](ScratchBird SQL Language Specification - Master Document.md)** - Comprehensive SQL language specification

### Dialect Overview

- **[01_SQL_DIALECT_OVERVIEW.md](01_SQL_DIALECT_OVERVIEW.md)** (115 lines) - Overview of supported SQL dialects

### Parser Architecture

- **[EMULATED_DATABASE_PARSER_SPECIFICATION.md](EMULATED_DATABASE_PARSER_SPECIFICATION.md)** (303 lines) - **CRITICAL** - Parser architecture for database emulation
- **[08_PARSER_AND_DEVELOPER_EXPERIENCE.md](08_PARSER_AND_DEVELOPER_EXPERIENCE.md)** (163 lines) - Parser developer experience and tooling

### Emulated Database Parsers

- **[POSTGRESQL_PARSER_SPECIFICATION.md](POSTGRESQL_PARSER_SPECIFICATION.md)** (1,626 lines) - Complete PostgreSQL parser specification
- **[POSTGRESQL_PARSER_IMPLEMENTATION.md](POSTGRESQL_PARSER_IMPLEMENTATION.md)** (671 lines) - PostgreSQL parser implementation details
- **[MYSQL_PARSER_SPECIFICATION.md](MYSQL_PARSER_SPECIFICATION.md)** (949 lines) - MySQL parser specification

### Procedural Language

- **[05_PSQL_PROCEDURAL_LANGUAGE.md](05_PSQL_PROCEDURAL_LANGUAGE.md)** - PSQL procedural language specification
- **[PSQL_CURSOR_HANDLES.md](PSQL_CURSOR_HANDLES.md)** - Cursor handle passing and lifetime rules

### Unified NoSQL Extensions (Beta)

- **[SCRATCHBIRD_UNIFIED_NOSQL_EXTENSIONS.md](SCRATCHBIRD_UNIFIED_NOSQL_EXTENSIONS.md)** - Unified NoSQL language extensions mapped to SBLR

### ScratchBird SQL Core (Alpha)

- **[SCRATCHBIRD_SQL_CORE_LANGUAGE.md](SCRATCHBIRD_SQL_CORE_LANGUAGE.md)** - Core SQL surface and canonical references

### V3 Parser Consolidation (Implementation-First)

- **[../README.md](../README.md)** - V3 parser consolidation index
- **[../SELECT_AND_QUERY.md](../SELECT_AND_QUERY.md)** - SELECT, CTE, set ops, ordering/limits
- **[../JOINS.md](../JOINS.md)** - Join and table reference parsing
- **[../WINDOWING.md](../WINDOWING.md)** - Window functions and frames
- **[../INSERT.md](../INSERT.md)** - INSERT parsing rules
- **[../UPDATE.md](../UPDATE.md)** - UPDATE parsing rules
- **[../DELETE.md](../DELETE.md)** - DELETE parsing rules
- **[../MERGE.md](../MERGE.md)** - MERGE parsing rules
- **[../DDL_CREATE.md](../DDL_CREATE.md)** - CREATE parsing rules
- **[../DDL_ALTER.md](../DDL_ALTER.md)** - ALTER parsing rules
- **[../DDL_DROP_TRUNCATE.md](../DDL_DROP_TRUNCATE.md)** - DROP/TRUNCATE parsing rules
- **[../TRANSACTION_CONTROL.md](../TRANSACTION_CONTROL.md)** - Transaction control parsing rules
- **[../ACCESS_CONTROL.md](../ACCESS_CONTROL.md)** - GRANT/REVOKE parsing rules
- **[../UTILITY_COPY.md](../UTILITY_COPY.md)** - COPY parsing rules
- **[../SESSION_AND_UTILITY.md](../SESSION_AND_UTILITY.md)** - SET/SHOW/RESET/EXPLAIN/ANALYZE/CONNECT/COMMENT/etc
- **[../PSQL_STATEMENTS.md](../PSQL_STATEMENTS.md)** - PSQL procedural statements and cursor handling
- **[../SBLR_V3_OPCODE_SPEC.md](../SBLR_V3_OPCODE_SPEC.md)** - V3 SBLR opcode encoding and registry
- **[../SBLR_V3_OLD_TO_NEW_MAPPING.md](../SBLR_V3_OLD_TO_NEW_MAPPING.md)** - Master old→new opcode mapping
- **[../SBLR_V3_OPCODE_PAYLOADS.md](../SBLR_V3_OPCODE_PAYLOADS.md)** - Per-opcode payload schemas

## Key Concepts

### Multi-Dialect Architecture

ScratchBird uses a listener + parser pool architecture that routes incoming SQL
through dialect-specific parsers:

1. **Native Parser (V2)** - ScratchBird's native SQL dialect
2. **Emulated Parsers** - PostgreSQL, MySQL, Firebird parsers that generate SBLR bytecode directly (MSSQL optional extension)
3. **Parser Isolation** - Emulated parsers are completely separate from V2 parser (no cross-contamination)

### Important Rules

From [EMULATED_DATABASE_PARSER_SPECIFICATION.md](EMULATED_DATABASE_PARSER_SPECIFICATION.md):

- Emulated parsers are COMPLETELY SEPARATE from V2 parser
- DO NOT modify V2 parser for emulation purposes
- Each emulated database has its OWN parser that generates SBLR directly

## Related Specifications

- [SBLR Bytecode](../) - Bytecode target for all parsers
- [DDL Statements](../ddl/) - DDL operation specifications
- [DML Statements](../dml/) - DML operation specifications
- [Query Optimization](../query/) - Query optimizer and planner
- [Listener/Pool Architecture](../core/Y_VALVE_ARCHITECTURE.md) - Multi-dialect routing (legacy Y-Valve spec)

## Critical Reading

Before working on parser implementation:

1. **MUST READ:** [/docs/specifications/parser/v3/MGA_RULES.md](/docs/specifications/parser/v3/MGA_RULES.md) - Absolute MGA architecture rules
2. **MUST READ:** [EMULATED_DATABASE_PARSER_SPECIFICATION.md](EMULATED_DATABASE_PARSER_SPECIFICATION.md) - Parser architecture
3. **MUST READ:** [/docs/specifications/parser/v3/IMPLEMENTATION_STANDARDS.md](/docs/specifications/parser/v3/IMPLEMENTATION_STANDARDS.md) - Implementation standards

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
