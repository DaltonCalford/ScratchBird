# FAQ

**Last Updated:** 2026-01-28

## General

### What is ScratchBird?
ScratchBird is a database engine built on a Firebird-style Multi-Generational
Architecture (MGA) with multiple wire protocol listeners. It compiles SQL into
SBLR bytecode for execution, and supports native, Firebird, PostgreSQL, and
MySQL SQL dialects through separate parsers.

### What stage of development is ScratchBird in?
ScratchBird is in Alpha. The core engine, transaction model, indexing, parser
infrastructure, and security subsystems are implemented and tested. Language
drivers and some operational tooling are still in progress.

### How is ScratchBird different from PostgreSQL or MySQL?
ScratchBird uses Firebird's Multi-Generational Architecture (MGA) for
transactions rather than PostgreSQL-style MVCC or MySQL's InnoDB MVCC. It also
supports multiple SQL dialects through emulated parsers, compiles all SQL to
SBLR bytecode, and has a unified engine that can be accessed via PostgreSQL,
MySQL, or Firebird wire protocols.

## Connectivity

### Which ports are used?
- Native: 3092
- PostgreSQL: 5432
- MySQL: 3306
- Firebird: 3050

### Can I connect with psql, mysql, or isql?
Yes. ScratchBird implements wire protocol adapters for PostgreSQL, MySQL, and
Firebird. Standard clients for those databases can connect to ScratchBird on
their respective ports. GUI tools like DBeaver, pgAdmin, and DataGrip also work
via the PostgreSQL protocol.

## Architecture

### Do emulated databases create physical files?
No. Emulated databases are metadata-only schemas; only ScratchBird databases
use on-disk files.

### Is a write-ahead log required?
No. MGA provides recovery without a write-ahead log. A write-after log may be
used later for replication/PITR.

### What is SBLR?
SBLR (ScratchBird Language Representation) is the bytecode intermediate
representation that all SQL statements are compiled to before execution. It is
analogous to Firebird's BLR but extended with modern features. The server core
only understands SBLR - it is dialect-agnostic.

### How do the parsers work?
ScratchBird has a V2 parser for native SQL and separate emulated parsers for
Firebird, PostgreSQL, and MySQL. Each parser translates its dialect's SQL into
SBLR bytecode. The emulated parsers are completely separate from the V2 parser.
On the response path, parsers convert native results back into dialect-specific
format.

## Transactions

### What transaction isolation levels are supported?
READ COMMITTED, REPEATABLE READ, and SERIALIZABLE. The MGA model provides true
snapshot isolation where readers never block writers.

### How does garbage collection work?
ScratchBird has automatic garbage collection through the sweep manager, which
reclaims space from old record versions that are no longer visible to any active
transaction. A long transaction monitor detects and manages transactions that
may delay garbage collection.

## Indexing

### What index types are available?
ScratchBird implements 14 index types: B-tree, Hash, GiST, GIN, SP-GiST, BRIN,
R-tree, Bitmap, LSM-Tree, HNSW, Columnstore, Full-text, Inverted, and
expression indexes. Bloom filters can be attached to B-tree, Hash, and GIN
indexes for accelerated lookups.

### Does ScratchBird support vector search?
Yes. Native vector type with variable dimensions and HNSW (Hierarchical
Navigable Small World) indexing for similarity search.

### Does ScratchBird support full-text search?
Yes. TSVECTOR and TSQUERY types are implemented with dedicated full-text
indexing, GIN index support, and text search configuration.

## Security

### What authentication methods are supported?
SCRAM-SHA-256, Kerberos/GSSAPI, LDAP, OAuth 2.0, SAML, multi-factor
authentication (MFA), and TLS client certificates. Password policy enforcement
and login attempt tracking are also implemented.

### Does ScratchBird support encryption at rest?
Yes. Transparent data encryption is implemented with a key management system.
Column-level data masking is also available.

## Data Types

### What data types are supported?
Numeric (INT8-128, UINT8-128, FLOAT32/64, DECIMAL, MONEY), string (CHAR,
VARCHAR, TEXT), binary (BINARY, VARBINARY, BLOB, BYTEA), date/time (DATE, TIME,
TIMESTAMP, INTERVAL), BOOLEAN, JSON/JSONB, XML, UUID (with UUIDv7), VECTOR,
spatial geometry types (POINT through GEOMETRYCOLLECTION), TSVECTOR/TSQUERY,
range types, network types (INET, CIDR, MACADDR), ARRAY, COMPOSITE, DOMAIN,
and VARIANT.

## Operations

### Is cluster support available?
Cluster features are deferred to Beta; specs exist but runtime support is not
in Alpha.

### Is backup and restore available?
Yes. The backup manager is implemented in the core engine.

## Documentation

### Where is the authoritative spec?
See `docs/specifications/README.md` and the Developers Guide:
- [Developers Guide](developer-guide/README.md)
