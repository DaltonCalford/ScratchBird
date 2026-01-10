# ScratchBird Feature Extraction Checklist

**Purpose:** Comprehensive extraction of every feature from all specifications for code verification

**Last Updated:** January 2026

**Status:** ✅ UPDATED (Alpha/Beta tagging, gaps filled)

This document provides a complete checklist of every feature, operation, and capability specified across all ScratchBird specification documents. Use this for:
- Code implementation verification
- Test coverage planning
- Feature gap analysis
- Documentation validation

---

## Document Structure

This checklist is organized into the following major sections:

1. **DDL Operations** (300+ features) - ✅ COMPLETE
2. **DML Operations** (250+ features) - ✅ COMPLETE
3. **Transaction System** (400+ features) - ✅ COMPLETE
4. **SBLR Bytecode** (500+ features) - ✅ COMPLETE
5. **System Catalog** (700+ features) - ✅ COMPLETE
6. **Triggers** (257 features) - ✅ COMPLETE
7. **UDR System** (500+ features) - ✅ COMPLETE
8. **Network & Wire Protocols** (750+ features) - ✅ COMPLETE
9. **Tools & Operations** (583 features) - ✅ COMPLETE
10. **Compression** (163 features) - ✅ COMPLETE
11. **API** (390+ features) - ✅ COMPLETE
12. **Testing** (451 features) - ✅ COMPLETE
13. **Scheduler** (350+ features) - ✅ COMPLETE
14. **Core Engine** (677 features) - ✅ COMPLETE
15. **Remote Database UDR** (438 features) - ✅ COMPLETE
16. **Drivers** (750+ features, 11 languages) - ✅ COMPLETE
17. **Connectivity** (286 features) - ✅ COMPLETE
18. **ORM/Frameworks** (12 frameworks) - ✅ COMPLETE
19. **Storage Engine & On-Disk Format** - ✅ ADDED
20. **Indexes & Access Methods** - ✅ ADDED
21. **Data Types & Casting** - ✅ ADDED
22. **Parser & Dialects** - ✅ ADDED
23. **Query Optimizer & Planner** - ✅ ADDED
24. **Security & Authentication** - ✅ ADDED
25. **Backup & Restore** - ✅ ADDED
26. **Deployment & Packaging** - ✅ ADDED
27. **Cluster & Replication** - ✅ ADDED

**Phase Tagging:** Each subsection heading includes `[Alpha]` or `[Beta]`. Bullets inherit the subsection phase unless explicitly tagged.

**TOTAL: 8,000+ Discrete, Testable Features**

---

## 1. DDL OPERATIONS (300+ Features)

See [docs/specifications/ddl/README.md](specifications/ddl/README.md)

### Database Objects (50+ features) [Alpha]
- Database creation, alteration, and deletion
- Schema management and hierarchical organization
- Table creation with all column types and constraints
- View creation (materialized and standard)
- Index creation (11+ types including B-tree, Hash, GiST, GIN, BRIN, etc.)
- Sequence generation and management

### Advanced Table Features (60+ features) [Alpha]
- Table partitioning (RANGE, LIST, HASH)
- Temporal tables and time-travel queries
- Generated columns (VIRTUAL and STORED)
- Identity columns with sequence backing
- Constraints (PRIMARY KEY, FOREIGN KEY, UNIQUE, CHECK, NOT NULL, DEFAULT, EXCLUSION)

### Procedural Objects (80+ features) [Alpha]
- Function creation (SQL, PSQL, EXTERNAL)
- Stored procedure creation
- Package creation (Firebird-style)
- Trigger creation (BEFORE, AFTER, INSTEAD OF)
- Event creation and scheduling
- Exception definition

### User-Defined Resources (40+ features) [Alpha]
- UDR (external function/procedure) creation
- Role and group management
- Row-level security policies
- Foreign data wrapper support
- CASCADE/RESTRICT drop behavior (1,029 lines of spec)

---

## 2. DML OPERATIONS (250+ Features)

See [docs/specifications/dml/README.md](specifications/dml/README.md)

### Core DML (100+ features) [Alpha]
- SELECT with all clauses (WHERE, GROUP BY, HAVING, ORDER BY, LIMIT, OFFSET)
- INSERT (single-row, multi-row, INSERT ... SELECT, ON CONFLICT upsert)
- UPDATE (single-table, multi-table, UPDATE ... FROM, correlated)
- DELETE (basic, DELETE ... USING, correlated)
- MERGE (WHEN MATCHED/NOT MATCHED, complex merge logic)
- RETURNING clause support for all DML

### Advanced Query Features (150+ features) [Alpha]
- JOIN operations (INNER, LEFT, RIGHT, FULL, CROSS)
- Subqueries (scalar, correlated, EXISTS, IN, NOT IN)
- CTEs (WITH clauses, recursive WITH)
- Window functions (ROW_NUMBER, RANK, LAG, LEAD, etc.)
- Set operations (UNION, INTERSECT, EXCEPT)
- XML_TABLE() and JSON_TABLE() functions

### MGA Integration [Alpha]
- Consistent snapshot reads based on isolation level
- Multi-version record creation for INSERT/UPDATE
- Tombstone marking for DELETE
- Garbage collection coordination

---

## 3. TRANSACTION SYSTEM (400+ Features)

See [docs/specifications/transaction/TRANSACTION_MAIN.md](specifications/transaction/TRANSACTION_MAIN.md)

### Core MGA (Multi-Generational Architecture) (150+ features) [Alpha]
- 64-bit transaction IDs (no wraparound)
- Transaction Inventory Pages (TIP) management
- Record versioning with xmin/xmax tracking
- Visibility determination algorithms
- Snapshot isolation implementation
- Read committed with read consistency
- Repeatable read support
- Serializable isolation with SSI (Serializable Snapshot Isolation)

### Transaction Control (80+ features) [Alpha]
- BEGIN TRANSACTION with isolation levels
- COMMIT (normal and RETAINING)
- ROLLBACK (normal and RETAINING)
- SAVEPOINT creation and management
- Two-phase commit preparation [Beta]
- Distributed transaction coordination [Beta]

### Lock Management (90+ features) [Alpha]
- Table-level locking (SHARED, PROTECTED, EXCLUSIVE)
- Row-level locking (for updates)
- Lock wait modes (WAIT, NO WAIT, LOCK TIMEOUT)
- Deadlock detection and resolution
- Lock escalation strategies
- Advisory locks

### Garbage Collection (80+ features) [Alpha]
- Background sweep process
- Record version cleanup
- Index tombstone removal
- Statistics collection during sweep
- Configurable sweep intervals
- Online garbage collection

---

## 4. SBLR BYTECODE (500+ Features)

See [docs/specifications/sblr/SBLR_OPCODE_REGISTRY.md](specifications/sblr/SBLR_OPCODE_REGISTRY.md)

### Bytecode Format & Encoding (40+ features) [Alpha]
- Compact stream format with version validation
- Extended opcode prefix (0xFF) for 16-bit opcodes
- Little-endian multi-byte integers
- UVARINT encoding (unsigned LEB128)
- UTF-8 NFC string encoding
- Module container format with CRC32 checksum

### Data Types (60+ features) [Alpha]
- 40+ base type opcodes (INTEGER, BIGINT, VARCHAR, UUID, JSON, etc.)
- Type coercion and casting
- Domain type support
- Array type encoding
- Composite/record types
- Extended types (INT128, POINT, TSVECTOR, ranges)

### Expressions & Operations (200+ features) [Alpha]
- Arithmetic operations (ADD, SUBTRACT, MULTIPLY, DIVIDE, MODULO)
- Comparison operations (EQ, NE, LT, GT, LE, GE, NULL_SAFE_EQ)
- Logical operations (AND, OR, NOT, IS_NULL)
- String operations (LENGTH, SUBSTRING, UPPER, LOWER, TRIM, CONCAT, pattern matching)
- Mathematical functions (60+ including SIN, COS, SQRT, POWER, LOG, etc.)
- Cryptographic functions (MD5, SHA1, SHA256, SHA512)
- JSON operations (EXTRACT, BUILD, SET, INSERT, REMOVE)
- XML operations (PARSE, SERIALIZE, XPATH)
- Array operations (APPEND, CAT, REMOVE, OVERLAP, CONTAINS)
- Range type operations (OVERLAPS, CONTAINS, UNION, INTERSECTION)
- Spatial/geometry operations (40+ including ST_POINT, ST_INTERSECTS, ST_BUFFER)
- Full-text search operations (TSMATCH, TS_RANK, TO_TSVECTOR)

### DML/DDL Statements (60+ features) [Alpha]
- INSERT, SELECT, UPDATE, DELETE, TRUNCATE_TABLE
- CREATE_TABLE, DROP_TABLE, ALTER_TABLE
- CREATE_INDEX, DROP_INDEX
- CREATE_SEQUENCE, ALTER_SEQUENCE, DROP_SEQUENCE
- MERGE operations (START through END)
- ON CONFLICT (upsert) support

### Procedural SQL (40+ features) [Alpha]
- FUNCTION and PROCEDURE definitions
- Variable declaration and assignment
- Control flow (IF, ELSIF, ELSE, LOOP, WHILE, EXIT, RETURN)
- Exception handling (TRY, EXCEPT, RAISE)
- Cursor operations (DECLARE, OPEN, FETCH, CLOSE)

### Security & Access Control (30+ features) [Alpha]
- User and role management opcodes
- GRANT/REVOKE privilege opcodes
- Row-level security policy opcodes
- Session authorization

### Aggregate & Window Functions (50+ features) [Alpha]
- Basic aggregates (SUM, AVG, MIN, MAX, COUNT)
- Statistical aggregates (STDDEV, VARIANCE, CORR, COVAR)
- Regression aggregates (REGR_SLOPE, REGR_INTERCEPT, REGR_R2)
- Window functions (ROW_NUMBER, RANK, DENSE_RANK, LAG, LEAD, NTH_VALUE)
- Window specification (PARTITION BY, ORDER BY, frame clauses)

---

## 5. SYSTEM CATALOG (700+ Features)

See [docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md](specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md)

### Core Object Definitions (150+ features) [Alpha]
- sys.databases, sys.schemas, sys.tables, sys.columns
- sys.indexes, sys.index_versions (shadow rebuild support)
- sys.constraints (all types including EXCLUSION, IN/NOT IN subquery)
- sys.sequences, sys.views, sys.triggers
- sys.procedures, sys.procedure_params, sys.packages
- sys.domains, sys.composite_types, sys.types

