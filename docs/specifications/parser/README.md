# Parser Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains SQL parsing and grammar specifications for ScratchBird's multi-dialect parser architecture.

## Overview

ScratchBird implements a unique multi-dialect SQL parser that supports native ScratchBird SQL, PostgreSQL, MySQL, and Firebird dialects (MSSQL post-gold). This directory contains the complete grammar specifications, parser implementation details, and emulation layer designs.

## Specifications in this Directory

### Core Grammar

- **[SCRATCHBIRD_SQL_COMPLETE_BNF.md](SCRATCHBIRD_SQL_COMPLETE_BNF.md)** (1,527 lines) - Complete BNF grammar for ScratchBird SQL
- **[ScratchBird Master Grammar Specification v2.0.md](ScratchBird%20Master%20Grammar%20Specification%20v2.0.md)** - Master grammar specification (Version 2)
- **[ScratchBird SQL Language Specification - Master Document.md](ScratchBird%20SQL%20Language%20Specification%20-%20Master%20Document.md)** - Comprehensive SQL language specification

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

## Key Concepts

### Multi-Dialect Architecture

ScratchBird uses a listener + parser pool architecture that routes incoming SQL
through dialect-specific parsers:

1. **Native Parser (V2)** - ScratchBird's native SQL dialect
2. **Emulated Parsers** - PostgreSQL, MySQL, Firebird parsers that generate SBLR bytecode directly (MSSQL post-gold)
3. **Parser Isolation** - Emulated parsers are completely separate from V2 parser (no cross-contamination)

### Important Rules

From [EMULATED_DATABASE_PARSER_SPECIFICATION.md](EMULATED_DATABASE_PARSER_SPECIFICATION.md):

- Emulated parsers are COMPLETELY SEPARATE from V2 parser
- DO NOT modify V2 parser for emulation purposes
- Each emulated database has its OWN parser that generates SBLR directly

## Related Specifications

- [SBLR Bytecode](../sblr/) - Bytecode target for all parsers
- [DDL Statements](../ddl/) - DDL operation specifications
- [DML Statements](../dml/) - DML operation specifications
- [Query Optimization](../query/) - Query optimizer and planner
- [Listener/Pool Architecture](../core/Y_VALVE_ARCHITECTURE.md) - Multi-dialect routing (legacy Y-Valve spec)

## Critical Reading

Before working on parser implementation:

1. **MUST READ:** [../../../MGA_RULES.md](../../../MGA_RULES.md) - Absolute MGA architecture rules
2. **MUST READ:** [EMULATED_DATABASE_PARSER_SPECIFICATION.md](EMULATED_DATABASE_PARSER_SPECIFICATION.md) - Parser architecture
3. **MUST READ:** [../../../IMPLEMENTATION_STANDARDS.md](../../../IMPLEMENTATION_STANDARDS.md) - Implementation standards

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
