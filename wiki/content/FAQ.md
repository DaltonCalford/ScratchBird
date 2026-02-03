# FAQ

**Last Updated:** 2026-02-03

## General

### What is ScratchBird?
ScratchBird is a database engine built on a Firebird-style Multi-Generational
Architecture (MGA). It compiles SQL into SBLR bytecode for execution and
supports native (V2) SQL plus emulated Firebird, PostgreSQL, and MySQL dialects.

### What stage of development is ScratchBird in?
ScratchBird is in Alpha. The engine and parser infrastructure are implemented
and exercised in the internal test environment, but features are still evolving.

### How is ScratchBird different from PostgreSQL or MySQL?
ScratchBird uses MGA instead of WAL-based MVCC, separates parsing from execution
via SBLR bytecode, and supports multiple dialects through distinct parsers.

## Connectivity

### Which ports are used?
Ports are configured per listener in `sb_server.conf`. Common defaults are:

- Native: 3092
- PostgreSQL: 5432

### Can I connect with psql, mysql, or isql?
If the corresponding listener is enabled, standard clients can connect (e.g.,
`psql` for PostgreSQL protocol). Availability depends on server configuration
and parser coverage.

## Architecture

### Do emulated databases create physical files?
No. Emulated databases are metadata-only schemas; only ScratchBird databases
create on-disk files.

### Is a write-ahead log required?
No. MGA provides recovery without a mandatory write-after log. WAL is optional
and post-gold.

### What is SBLR?
SBLR (ScratchBird Language Representation) is the bytecode intermediate
representation that all SQL statements compile to before execution. It is
analogous to Firebird's BLR but extended for ScratchBird.

### How do the parsers work?
ScratchBird has a V2 parser for native SQL and separate emulated parsers for
Firebird, PostgreSQL, and MySQL. Each parser translates its dialect's SQL into
SBLR bytecode and maps results back to dialect-specific shapes.

## Data Types and Functions

See the canonical feature list:

- [Feature Catalog](reference/Feature-Catalog.md)

*Last updated: 2026-02-03 | Wiki version synced with codebase*