### Security & Access Control (80+ features) [Alpha]
- sys.users, sys.roles, sys.groups
- sys.role_members, sys.permissions
- sys.row_level_security policies
- Authentication methods (LOCAL, SSO, LDAP, KERBEROS)
- Password hashing (bcrypt, scrypt, argon2)
- Multi-factor authentication support

### Advanced Features (70+ features) [Alpha]
- sys.tablespaces, sys.foreign_servers, sys.foreign_tables
- sys.udr (User-Defined Resources)
- sys.dependencies (object dependency tracking)
- sys.comments, sys.statistics
- Character sets (sys.charsets) and collations (sys.collation_defs)
- Timezone definitions (sys.timezones)

### Emulation Support (60+ features) [Alpha]
- sys.emulation_types, sys.emulation_servers, sys.emulated_databases
- PostgreSQL pg_catalog emulation views
- MySQL information_schema emulation views
- Firebird RDB$ system table emulation
- MSSQL sys catalog view emulation

### Schema Path Resolution (80+ features) [Alpha]
- Hierarchical schema organization (18 bootstrap schemas)
- Default schema assignment from user/role/group
- Search path resolution for unqualified names
- Relative path resolution (leading dot syntax)
- Absolute path resolution
- No-search prefix (!:) support
- Case-sensitive/insensitive identifier matching

### Catalog Operations & Caching (150+ features) [Alpha]
- In-memory catalog caches (schema, table, column, index, sequence, view)
- Thread-safe catalog mutex protection
- Catalog CRUD operations (create, update, delete, query)
- TOAST support for large catalog values
- Catalog bootstrap and initialization
- Catalog versioning and migration

### Component Architecture (50+ features) [Alpha]
- CatalogManager coordinator
- SchemaResolver for name resolution
- PermissionManager for access control
- MetadataCache for caching layer
- UUID v7-based object identifiers

---

## 6. TRIGGERS (257 Features)

See [docs/specifications/triggers/TRIGGER_CONTEXT_VARIABLES.md](specifications/triggers/TRIGGER_CONTEXT_VARIABLES.md)

### Trigger Types (30+ features) [Alpha]
- DML triggers (BEFORE/AFTER/INSTEAD OF for INSERT/UPDATE/DELETE/SELECT)
- Database event triggers (CONNECT, DISCONNECT, TRANSACTION START/COMMIT/ROLLBACK)
- DDL triggers (BEFORE/AFTER for CREATE/ALTER/DROP on all object types)
- Row-level and statement-level granularity
- Multi-event triggers (INSERT OR UPDATE OR DELETE)
- Column-specific triggers (UPDATE OF column_list)

### Trigger Execution (40+ features) [Alpha]
- ACTIVE/INACTIVE state management
- POSITION clause for firing order (0-32767)
- NEW and OLD record variables (read-write in BEFORE, read-only in AFTER)
- WHEN clause for conditional firing
- Trigger chain communication (shared data between triggers)
- Flow control (RETURN NEW, RETURN OLD, RETURN NULL, SKIP_REMAINING, CANCEL)

### Context Variables (120+ features) [Alpha]
- Event detection (GET TRIGGER_EVENT, INSERTING, UPDATING, DELETING)
- Trigger metadata (NAME, TIMING, LEVEL, SCHEMA, TABLE, POSITION)
- Database context (DATABASE, CATALOG, TABLESPACE)
- User and session variables (USER, SESSION_USER, APPLICATION, CLIENT_IP, PID)
- Timing information (TIMESTAMP, TRANSACTION_ID, STATEMENT_ID, EXECUTION_TIME)
- Nested trigger detection (DEPTH, PARENT, CHAIN)
- Column change detection (CHANGED_COLUMNS, IS_COLUMN_CHANGED)
- Column value access (GET_COLUMN_VALUE for OLD/NEW)

### Statement-Level Features (20+ features) [Alpha]
- Transition tables (OLD TABLE AS, NEW TABLE AS)
- TRIGGER_ROW_COUNT variable
- TRIGGER_AFFECTED_IDS array

### DDL Trigger Context (20+ features) [Alpha]
- RDB$GET_CONTEXT with DDL_TRIGGER namespace
- EVENT_TYPE, OBJECT_TYPE, OBJECT_NAME context values
- SQL_TEXT context (full DDL statement)
- OLD_OBJECT_NAME/NEW_OBJECT_NAME for renames

### SQL Syntax (27+ features) [Alpha]
- CREATE TRIGGER with all options
- ALTER TRIGGER (status, phase, events, position, body, rename)
- DROP TRIGGER (CASCADE, RESTRICT)
- CREATE OR ALTER TRIGGER
- RECREATE TRIGGER
- SQL SECURITY (DEFINER, INVOKER)

---

## 7. UDR SYSTEM (500+ Features)

See [docs/specifications/udr/10-UDR-System-Specification.md](specifications/udr/10-UDR-System-Specification.md)

### UDR Plugin Management (50+ features) [Alpha]
- Plugin discovery and loading from shared libraries
- Plugin lifecycle (initialize, execute, shutdown)
- Version compatibility checking
- Entry point (fb_udr_plugin) support
- Plugin factory interface
- Reference counting
- Dynamic library loading (dlopen/dlsym)

### UDR API Interfaces (60+ features) [Alpha]
- IPluginModule, IPluginFactory
- IExternalFunction, IExternalProcedure, IExternalTrigger
- IStatus (error handling)
- IContext (engine services)
- IMessageBuffer (parameter/result data)
- IMessageMetadata (field information)
- Memory allocation through context
- SQL execution from UDR

### UDR Connectors (Baseline) (140+ features) [Beta]
- Connector packaging (shared library + JSON manifest + signature)
- Signature verification (ed25519)
- Checksum validation (SHA256)
- SQL/MED foreign data wrapper support
- CREATE FOREIGN DATA WRAPPER, CREATE SERVER, CREATE USER MAPPING
- IMPORT FOREIGN SCHEMA
- CREATE/ALTER/DROP FOREIGN TABLE
- Capability model (network, file read/write, subprocess)
- Server-level configuration options (pass-through, DDL/DML, hosts/ports, limits)
- Connection lifecycle and pooling
- Pass-through execution (remote_exec, remote_query, remote_call)
- Change Data Capture for migration
- Dual-write coordination
- Conflict detection and resolution strategies

### Remote Database UDR (110+ features) [Beta]
- PostgreSQL client (wire protocol 3.0, libpq integration, SCRAM auth)
- MySQL client (wire protocol, libmysqlclient, caching_sha2_password)
- MSSQL client (TDS 7.0-7.4, FreeTDS, Windows auth, Azure AD)
- Firebird client (wire protocol, fbclient, SRP auth)
- ScratchBird federation (cluster PKI auth)
- Type mapping for 40+ types per database
- Query pushdown (WHERE, ORDER BY, LIMIT, aggregates, JOINs)
- Schema introspection and import
- Migration workflows (Big Bang, Incremental, Parallel Run, Strangler Fig)

### Local Files UDR (40+ features) [Beta]
- CSV file support (with headers, custom delimiters)
- JSON array format
- JSON Lines (jsonl) format
- Path canonicalization and validation
- Allowlist/denylist patterns
- Streaming parsing with backpressure

### Local Scripts UDR (30+ features) [Beta]
- Approved interpreter allowlist
- CSV output format, JSON Lines output
- Parameter passing via stdin
- Row output via stdout
- Timeout enforcement
- Network access denial

---

## 8. NETWORK & WIRE PROTOCOLS (750+ Features across 5 Protocols)

See [docs/specifications/network/WIRE_PROTOCOL_SPECIFICATIONS.md](specifications/network/WIRE_PROTOCOL_SPECIFICATIONS.md)

### Network Layer Architecture (60+ features) [Alpha]
- Connection pooling with adaptive sizing
- Connection validation and health checking
- Protocol detection from initial bytes
- Y-Valve router for multi-protocol support
- Load balancing (round-robin, least connections, latency-based)
- Zero-copy operations
- Compression (zstd)
- Result caching

### Firebird Wire Protocol (120+ features) [Alpha]
- 100+ operation codes (op_connect, op_attach, op_execute, op_fetch, etc.)
- XDR encoding for all data types
- SRP authentication and legacy auth
- Batch operations (Protocol 17+)
- Replication protocol [Beta]
- BLOB and ARRAY handling
- Wire encryption support

### MySQL Wire Protocol (100+ features) [Alpha]
- Initial handshake and capability negotiation
- mysql_native_password and caching_sha2_password auth
- COM_QUERY, COM_STMT_PREPARE, COM_STMT_EXECUTE
- Binary protocol (prepared statements)
- SSL/TLS negotiation
- Compression protocol
- Replication protocol (binlog) [Beta]
- All MySQL type encodings

### PostgreSQL Wire Protocol (120+ features) [Alpha]
- Protocol version 3.0
- StartupMessage, SSLRequest, GSSENCRequest
- SCRAM-SHA-256 authentication
- Simple query protocol
- Extended query protocol (Parse, Bind, Execute)
- COPY protocol (text, binary, CSV)
- Notification protocol (LISTEN/NOTIFY)
- Streaming replication protocol [Beta]
- 40+ OID types

### TDS Wire Protocol (SQL Server) (100+ features) [Alpha]
- TDS 7.0-7.4 versions
- Pre-login and Login7 packets
- Token stream protocol (100+ tokens)
- RPC requests (sp_executesql, etc.)
- Bulk copy protocol (BCP)
- MARS (Multiple Active Result Sets)
- SQL Server type encodings
- Windows Authentication, Azure AD

### ScratchBird Native Protocol (250+ features) [Alpha]
- Protocol v1.1 with TLS 1.3 requirement
- 80+ message types (client, server, bidirectional)
- 40-byte message header with UUID attachment tracking
- zstd compression
- SCRAM-SHA-256 and certificate auth
- Cluster PKI auth [Beta]
- Cluster key infrastructure with X.509 certificates [Beta]
- HKDF session key derivation
- Streaming with backpressure control
- Query execution (QUERY, PARSE/BIND/EXECUTE, SBLR_EXECUTE)
- Federated query support [Beta]
- Pub/sub messaging (SUBSCRIBE/NOTIFY)
- 86+ native type serializations
- Error handling with SQLSTATE mapping

---

## 9. TOOLS & OPERATIONS (583 Features)

See [docs/specifications/admin/SB_ADMIN_CLI_SPECIFICATION.md](specifications/admin/SB_ADMIN_CLI_SPECIFICATION.md)

### sb_admin CLI Commands (137+ features) [Alpha]
- Server management (status, start, stop, restart, reload, config, connections, kill)
- Database management (list, create, drop, info, size, vacuum, analyze, check)
- Cluster management (status, init, join, leave, nodes, promote, demote, failover) [Beta]
- User management (list, create, drop, alter, password, roles, grant, revoke)
- Backup commands (create, list, info, restore, verify, delete, schedule, export) [Beta]
- Restore commands (full, PITR, table, status, cancel) [Beta]
- Diagnostics (health, slow-queries, locks, bloat, cache, io, wait-events, activity, explain)
- Log analysis (tail, search, errors, stats)
- Monitoring integration (Nagios checks, Prometheus metrics, SNMP)
- Maintenance (vacuum, reindex, enable/disable maintenance mode)
- Security (audit, SSL status, key rotation, firewall rules)

### Server Binary (sb_server) (128+ features) [Alpha]
- Command-line options (config file, data dir, ports for all protocols, SSL, connections)
- Database modes (single-database, multi-database)
- Network protocol listeners (native, PostgreSQL, MySQL, TDS, Firebird, Unix sockets)
- SSL/TLS support (certificates, minimum/maximum versions, cipher suites, client certs)
- Connection management (limits per user/database, timeouts, pooling)
- Memory management (shared buffers, work memory, huge pages)
- Storage configuration (WAL directory, checkpoints, fsync, compression)
- Authentication (multiple methods, password hashing, LDAP, Kerberos)
- Audit logging (JSON/text/syslog formats, tamper detection)
- Logging (levels, destinations, formats, rotation, slow queries)
- Statistics collection (Prometheus, StatsD)
- Replication configuration [Beta]
- Cluster configuration (discovery, federation) [Beta]

### systemd Integration (51+ features) [Alpha]
- Service unit file with sd_notify() support
- Socket activation
- Resource limits (files, processes, memory)
- Security hardening (NoNewPrivileges, PrivateTmp, ProtectSystem, etc.)
- Restart policies

### Prometheus Metrics (72+ features) [Alpha]
- Connection metrics (10+ metrics)
- Query metrics (6+ metrics)
- Transaction metrics (5+ metrics)
- MGA/MVCC metrics (4+ metrics)
- Memory metrics (4+ metrics)
- Storage metrics (9+ metrics)
- WAL/Recovery metrics (5+ metrics)
- Replication metrics (4+ metrics)
- Lock metrics (4+ metrics)
- Connection pool metrics (5+ metrics)
- Cache metrics (7+ metrics)
- Garbage collection metrics (5+ metrics)
- Backup metrics (4+ metrics)
- Server info metrics (4+ metrics)

### Prometheus Alerting Rules (13+ features) [Alpha]
- Connection, query, replication, memory, disk, lock, backup, GC, server alerts

### Third-Party Tool Integrations (38+ features) [Beta]
- DBeaver (Community & Enterprise)
- pgAdmin 4
- DataGrip (JetBrains)
- MySQL Workbench
- Tableau, Power BI, Qlik, Metabase
- Grafana, Prometheus
- Microsoft Excel

---

## 10. COMPRESSION (163 Features)

See [docs/specifications/compression/COMPRESSION_FRAMEWORK.md](specifications/compression/COMPRESSION_FRAMEWORK.md)

### Algorithms (6+ features) [Alpha]
- None/Uncompressed, LZ4 (default), Zstandard (zstd), Snappy, Brotli, GZIP

### Compression Levels (3+ features) [Alpha]
- FASTEST, DEFAULT, BEST

### Page Compression (45+ features) [Alpha]
- Heap, B-tree leaf, B-tree internal page types
- Compression threshold (>50% full, >10% space savings, >256 bytes data)
- PAGE_FLAG_COMPRESSED marker
- Compressed page header with metadata
- Page size support (8KB-128KB)
- Typical compression ratio (2-3x)

### TOAST Compression (15+ features) [Alpha]
- PLAIN, EXTENDED, COMPRESSED, EXTERNAL strategies
- Automatic compression with EXTERNAL strategy
- Automatic decompression on detoasting
- TOAST chunk compression
- Text data compression (50-90% savings)

### Backup Compression (35+ features) [Beta]
- Per-page compression in backup
- Compression type field in backup blocks
- Block-level compression
- Adaptive compression (sample-based detection)
- Algorithm selection based on compressibility

### Wire Protocol Compression (3+ features) [Alpha]
- Firebird wire compression (zlib)
- MySQL protocol compression
- Streaming backup compression

### Configuration & Tuning (20+ features) [Alpha]
- Database-level compression setting
- Table-level compression setting (future)
- Column-level compression (future)
- Compression dictionaries (future)

### Statistics & Monitoring (10+ features) [Alpha]
- bytes_in, bytes_out, compress_time_us, decompress_time_us, compress_calls, decompress_calls

### Testing Requirements (26+ features) [Alpha]
- Unit tests for codec functionality
- Integration tests with page manager
- All page sizes (8KB-128KB)
- Mixed compressed/uncompressed pages
- Crash recovery with compressed pages
- Interoperability tests

---

## 11. API (390+ Features)

See [docs/specifications/api/CLIENT_LIBRARY_API_SPECIFICATION.md](specifications/api/CLIENT_LIBRARY_API_SPECIFICATION.md)

### Connection Management (28+ features) [Alpha]
- Create connection (remote and embedded)
- Initialize connection options with defaults
- Close connection, disconnect, reconnect
- Ping/health check
- Reset session state
- Get server version, error message, SQLSTATE

### Connection Options (18+ features) [Alpha]
- host, port, database, username, password
- connect_timeout, query_timeout, idle_timeout
- SSL/TLS mode and certificate configuration
- auto_reconnect, read_only, application_name

### Query Execution (15+ features) [Alpha]
- Execute SQL query with results
- Execute statement without results (DML)
- Execute batch SQL
- Execute asynchronously
- Check async completion, wait, get result
- Cancel running query

### Prepared Statements (15+ features) [Alpha]
- Prepare statement
- Get parameter count
- Bind parameters by index/name (NULL, int, double, string, blob, date, timestamp, generic value)
- Clear bindings
- Execute prepared statement (with/without results)
- Free prepared statement

### Result Set Handling (22+ features) [Alpha]
- Get column count, column description, column index by name
- Move to next row
- Get rows affected count
- Free result set
- Check if NULL, get typed values (40+ type accessors)

### Transaction Management (10+ features) [Alpha]
- Begin transaction (default/specific isolation/read-only)
- Commit, rollback
- Create savepoint, rollback to savepoint, release savepoint
- Check if in transaction
- Set autocommit mode

### Batch Operations (9+ features) [Alpha]
- Create batch
- Add values to batch row (NULL, int, double, string, blob)
- End batch row
- Execute batch
- Free batch

### Notifications (4+ features) [Alpha]
- Subscribe to channel
- Unsubscribe from channel
- Send notification
- Notification callback

### Metadata Query (5+ features) [Alpha]
- List databases, schemas, tables
- Describe table columns
- Describe table indexes

### Utility Functions (9+ features) [Alpha]
- Escape string/identifier for SQL
- Get type name, parse type name
- Convert value to string
- Get error message for code
- Check if error is retryable

### Connection Pooling (200+ features) [Alpha]
- Pool architecture (Pool Manager, database pools, user pools, statement cache, result cache)
- Pool modes (session, transaction, statement)
- Pool lifecycle (initialize, create, pre-warm, acquire, release, shutdown)
- Connection states (CREATED, IDLE, ACQUIRED, IN_TRANSACTION, CLOSING, CLOSED)
- Pool configuration (20+ options: min/max sizes, timeouts, validation, lifetimes)
- Statement cache (shared and connection-local, LRU/LFU/FIFO eviction, query normalization)
- Result cache (TTL, MGA epoch validation, table dependency tracking, invalidation)
- Health checking (acquire/release/background validation, TCP keepalive, validation query)
- Load balancing (round-robin, least connections, weighted, latency-based, read/write split)
- Pool statistics (30+ metrics: connections, acquires, releases, waits, cache hit ratios)
- SQL interface (SHOW/ALTER POOL commands, DISCARD CACHE commands)
- Security (user isolation, context reset, credential wiping)
- Thread safety (pool-level mutexes, RW locks for caches, atomic counters)

### Data Types (40+ types) [Alpha]
- NULL, BOOLEAN
- SMALLINT, INTEGER, BIGINT, REAL, DOUBLE, DECIMAL
- CHAR, VARCHAR, TEXT, BLOB
- DATE, TIME, TIMESTAMP, TIMESTAMP WITH TIMEZONE, INTERVAL
- UUID, JSON, ARRAY
- INET, CIDR, MACADDR
- POINT, LINE, POLYGON, BOX, CIRCLE
- INT4RANGE, INT8RANGE, NUMRANGE, TSRANGE, DATERANGE

### Error Codes (43+ codes) [Alpha]
- SB_OK, connection errors, query errors, transaction errors, resource errors, parameter errors, state errors, internal errors

---

## 12. TESTING (451 Features)

See [docs/specifications/testing/ALPHA3_TEST_PLAN.md](specifications/testing/ALPHA3_TEST_PLAN.md)

### Test Frameworks & Tools (20+ features) [Alpha]
- Google Test, Catch2, pgbench, sysbench
- OWASP ZAP, Nmap, sqlmap, fuzzing tools
- Prometheus, Grafana
- testssl.sh, openssl, mitmproxy
- hping3, hydra, Wireshark, gdb
- JUnit reporting, GitHub Actions CI/CD

### Protocol Compliance Testing (117+ test cases across 5 protocols) [Alpha]
- PostgreSQL protocol (45 tests: StartupMessage, auth, SSL, query protocols, type serialization)
- MySQL protocol (20 tests: handshake, auth, commands, binary protocol)
- TDS protocol (16 tests: PRELOGIN, LOGIN, SQL_BATCH, RPC, transactions)
- Firebird protocol (16 tests: op_connect/attach, auth, statement lifecycle, BLOBs, ARRAYs)
- Native protocol (21 tests: STARTUP, cluster PKI, compression, streaming, federation, types)

### Authentication Testing (27+ tests across 11 methods) [Alpha]
- Password, MD5, SCRAM-SHA-256
- mysql_native_password, caching_sha2_password
- NTLM, Kerberos, LDAP
- Certificate (mTLS), SAML, OAuth 2.0, MFA (TOTP)

### Authorization Testing (10+ tests) [Alpha]
- SELECT/INSERT permissions
- RBAC, GBAC
- Row-level security, column-level security
- Schema permissions
- GRANT/REVOKE, DENY override

### Load Testing (35+ scenarios) [Alpha]
- Connection load (7 scenarios: concurrent, storm, cycling, limits, long-lived, idle timeout)
- Query load (8 scenarios: simple SELECT, complex JOIN, INSERT, UPDATE, mixed OLTP, prepared statements, large result sets, concurrent transactions)
- Protocol-specific load (8 tests: pgbench, COPY, sysbench, bulk INSERT, BCP, gbak, SBLR, federation)
- Stress tests (7 tests: CPU, memory, disk I/O, network, mixed protocol storm, checkpoint, GC under load)
- Endurance tests (5 tests: continuous OLTP, connection cycling, log rotation, statistics, config reload)

### Security Penetration Testing (70+ tests) [Alpha]
- Network security (10 tests: port scanning, fingerprinting, TLS vulnerabilities, protocol downgrade, replay attacks)
- Authentication attacks (10 tests: brute force, credential stuffing, timing attacks, session hijacking)
- SQL injection (10 tests: classic, blind, time-based, UNION-based, stacked queries, prepared statement bypass)
- Protocol fuzzing (10 tests: all protocols, malformed/oversized/truncated packets)
- Authorization bypass (10 tests: privilege escalation, IDOR, role confusion, RLS bypass)
- DoS attacks (10 tests: connection/memory/CPU/disk exhaustion, slow queries, lock contention)
- Data protection (10 tests: sensitive data in logs/errors/dumps, backups, encryption, password storage)

### Performance Benchmarking (28+ measurements) [Alpha]
- Connection performance (5: latency, SSL, auth, pool acquire, max connections/sec)
- Query performance (8: point SELECT, range SELECT, complex JOIN, aggregate, INSERT, batch INSERT, UPDATE, DELETE)
- Throughput benchmarks (5: TPC-B, TPC-C, bulk COPY, bulk SELECT, mixed OLTP)
- Latency percentiles (5: p50, p90, p99, p99.9 for SELECT/range/INSERT/UPDATE/transaction)
- Scalability benchmarks (5: connection/CPU/data/concurrent/buffer pool scaling)

### Test Automation (40+ features) [Alpha]
- GitHub Actions workflows
- Matrix strategy for protocols
- Self-hosted performance runners
- Test artifact upload
- Automated reporting
- CSV/JSON reports
- Test graphs
- JUnit XML output

### Test Data Management (7+ datasets) [Alpha]
- Tiny (1MB, 10K rows), Small (100MB, 1M), Medium (1GB, 10M), Large (10GB, 100M), Huge (100GB, 1B)
- Test user accounts
- Realistic data generation

### Implementation Standards Testing (37+ requirements) [Alpha]
- Foundation audit, catalog verification
- Specification reading verification
- Happy path tests
- Restart/persistence tests (MANDATORY)
- Negative/error tests
- Multi-path tests (SQL, API, executor)
- Concurrency tests
- Integration tests (security, audit, resolver)
- Code quality verification
- Completion verification with evidence

---

## 13. SCHEDULER (350+ Features)

See [docs/specifications/scheduler/ALPHA_SCHEDULER_SPECIFICATION.md](specifications/scheduler/ALPHA_SCHEDULER_SPECIFICATION.md)

### Core Scheduling (45+ features) [Alpha]
- Cron-based scheduling (5-field expressions with wildcards, steps, ranges)
- One-time jobs (AT timestamp)
- Recurring jobs with automatic next run calculation
- Cron expression validation
- Next run time calculation

### Job Types (18+ features) [Alpha]
- SQL jobs (execute SQL within transaction)
- Procedure jobs (call stored procedures with parameters)
- External jobs (execute shell commands/scripts with timeout)

### Job State Management (16+ features) [Alpha]
- ENABLED, DISABLED, PAUSED states
- Automatic state transitions (one-time to DISABLED after execution)
- ALTER JOB SET STATE syntax

### Job Run States (16+ features) [Alpha]
- PENDING, RUNNING, COMPLETED, FAILED, CANCELLED states
- State tracking with timestamps
- State transitions

### Job Dependencies (10+ features) [Alpha]
- DEPENDS ON clause for single/multiple dependencies
- Dependency verification before execution
- Circular dependency detection
- DAG-based job dependencies

### Failure Handling & Retry (18+ features) [Alpha]
- Automatic retry on failure
- Configurable max retry count (default 3)
- Exponential backoff (base * 2^retry_count)
- Track retry count per run
- Capture error codes and messages
- Alert on max retries exceeded

### Catalog Schema (50+ features) [Alpha]
- sys.jobs table (20+ fields)
- sys.job_runs table (15+ fields)
- sys.job_dependencies table
- Foreign key constraints
- Indexes on next_run_time, state, job_uuid, started_at

### SQL Syntax (48+ features) [Alpha]
- CREATE JOB with all clauses (SCHEDULE, DEPENDS ON, MAX_RETRIES, RETRY_BACKOFF, DESCRIPTION, AS/CALL/EXEC)
- ALTER JOB (SET STATE, SET SCHEDULE, SET MAX_RETRIES, SET RETRY_BACKOFF)
- DROP JOB (IF EXISTS, CASCADE, RESTRICT, KEEP HISTORY)
- EXECUTE JOB for manual execution
- Query job status (sys.jobs, sys.job_runs, sys.job_dependencies)

### Scheduler Thread (30+ features) [Alpha]
- Background scheduler thread
- Polling loop (default 10s, configurable)
- Query enabled jobs due to run
- Create JobRun entries
- Job execution queue
- Update next run time for recurring jobs
- Check max concurrent jobs limit
- Spawn worker threads
- Track active job count

### Configuration (18+ features) [Alpha]
- scheduler.enabled, scheduler.polling_interval_seconds, scheduler.max_concurrent_jobs
- scheduler.job_timeout_seconds, scheduler.job_history_retention_days
- scheduler.default_max_retries, scheduler.default_retry_backoff_seconds
- Runtime control via ALTER SYSTEM SET

### Security (19+ features) [Alpha]
- Execute with creator's privileges
- Enforce CREATE JOB privilege
- Enforce ownership for ALTER/DROP JOB
- Cluster_admin role for ALTER/DROP
- Audit logging (JOB_CREATED, JOB_EXECUTED, JOB_MODIFIED, JOB_DELETED)

### Beta-Specific Features (32+ features for cluster scheduler) [Beta]
- Job classes (LOCAL_SAFE, LEADER_ONLY, QUORUM_REQUIRED)
- Partition rules (ALL_SHARDS, SINGLE_SHARD, SHARD_SET, DYNAMIC)
- Distributed execution with Raft-based control plane
- Scheduler agents on each node
- Job run assignment to nodes and shards
- Forward-compatible schema

### Testing (50+ features) [Alpha]
- Unit tests (cron parsing, next run time, job execution, privilege enforcement, retry logic)
- Integration tests (end-to-end scheduling, dependencies, state transitions, concurrent jobs)
- Restart/persistence tests
- Performance tests (scheduling overhead, execution latency, throughput)

---

## 14. CORE ENGINE (677 Features)

See [docs/specifications/core/README.md](specifications/core/README.md)

### Y-Valve Architecture (Future Phase 2+) (68+ features) [Alpha]
- Multi-protocol detection and routing
- 5 wire protocol parsers (PostgreSQL, MySQL, Firebird, TDS, Native)
- SQL dialect translation for each protocol
- Protocol-specific authentication
- Connection pooling by protocol/database/username
- Fast-path query execution
- Cross-protocol query result consistency
- Process-per-connection architecture
- Cross-platform socket handoff

### Thread Safety & Concurrency (18+ features) [Alpha]
- Immutable data structures, thread-local storage, RW locks, mutexes, lock-free atomics
- Page I/O serialization, buffer pool concurrency control
- TIP RW locking, catalog cache RW locking
- Global lock ordering, deadlock prevention

### Process & Memory Management (18+ features) [Alpha]
- Single-threaded engine core
- Multi-threaded execution
- Process-per-connection architecture
- Buffer pool management with multiple instances
- Ring buffers for scans/VACUUM/bulk writes
- Adaptive hash index, young/old LRU lists
- Read-ahead, direct I/O, async I/O

### Design Limits & Constraints (67+ features) [Alpha]
- Page sizes (8KB, 16KB, 32KB, 64KB, 128KB)
- Max database sizes (32TB to 512TB depending on page size)
- 32-bit page IDs (4.29B pages max, future 64-bit)
- Table/column limits (max 4096 columns, practical 1024, recommended <200)
- TOAST support (up to 4GB theoretical, 1GB practical)
- VARCHAR/TEXT/BLOB/JSON/JSONB/VECTOR limits
- SQL standard 128-character identifiers
- 64-bit transaction IDs (no wraparound)
- Buffer pool limits (min 32 pages to very large 65536 pages)

### Live Migration & Passthrough (189+ features) [Beta]
- Migration states (NOT_STARTED, BULK_LOADING, SYNCHRONIZING, DUAL_WRITE, CUTOVER_READY, LOCAL_ONLY, ROLLBACK, PAUSED, ERROR)
- Query routing (post-semantic-analysis interception, per-table routing, transaction-aware)
- Hybrid query execution (cross-system JOINs with multiple strategies)
- Bulk loading (configurable batch size, parallel workers, rate limiting, resumable)
- CDC (PostgreSQL logical replication, MySQL binlog, SQL Server CDC, Firebird triggers)
- Dual-write coordination (2PC, conflict detection, 5 conflict strategies)
- Cutover & validation (row count, checksum, constraint/index validation, quiesce writes)
- Migration monitoring (Prometheus metrics, Grafana dashboards)
- Migration security (encrypted credentials, SSL/TLS, audit logging)

### Git Metadata Integration (122+ features) [Alpha]
- Git repository management (init, URL config, branch management, pull/push, SSH keys)
- Schema export (full/specific, file-per-object, separate indexes, grants/comments/defaults)
- Schema import (from specific branch, dry-run, conflict resolution strategies)
- Migration management (script generation, versioned/timestamp/sequential naming, up/down sections, pending/history display, rollback)
- DDL change tracking (automatic capture, SYS$DDL_HISTORY, Git commit association)
- Schema diff & comparison (database-to-Git, branch-to-branch, environment comparison)
- Environment management (dev/staging/production-specific config, auto-apply vs approval)
- CI/CD integration (GitHub Actions, GitLab CI, migration validation, deployment)
- Git audit (full DDL change trail, compliance reports)
- Git access control (7 role permissions: GIT_ADMIN, GIT_EXPORT, GIT_IMPORT, GIT_MIGRATE, GIT_ROLLBACK, GIT_PUSH, GIT_PULL)

### Internal Functions (47+ features) [Alpha]
- Temporal functions (NOW, CURRENT_TIMESTAMP, CURRENT_DATE, CURRENT_TIME, DATE_ADD, DATE_SUB, DATE_DIFF, AT TIME ZONE)
- String functions (LTRIM, RTRIM, CONCAT, CONCAT_WS)
- JSON/JSONB functions (text-based and binary CBOR)
- Array functions (ARRAY_AGG, statistics)
- Spatial functions (ST_POINT, ST_MAKELINE, ST_MAKEPOLYGON, ST_ASTEXT, ST_ASBINARY, ST_GEOMETRYTYPE, ST_ISVALID)

### Core Data Structures (51+ features) [Alpha]
- YValveConnectionHandoff structure
- ConnectionContext with state machine (8 states)
- Migration structures (state, checkpoints, conflicts, history, CDC positions) [Beta]
- Git structures (SYS$DDL_HISTORY, SYS$MIGRATIONS, SYS$MIGRATION_LOCK, SYS$GIT_CONFIG, SYS$GIT_SYNC_HISTORY)

### Core Algorithms (23+ features) [Alpha]
- Protocol detection (confidence scoring 0.0-1.0)
- Query routing (decision matrix, cross-system JOIN detection, pushdown feasibility) [Beta]
- Migration processing (state machine, bulk load, CDC streaming, conflict detection) [Beta]
- Schema comparison (object diff, column/constraint/index/permission diff)

### Configuration & Tuning (51+ features) [Alpha]
- Migration configuration (20+ settings: batch sizes, parallel workers, memory limits, CDC settings, validation settings, conflict strategy) [Beta]
- Performance tuning (batch sizing, worker parallelism, rate limiting, index strategies, CDC management)

### Error Handling & Recovery (36+ features) [Alpha]
- Error categories (11 types: CONNECTION, AUTHENTICATION, PERMISSION, SCHEMA_MISMATCH, etc.)
- Retry logic (max retries, exponential backoff, retryable error detection)
- Recovery procedures (crash recovery, resume, state repair, skip/baseline) [Beta]
- Data validation & repair (mismatch detection, 4 repair strategies) [Beta]

### Implementation Recommendations (85+ features) [Alpha]
- Index implementation (7 types: B-Tree with compression, UUID v7 optimized, Hash, Bitmap, GIN, Columnstore, Adaptive hash)
- Network layer (13 enhancements: pooling, event notification, SSL/TLS, multiplexing, query caching, zero-copy, compression, connection migration)
- Query optimizer (15 features: statistics, multi-column stats, cost model, DP join ordering, genetic algorithm, parallel execution, adaptive execution, plan caching, EXPLAIN, ML cost model, hints, auto-tuning)
- Storage engine (14 features: MGA, TIP, parallel VACUUM/sweep, compression, encryption, tiered storage, multi-page-size, FSM, TOAST, visibility maps, read-ahead, ring buffers)
- Transaction & lock management (33 features: 64-bit TxnIDs, 4 isolation levels, predicate locking, 9 lock granularities, 8 lock modes, deadlock detection methods, lock escalation, OCC, 2PC, savepoints, nested transactions, advisory locks, distributed coordination)

---

## 15. REMOTE DATABASE UDR (438 Features)

See [docs/specifications/remote_database_udr/README.md](specifications/remote_database_udr/README.md)

### Connection Pool (73+ features) [Beta]
- Configurable pool parameters (min/max size, timeouts, lifetimes, validation)
- Thread-safe pool operations
- Connection validation (acquire/release/background)
- Health checking (PING, QUERY, FULL_VALIDATE)
- Pool statistics (30+ metrics)
- Per-user connection pooling

### Remote Database Registration (40+ features) [Beta]
- REGISTER REMOTE DATABASE syntax
- Protocol specification (postgresql, mysql, mssql, firebird, scratchbird)
- Connection parameters (host, port, database, username, password from file/env)
- Pool configuration options
- SSL/TLS options (mode, CA/client cert/key, verification)
- Query pushdown/result cache options
- Server configuration modification (ALTER SERVER)

### Foreign Table Management (33+ features) [Beta]
- CREATE/DROP/ALTER FOREIGN TABLE
- Column name mapping, remote table name option
- Updatable flag, estimated row count/startup cost hints
- Fetch size configuration

### User Mapping (15+ features) [Beta]
- CREATE/ALTER/DROP USER MAPPING
- For specific user, PUBLIC, CURRENT_USER
- Remote username/password configuration
- Authentication types (password, kerberos, certificate)

### Schema Introspection (62+ features) [Beta]
- Discover schemas, tables, detailed table information
- Extract column metadata (40+ attributes)
- Extract index information
- Extract foreign keys, check constraints, primary keys
- Partition information
- View definitions
- Schema import (IMPORT FOREIGN SCHEMA with LIMIT TO/EXCEPT)
- Import options (views, materialized views, collations, constraints, defaults)
- Table/column filtering, type mapping overrides

### Query Operations (51+ features) [Beta]
- SELECT/INSERT/UPDATE/DELETE on foreign tables
- WHERE/ORDER BY/LIMIT/OFFSET pushdown
- GROUP BY/DISTINCT pushdown
- JOIN pushdown (same server)
- Cross-server JOIN (local join)
- Query cost estimation
- Prepared statement support
- Cursor support for large result sets
- Batch operations
- Query hints (NO_PUSHDOWN, FETCH_SIZE, USE_CURSOR, TIMEOUT)
- Pass-through execution (remote_exec, remote_query, remote_call)

### Type Conversion (54+ features) [Beta]
- Type mapping for 40+ types per database (PostgreSQL, MySQL, MSSQL, Firebird)
- NULL handling, type overflow detection, precision/scale preservation
- Character set conversion (UTF-8), timezone handling

### Database-Specific Protocols (64+ features) [Beta]
- PostgreSQL client (wire protocol 3.0, libpq, password/SCRAM/SSL/certificate auth, version 9.6-17.x)
- MySQL client (wire protocol, libmysqlclient, native password/caching_sha2_password auth, 5.7/8.0+/MariaDB 10.x)
- MSSQL client (TDS 7.0-7.4, FreeTDS, SQL Server/Windows/Azure AD auth, 2016-2022/Azure SQL)
- Firebird client (wire protocol, fbclient, legacy/SRP/Win_Sspi auth, 2.5/3.0/4.0/5.0)
- ScratchBird federation (cluster PKI auth)

### Migration Workflows (42+ features) [Beta]
- Migration strategies (Big Bang, Incremental, Parallel Run, Strangler Fig)
- Replication streaming, watermark-based sync
- Conflict resolution (4 strategies)
- Row count/checksum validation
- Migration progress tracking
- CREATE REPLICATION STREAM syntax
- Replication lag monitoring

### Performance Optimization (22+ features) [Beta]
- Result set caching (TTL, LRU, cache invalidation)
- Query pushdown cost-based decisions
- Network latency measurement, selectivity estimation
- Query plan caching, statistics collection
- Batching and streaming (configurable batch/fetch sizes, cursor-based fetching)

### Monitoring & Administration (34+ features) [Beta]
- System views (foreign_servers, foreign_tables, user_mappings, remote_pool_stats, remote_connections)
- Administrative commands (SHOW, warmup_pool, evict_idle_connections, close_all_connections, refresh_foreign_schema)
- Statistics (20+ metrics: connections, acquires, waits, queries, cache hit ratio)

### Security (27+ features) [Beta]
- Authentication methods (10+ methods)
- SSL/TLS encrypted connections, certificate verification
- Per-user authentication mapping, least privilege
- Kerberos, Azure AD, SCRAM-SHA-256 support
- Network security (VPN, firewall, IP allowlist)
- Query timeout as DoS prevention

### Error Handling (20+ features) [Beta]
- Error codes (11 codes: RD001-RD033)
- Automatic retry on transient failure, configurable retry count/delay
- SQLSTATE error code mapping

### Testing & Validation (16+ features) [Beta]
- Connection validation query, periodic health checks
- Custom health check query
- Row count/checksum comparison
- Schema/constraint/performance validation

---

## 16. DRIVERS (750+ Features, 11 Languages)

See [docs/specifications/beta_requirements/drivers/README.md](specifications/beta_requirements/drivers/README.md)

### Baseline Requirements (All Drivers) (50+ features) [Beta]
- ScratchBird Native Wire Protocol (SBWP) v1.1
- TCP transport (port 3092), Unix domain sockets
- TLS 1.3 required, server/client certificate auth
- SCRAM-SHA-256 authentication (required)
- Legacy password, certificate, OAuth2/OIDC, MFA TOTP support
- Simple QUERY and extended query (PARSE/BIND/EXECUTE)
- SBLR_EXECUTE for precompiled bytecode (optional)
- CANCEL with MSG_FLAG_URGENT
- Streaming for large result sets
- Positional ($1, $2) and named (:name) placeholders
- Binary encoding (default), text encoding
- SQLSTATE code preservation
- Error field mapping (message, detail, hint, position)
- Implicit transaction after AUTH_OK
- Autocommit mode
- Timezone handling
- Statement cache (configurable size)
- Batch execution (where supported)
- Compression negotiation (zstd)

### Connection Management (25+ features) [Beta]
- URI connection string (scratchbird://...)
- Key-value connection string (optional)
- Connection options (18+ parameters: host, port, database, user, password, SSL mode/certs, timeouts, application_name, search_path, binary_transfer, compression)
- Open/close/disconnect/reconnect
- Ping/health check, reset session
- Connection pooling (built-in or language-standard)

### Transaction Control (20+ features) [Beta]
- Begin/commit/rollback transaction
- Set/get isolation level
- Autocommit mode on/off
- Savepoints (create, release, rollback to)
- Transaction blocks with auto commit/rollback

### Statement Execution (60+ features) [Beta]
- Simple query (SELECT, DML, DDL)
- Get row count, last insert ID
- Execute without results, query scalar value
- Prepared statements (prepare, bind by index/name, execute, close, reuse, cache)
- Bind NULL and all typed parameters (40+ types)
- Batch operations (add to batch, execute batch, handle partial failure)
- Set query timeout, fetch size, cancel statement

### Result Set Handling (60+ features) [Beta]
- Check has next row, fetch next/single/multiple/all
- Close result set
- Get value by column index/name
- Get column count/name/type
- Check if NULL
- Get typed values (40+ type accessors for all ScratchBird types)
- Get column metadata
- Support multiple result sets

### Type Mapping (80+ features for all types) [Beta]
- NULL, BOOLEAN, INT8-INT128, UINT8-UINT128, FLOAT32/FLOAT64, DECIMAL, MONEY
- CHAR, VARCHAR, TEXT, BINARY, VARBINARY, BLOB/BYTEA
- DATE, TIME, TIMESTAMP, INTERVAL
- UUID, JSON, JSONB, XML
- ARRAY, RANGE, COMPOSITE/ROW, VARIANT, VECTOR
- TSVECTOR, TSQUERY
- INET, CIDR, MACADDR, MACADDR8
- GEOMETRY, POINT, LINESTRING, POLYGON, MULTIPOINT, MULTILINESTRING, MULTIPOLYGON, GEOMETRYCOLLECTION
- Round-trip without loss for all types

### Error Handling (30+ features) [Beta]
- SQLSTATE class mapping (14 classes: 01, 02, 08, 0A, 22, 23, 28, 40, 42, 53, 54, 57, 58, XX)
- Error information access (SQLSTATE, message, detail, hint, position, context, severity)

### Language-Specific Implementations (300+ features across 11 languages) [Beta]

**C++ Driver (libscratchbird_cpp):** RAII classes, C++17, C API wrapper (35+ exported functions), std::chrono, boost::multiprecision, std::optional

**NET/C# Driver (ScratchBird.Data):** ADO.NET DbProviderFactory, DbConnection, DbCommand, DbParameter, DbDataReader, DbTransaction, DbDataAdapter, .NET 6.0+, Entity Framework Core, DateOnly/TimeOnly, Int128/UInt128

**Go Driver (github.com/scratchbird/scratchbird-go):** database/sql Driver/Connector/Conn/Stmt/Tx/Rows interfaces, context.Context, GORM/sqlx integration, *big.Int, decimal.Decimal, uuid.UUID, net.IP/net.IPNet

**Java JDBC Driver (org.scratchbird:scratchbird-jdbc):** JDBC 4.2+ Type 4 driver, java.sql.Driver, javax.sql.DataSource, Connection/Statement/PreparedStatement/CallableStatement/ResultSet/ResultSetMetaData/DatabaseMetaData interfaces, BigInteger/BigDecimal, LocalDate/LocalTime/OffsetDateTime, Hibernate/JPA integration

**Node.js/TypeScript Driver (scratchbird):** Promise-based async Client/Pool classes, TypeScript type definitions, ES6+, bigint, Buffer/Uint8Array, Date, Float32Array, Sequelize/TypeORM/Prisma integration

**Pascal/Delphi Driver (scratchbird-pascal):** FireDAC/IBX/Zeos/SQLdb adapters, TFDConnection/TZConnection/TSQLConnection components, TGUID, TDateTime, TBytes, Currency/Extended, Firebird migration toolkit

**PHP Driver (scratchbird/pdo-scratchbird):** PDO driver, mysqli compatibility layer, PDOStatement methods, FETCH_ASSOC/NUM/BOTH/OBJ modes, DateTimeImmutable, bcmath string decimals, WordPress/Laravel/Doctrine integration

**Python Driver (scratchbird):** PEP 249 DB-API 2.0, apilevel="2.0", threadsafety=2, paramstyle="named", Connection/Cursor/AsyncConnection/AsyncCursor classes, int (arbitrary precision), decimal.Decimal, datetime classes, uuid.UUID, bytes/memoryview, ipaddress, SQLAlchemy/Django/Pandas/NumPy integration

**R Driver (DBI + scratchbird):** DBI-compatible, dbConnect/dbExecute/dbGetQuery/dbFetch generics, logical, integer, bit64::integer64, numeric, Date, POSIXct, hms::hms, difftime, dplyr/dbplyr/sf integration

**Ruby Driver (scratchbird):** Ruby-native API, DBI/Sequel/ActiveRecord adapters, TrueClass/FalseClass, Integer (bignum), BigDecimal, Float, String (UTF-8/BINARY), Date/Time/DateTime, IPAddr, Hash/Array, Range, Rails integration

**Rust Driver (scratchbird):** Async-first (Tokio), blocking wrapper, async Client/Statement/Transaction/Row structs, i8-i128/u8-u128, rust_decimal::Decimal, f32/f64, String, Vec<u8>, chrono types, uuid::Uuid, serde_json::Value, std::net::IpAddr, ipnet::IpNet, Option<T>, sqlx/diesel integration

### Testing & Validation (125+ features) [Beta]
- Protocol tests (TLS handshake, certificate validation, STARTUP/AUTH flows, QUERY/PARSE/BIND/EXECUTE, CANCEL, timeouts)
- Type round-trip tests (40+ types)
- API compliance tests (30+ tests for statement cache, parameter binding, prepared statements, pooling, transactions, savepoints, autocommit, result iteration, batch execution)
- Error handling tests (14 SQLSTATE classes, error field preservation, connection/query/transaction errors)
- Performance tests (connection establishment, query latency, throughput, streaming, binary vs text, compression, cache hit rate, pool efficiency, benchmarks vs native drivers)
- Integration tests (18 ORM/framework integrations: SQLAlchemy, Django, Pandas, EF Core, Hibernate, JPA, GORM, sqlx, Sequelize, TypeORM, Prisma, ActiveRecord, Rails, Laravel, Doctrine, dplyr, sqlx, diesel)
- Stress tests (connection churn, leak detection, memory usage, concurrent queries, long-running queries, cancellation, deadlocks, recovery after failures)

---

## 17. CONNECTIVITY (286 Features)

See [docs/specifications/beta_requirements/connectivity/README.md](specifications/beta_requirements/connectivity/README.md)

### ODBC Driver (140+ features) [Beta]
- ODBC 3.5/3.8 specification compliance, SQL-92/SQL-99
- Platform support (Windows 10/11/Server 2016+, Linux Ubuntu/RHEL/Debian/Fedora/CentOS, macOS, 32-bit/64-bit)
- Connection functions (SQLAllocHandle, SQLConnect, SQLDriverConnect, SQLBrowseConnect, SQLDisconnect, SQLFreeHandle)
- Connection strings (DSN-based and DSN-less with 20+ parameters)
- Statement execution (SQLPrepare, SQLExecute, SQLExecDirect, SQLNumResultCols, SQLRowCount)
- Data retrieval (SQLFetch, SQLFetchScroll with FORWARD/BACKWARD/ABSOLUTE/RELATIVE, SQLGetData, SQLBindCol, SQLMoreResults)
- Transaction management (SQLEndTran, SQLSetConnectAttr for auto-commit/isolation level)
- Metadata functions (SQLTables, SQLColumns, SQLPrimaryKeys, SQLForeignKeys, SQLStatistics, SQLProcedures, SQLGetTypeInfo)
- Diagnostic functions (SQLGetDiagRec, SQLGetDiagField, SQLError)
- Data type mappings (20+ SQL_* types to ScratchBird types)
- Character encoding (Unicode, UTF-8, UTF-16)
- Cursor support (forward-only, scrollable)
- Connection pooling (built-in, driver-manager integration)
- SSL/TLS support (6 SSL modes, client certificate auth)
- DSN configuration (User/System DSN, GUI configuration, ODBC Data Source Administrator, unixODBC registration, odbcinst.ini/odbc.ini, isql testing)
- Driver manager compatibility (Windows ODBC, unixODBC, iODBC)
- Native ScratchBird wire protocol (port 3092, network-only)
- Package installation (MSI installer for Windows with silent install/signing/WHQL, DEB packages for Ubuntu/Debian, RPM packages for RHEL/Fedora/CentOS, macOS package, automatic registration, Group Policy deployment)

### BI Tool Integration (35+ features) [Beta]
- Microsoft Excel (Data Connection .odc files, Microsoft Query, PivotTable, Power Query, data refresh, load to worksheet/PivotTable, Excel 2016+)
- Power BI Desktop (connector via ODBC, DirectQuery/Import modes, query folding, incremental refresh, DAX query, Power BI Service/gateway, certification)
- Tableau (Desktop connector Generic ODBC/custom, live connection/extract, custom SQL, Tableau Server, 2021+, connector certification)
- Other BI/reporting tools (SSRS, Crystal Reports, JasperReports, QlikView/Qlik Sense, Business Objects, Informatica, Talend, Pentaho)
- Database tools (DBeaver, DataGrip, SQuirreL SQL, Microsoft Access, LibreOffice Base)

### JDBC Driver (10+ features) [Beta]
- JDBC 4.2 compliance, Type 4 (pure Java)
- PreparedStatement, CallableStatement
- ResultSetMetaData, complete metadata
- Connection pooling, standard pooling support

### Performance & Testing (30+ features) [Beta]
- Performance benchmarks (9 benchmarks: connection, simple SELECT, bulk SELECT, prepared statement, bulk INSERT, metadata, Excel refresh, Power BI DirectQuery - all within 10-20% of native drivers)
- Testing requirements (connection/disconnect/reconnect, query tests, transactions, metadata, compatibility with tools, ODBC compliance test suite, Microsoft ODBC Test, unixODBC tests, Tableau/Power BI/Excel tests, concurrent connections, memory leak tests, >90% compliance score)

### Documentation & Examples (45+ features) [Beta]
- Quick start guide, installation instructions (Windows/Linux/macOS)
- Connection string reference, DSN configuration guide (GUI/CLI)
- Integration guides (Excel, Power BI, Tableau)
- Troubleshooting guide (connection issues, performance)
- Performance tuning, deployment guide (enterprise)
- Security best practices, SSL/TLS configuration, connection pooling configuration
- Complete ODBC function reference, driver-specific extensions
- Connection/statement attribute reference
- Migration guides (from PostgreSQL/MySQL/SQL Server/Firebird ODBC)
- DSN/connection string migration
- Application compatibility notes
- Code examples (Python pyodbc, C# System.Data.Odbc, C/C++ ODBC, prepared statements, transactions, metadata, Excel/Power BI/Tableau)

---

## 18. ORM/FRAMEWORKS (12 Frameworks)

See [docs/specifications/beta_requirements/orms-frameworks/README.md](specifications/beta_requirements/orms-frameworks/README.md)

### P0 - Critical (Beta Required) - 4 ORMs [Beta]
**SQLAlchemy (Python):** Dialect implementation, Core expression language, ORM layer, Async/await, Alembic integration, connection pooling, reflection/introspection

**Sequelize (Node.js):** Dialect implementation, Migrations, TypeScript type definitions, Async/await, Query interface, Model definition/validation

**Hibernate/JPA (Java):** JPA 2.2+ compliance, Hibernate dialect, Spring Data JPA, Entity mapping, JPQL, Criteria API, Flyway/Liquibase integration

**Entity Framework Core (.NET):** Database provider implementation, Migrations, LINQ, DbContext, Model relationships/navigation, Async query, Code-first/Database-first, Fluent API

### P1 - High Priority - 5 ORMs [Beta]
**TypeORM (TypeScript):** Full TypeScript support, Decorators, Metadata reflection, Migrations, Active Record/Data Mapper patterns, Repository, Query Builder, Async/await

**Prisma (Node.js/TypeScript):** Prisma Client generation, Prisma Migrate, Prisma Schema SDL, Type-safe client, Automatic migrations, Introspection, Async query, Relation queries

**Rails ActiveRecord (Ruby):** ActiveRecord adapter, Migrations, Rails integration, Model associations, Validations, Callbacks, Query interface, Schema definition

**Laravel Eloquent (PHP):** Database driver integration, Query builder, Migrations, Eloquent ORM models, Relationships, Eager/lazy loading, Model events/observers

**Dapper (.NET):** Micro-ORM support, Performance optimization, Query execution, Parameter binding, Multi-mapping, Async query, Stored procedure support

### P2 - Medium Priority - 1 ORM [Beta]
**Django ORM (Python):** Django database backend, Migrations, Admin interface, QuerySet API, Model definitions, Model relationships, Async query, Custom managers

### Specialized Frameworks - 2 [Beta]
**Cypher/OpenCypher (Graph):** Cypher query language, OpenCypher compliance, Neo4j compatibility layer, Pattern matching, Graph traversal, Node/relationship queries, Path finding, Graph algorithms

**Gremlin/TinkerPop (Graph):** Gremlin graph traversal language, Apache TinkerPop integration, Graph traversal steps, Vertex/edge operations, Graph algorithms, OLTP/OLAP support

### Cross-Cutting Features (All ORMs) [Beta]
- CRUD operations (CREATE, READ, UPDATE, DELETE)
- Relationships (one-to-one, one-to-many, many-to-many)
- Query capabilities (simple queries, complex queries with joins, aggregations, filtering, ordering, pagination)
- Transaction management (begin, commit, rollback, nested transactions, savepoints)
- Schema management (creation, migrations, version control, rollback, introspection/reflection)
- Performance (benchmarks against PostgreSQL/MySQL/SQL Server, connection pooling, query optimization)
- Async support (where applicable: async queries/transactions, connection pooling with async)

---

## 19. STORAGE ENGINE & ON-DISK FORMAT

See [docs/specifications/storage/README.md](specifications/storage/README.md) and [docs/specifications/storage/ON_DISK_FORMAT.md](specifications/storage/ON_DISK_FORMAT.md)

### On-Disk Page Layout [Alpha]
- Page header magic 0x53425244 ('SBRD') and version validation
- CRC32C checksum (Castagnoli) excluding checksum field bytes
- Page size validation (8KB, 16KB, 32KB, 64KB, 128KB) and header page_size match
- Page type enum validation (heap, index, catalog, TIP, FSM)
- Page flags (DIRTY, PINNED, COMPRESSED, ENCRYPTED) persistence rules
- UUIDv7 database_uuid and table_id stored in header
- Heap pages require non-zero table_id (zero indicates corruption)
- LSN field semantics when WAL is disabled (0) vs enabled

### Heap & Tuple Storage [Alpha]
- Heap page layout with item pointer array and special area offsets
- TupleHeader fields (xmin, xmax, back_version_gpid/slot, ctid_gpid/slot, infomask)
- Null bitmap encoding (NULLs stored only in bitmap, no payload)
- Tuple data alignment to 8-byte boundaries
- Insert/update/delete with version chains and back-version pointers
- Deleted tuples mark xmax and are prunable by sweep
- get_tuple vs get_tuple_detoasted behavior for TOASTed tuples
- Tuple visibility checks follow MGA/TIP rules

### Buffer Pool & Page Management [Alpha]
- Shared buffer pool with pin/unpin and dirty tracking
- Page eviction policies (LRU variants, ring buffers) and reuse
- Free space map (FSM) tracking and allocation strategy
- Page allocation, deallocation, and page-type validation
- Page checksum validation on read and before flush
- Vacuum integration for heap and TOAST cleanup
- Multiple page size support across buffer pool and FSM

### Tablespaces & Storage Classes [Alpha]
- Tablespace creation with path validation and resolution
- Per-table tablespace assignment and catalog tracking
- Storage classes (NVMe/SSD/HDD/Archive/S3/Memory) metadata
- Tablespace permission checks and path normalization
- Tablespace free space accounting and reporting

### TOAST & LOB Storage [Alpha]
- ToastPointer (18 bytes) with 0x01 marker and value/toast IDs
- ToastChunk header (TupleHeader + value_id/chunk_seq/chunk_size)
- Per-page TOAST thresholds and chunk sizing via ToastSettings
- Strategy selection (PLAIN/EXTENDED/COMPRESSED/EXTERNAL)
- TIP-based visibility for TOAST chunks (xmin committed, xmax not)
- Detoast on read and IndexKeyExtractor detoast for index keys
- TOAST garbage collection (orphan detection + committed delete sweep)
- TOAST table naming scheme `pg_toast_<UUID>`

### Extended Page Sizes [Alpha]
- 8KB/16KB/32KB/64KB/128KB page support
- Extended ItemPointer format for >64KB pages (32-bit offsets)
- Extended HeapPageSpecial format with 32-bit lower/upper offsets
- ToastSettings derived from page size (threshold/target/chunk size)
- Page size validation and compatibility checks at open

### Storage Integrity & Recovery [Alpha]
- Database header validation (magic, page_id=0, block size match)
- CRC32C verification for page reads and writes
- TIP-based crash recovery (active transactions -> aborted)
- Corruption detection for invalid offsets/page sizes
- Validation tooling for on-disk structures (page/header/tuple checks)

---

## 20. INDEXES & ACCESS METHODS

See [docs/specifications/indexes/README.md](specifications/indexes/README.md)

### Index Architecture [Alpha]
- Index metadata and catalog integration (type, columns, uniqueness, predicates)
- Index build from heap scan and bulk load/sort
- Index rebuild and shadow index versioning
- Index scan APIs (point, range, full scan, ordered scan)
- Composite keys, included columns, and partial indexes
- Index key extraction and TOAST detoasting for varlen columns
- MGA visibility rules for index entries (xmin/xmax)

### Index Types [Alpha]
- B-tree (unique, composite, descending, covering)
- Hash (equality lookups)
- GiST (extensible framework for complex keys)
- GIN (inverted index for arrays/text search)
- BRIN (block range summaries)
- Bloom filter (set membership tests)
- Inverted index (full-text search)
- IVF (vector similarity search)
- Zone maps (min/max pruning)
- LSM tree (write-optimized storage)
- Columnstore (OLAP workloads)

### Index Maintenance & GC [Alpha]
- Index vacuum and garbage collection (tombstone removal)
- Cooperative heap/index GC protocol (MGA-aware)
- Reindex and shadow index rebuild workflows
- Index statistics collection and bloat tracking

---

## 21. DATA TYPES & CASTING

See [docs/specifications/types/README.md](specifications/types/README.md)

### Core Types [Alpha]
- Integer types (INT8/16/32/64, UINT8/16/32/64, INT128/UINT128)
- Floating types (FLOAT32/FLOAT64 IEEE 754)
- DECIMAL stored as scaled integer by precision (1/2/4/8/16 bytes)
- MONEY stored as int64 with implied scale
- BOOLEAN stored as 1 byte
- UUID stored as 16 raw bytes; UUIDv7 identity columns

### Text, Binary, and Collation [Alpha]
- Varlen encoding v1: uint32 length + raw bytes
- CHAR padding with spaces; BINARY padding with 0x00 bytes
- VARCHAR/BINARY length enforcement and truncation errors
- Character set and collation catalog definitions
- JSON/JSONB/XML stored as length-prefixed UTF-8 text (JSONB text in Alpha)
- BLOB/BYTEA stored as length-prefixed binary

### Temporal & Timezone [Alpha]
- DATE stored as int32 MJD + int32 offset_seconds
- TIME stored as int64 microseconds since midnight + int32 offset_seconds
- TIMESTAMP stored as int64 microseconds since epoch + int32 offset_seconds
- UTC normalization on input with original offset preserved
- `server.time.date_default_time` applied to DATE input
- Timezone catalog, parsing, and formatting rules
- Interval types and canonical formatting

### Spatial & Advanced Types [Alpha]
- Geometry types (POINT, LINESTRING, POLYGON, MULTI*) serialization via TypedValue
- TSVECTOR/TSQUERY binary encoding per toBinary implementations
- Arrays/composites stored as typed value lists with per-element payloads
- Range types with binary encoding and bounds

### Casting & Error Codes [Alpha]
- Canonical encoding rules and round-trip guarantees
- CAST/TRY_CAST conversion matrix (string <-> numeric/temporal/binary)
- Canonical text formats (UUID, dates, times, timestamps)
- Binary USING formats (hex/base64/escape)
- Numeric USING hexadecimal parsing/formatting
- SQLSTATE mapping for conversion errors (22P02, 22007, 22008, 22001)

---

## 22. PARSER & DIALECTS

See [docs/specifications/parser/README.md](specifications/parser/README.md)

### Grammar & Lexer [Alpha]
- ScratchBird SQL grammar (BNF) covering DDL/DML/transaction statements
- Tokenization of identifiers, keywords, numeric and string literals
- Blob literal parsing (X'...') and whitespace handling
- Quoted vs unquoted identifier rules and case folding
- Statement list parsing and statement terminator handling
- Error recovery with diagnostic spans and locations

### Parser Architecture [Alpha]
- Parser v2 AST node model and node typing
- Semantic analysis and type/domain resolution
- SBLR emission requirements with type modifiers and USING formats
- Parser remapping/implementation strategy for dialect alignment

### Emulated Dialects [Alpha]
- Firebird SQL dialect compatibility (DDL/DML surface)
- PostgreSQL dialect compatibility (DDL/DML surface)
- MySQL dialect compatibility (DDL/DML surface)
- Emulated system catalog mappings per dialect
- Dialect-specific keyword and feature gating
- Emulated parsers must hide ScratchBird-only features not supported by target DB

### Procedural SQL [Alpha]
- PSQL procedural language grammar
- DECLARE/BEGIN/END blocks with variable scope
- Control flow (IF/ELSIF/ELSE, LOOP/WHILE, EXIT/RETURN)
- Exception handling blocks and diagnostics

---

## 23. QUERY OPTIMIZER & PLANNER

See [docs/specifications/query/QUERY_OPTIMIZER_SPEC.md](specifications/query/QUERY_OPTIMIZER_SPEC.md)

### Plan Generation [Alpha]
- Logical plan construction from parsed query trees
- Physical plan selection (scan/join/aggregate/limit)
- Predicate pushdown and projection pruning
- BLR-aware plan caching for repeated statements

### Statistics & Costing [Alpha]
- Table and column statistics collection (n_tuples, n_pages, null_fraction)
- Histograms (equal-height/equal-width) and MCV lists
- Correlation and average width statistics
- Multi-column statistics (dependencies, ND histograms)
- Selectivity estimation using histogram + MCV
- Cost model for IO/CPU/network with staleness tracking
- ANALYZE and auto-analyze triggers

### Join Ordering & Access Paths [Alpha]
- Join order enumeration (DP/heuristics)
- Index selection and access path costing
- Merge/hash/nested-loop join choices
- Bitmap and index scan costing where applicable

### Plan Caching & EXPLAIN [Alpha]
- Plan cache for prepared statements
- EXPLAIN output formats and diagnostics
- Plan invalidation on schema changes
- Query feedback and adaptive re-planning hooks

---

## 24. SECURITY & AUTHENTICATION

See [docs/specifications/Security Design Specification/README.md](specifications/Security%20Design%20Specification/README.md)

### Authentication Frameworks [Alpha]
- Password, certificate, OAuth/OIDC, MFA, LDAP, Kerberos authentication
- External identity provider mapping and claims translation
- Password hashing policies (bcrypt/scrypt/argon2)
- Authentication policy configuration and enforcement

### Authorization & Policy [Alpha]
- Role-based and group-based access control
- Role composition and hierarchy rules
- Row-level and column-level security policies
- Privilege grant/revoke model with audit hooks

### Encryption & Key Management [Alpha]
- TLS configuration and cipher policies
- Key hierarchy and rotation policy
- Encrypted storage integration points and key custody
- Certificate management for server/client auth

### Audit & Compliance [Alpha]
- Audit event canonicalization
- Tamper-evident audit chains and verification checkpoints
- Compliance reporting requirements and retention rules

### Security Levels & Hardening [Alpha]
- Security levels and enforcement tiers
- Hardening guidance and configuration
- Network presence binding and session provenance
- Cluster security bundle and PKI controls [Beta]

---

## 25. BACKUP & RESTORE

See [docs/specifications/BACKUP_AND_RESTORE.md](specifications/BACKUP_AND_RESTORE.md)

### Backup Formats & Modes [Beta]
- Full backup format with database metadata
- Incremental backups based on page changes since last backup
- Differential backups based on changes since base full backup
- Backup chain/catalog tracking for restore sequencing

### Backup Process & Consistency [Beta]
- MGA snapshot isolation for consistent backups (no WAL replay)
- Non-blocking backup with concurrent transaction safety
- Page-level checksum capture for validation
- Streaming backup support for remote storage targets

### Restore & Validation [Beta]
- Full restore and table-level restore workflows
- Apply incremental/differential chains in order
- Backup verification and checksum validation without full restore
- Restore failure handling and rollback/retry procedures

### PITR & Temporal [Beta]
- Point-in-time recovery with transaction archive
- PITR positioned as lower priority alongside temporal tables

### Security & Encryption [Beta]
- Encrypted backups with key management integration
- Access control and audit trails for backup operations
- Backup compression options and verification

### Operational Controls [Beta]
- Backup scheduling hooks and retention policies
- Progress reporting, metrics, and observability hooks
- Error handling and retry policies

### Cluster Backup & Restore [Beta]
- Cluster-aware backup and restore coordination
- Node-level snapshot consistency and validation

---

## 26. DEPLOYMENT & PACKAGING

See [docs/specifications/deployment/README.md](specifications/deployment/README.md)

### Systemd Service [Alpha]
- Service unit with sd_notify support
- Socket activation and restart policies
- Resource limits (files, processes, memory) in unit
- Security hardening options (NoNewPrivileges, PrivateTmp, ProtectSystem)

### Configuration & Layout [Alpha]
- Configuration file hierarchy and include semantics
- Data/log directory layout and defaults
- Environment variable overrides for config values
- Service user/group ownership and permissions

### Packaging [Alpha]
- DEB/RPM packaging conventions
- Installation, upgrade, and rollback workflows
- Service registration and default config provisioning

---

## 27. CLUSTER & REPLICATION

See [docs/specifications/Cluster Specification Work/SBCLUSTER-SUMMARY.md](specifications/Cluster%20Specification%20Work/SBCLUSTER-SUMMARY.md) and [docs/specifications/beta_requirements/replication/](specifications/beta_requirements/replication/)

### Cluster Configuration & Membership [Beta]
- Cluster config epoch (CCE) management
- Node identity, enrollment, and rotation
- Quorum membership and voting rules
- Cluster config propagation and validation
- Membership change auditing and rollback

### Sharding & Distributed Query [Beta]
- Sharding strategies and routing
- Shard map management and placement rules
- Distributed query planning and execution
- Cross-shard transaction coordination
- Resharding and rebalancing workflows

### Replication & Consistency [Beta]
- UUIDv7-optimized replication streams
- Merkle forest reconciliation and anti-entropy
- Conflict detection and resolution strategies
- Replication lag tracking and observability
- Schema colocation and version alignment

### Cluster Scheduler & Operations [Beta]
- Cluster-wide job scheduling
- Failover and leader election
- Cluster operations (join/leave, promote/demote)
- Observability and metrics for cluster health

### Cluster Security [Beta]
- Cluster PKI and certificate policies
- Secure inter-node communication and key rotation
- Security posture and threat model controls

---

## SUMMARY STATISTICS

**Total Features Across All Categories:** 8,000+ discrete, testable items

**By Category:**
- DDL Operations: 300+
- DML Operations: 250+
- Transaction System: 400+
- SBLR Bytecode: 500+
- System Catalog: 700+
- Triggers: 257
- UDR System: 500+
- Network & Wire Protocols: 750+
- Tools & Operations: 583
- Compression: 163
- API: 390+
- Testing: 451
- Scheduler: 350+
- Core Engine: 677
- Storage Engine & On-Disk Format: 250+
- Indexes & Access Methods: 200+
- Data Types & Casting: 150+
- Parser & Dialects: 200+
- Query Optimizer & Planner: 150+
- Security & Authentication: 200+
- Backup & Restore: 120+ (Beta)
- Deployment & Packaging: 50+
- Remote Database UDR: 438 (Beta)
- Drivers (11 languages): 750+ (Beta)
- Connectivity (ODBC/JDBC): 286 (Beta)
- ORM/Frameworks (12 frameworks): Cross-cutting features (Beta)
- Cluster & Replication: 300+ (Beta)

**Key Highlights:**
- 5 wire protocol implementations (PostgreSQL, MySQL, TDS, Firebird, Native)
- 11+ index types (B-tree, Hash, GiST, GIN, BRIN, HNSW, Bloom Filter, Inverted, IVF, Zone Maps, LSM Tree, Columnstore)
- 40+ data types with full round-trip fidelity
- 11 driver languages (C/C++, .NET/C#, Go, Java, Node.js/TypeScript, Pascal/Delphi, PHP, Python, R, Ruby, Rust)
- 12 ORM/framework integrations
- 500+ SBLR bytecode operations
- 700+ system catalog features
- Multi-Generational Architecture (MGA) with 64-bit transaction IDs
- Comprehensive security (authentication methods, row-level security, audit logging)
- Enterprise operations (backup/restore, replication, cluster management, monitoring, migration)

---

## USAGE GUIDANCE

### For Implementation [Alpha]
1. Use this checklist to verify all features are implemented
2. Cross-reference with source code to identify gaps
3. Track implementation progress per category
4. Ensure all features have corresponding tests

### For Testing [Alpha]
1. Create test plans covering all features
2. Verify each discrete item is testable
3. Generate test coverage reports
4. Cross-reference test results with this checklist

### For Documentation [Alpha]
1. Ensure all features are documented
2. Cross-reference documentation with feature list
3. Identify undocumented features
4. Update user guides and API references

### For Planning [Alpha]
1. Use feature counts for sprint planning
2. Prioritize features by category
3. Track completion percentage
4. Identify dependencies between features

---

**Last Updated:** January 2026
**Document Version:** 1.0
**Total Feature Count:** 8,000+
**Status:** ✅ UPDATED (Alpha/Beta tagging, gaps filled)
