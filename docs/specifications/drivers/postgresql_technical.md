# Building a PostgreSQL-Compatible Server: Complete Technical Reference

PostgreSQL emulation requires implementing four interconnected layers: the wire protocol for client communication, the SQL parser for query understanding, the type system for data handling, and the system catalog for metadata queries. This guide provides the complete technical specifications needed to build a PostgreSQL-compatible frontend to a custom storage engine, drawing from PostgreSQL's source code and lessons from successful emulation projects like CockroachDB and YugabyteDB.

## The wire protocol forms the communication foundation

All PostgreSQL client-server communication uses a message-based protocol over TCP (default port 5432). Understanding this protocol is essential—it's what makes psql, JDBC, and every other PostgreSQL client work.

### Message structure and startup sequence

Every message after startup follows a consistent format: a single-byte type identifier, a 4-byte length (including itself but not the type byte), and a variable payload. The startup message is the exception—it has no type byte.

**Connection handshake sequence:**
1. Client opens TCP connection
2. Client optionally sends `SSLRequest` (8 bytes: length=8, code=80877103)
3. Server responds with `S` (will SSL) or `N` (no SSL)
4. Client sends `StartupMessage` with protocol version **196608** (3 << 16) and parameters
5. Server sends authentication challenge (type `R`)
6. Client responds with credentials
7. Server sends `AuthenticationOk` (type `R`, code 0)
8. Server sends `ParameterStatus` messages (type `S`) for each runtime parameter
9. Server sends `BackendKeyData` (type `K`) with process ID and secret key
10. Server sends `ReadyForQuery` (type `Z`) with transaction status `I` (idle)

**Essential ParameterStatus parameters to send:**
- `server_version`: Version string (e.g., "15.0")
- `client_encoding`: Character encoding (typically "UTF8")
- `server_encoding`: Server-side encoding
- `DateStyle`: Date format (e.g., "ISO, MDY")
- `TimeZone`: Server timezone
- `integer_datetimes`: Always "on" for modern servers
- `standard_conforming_strings`: "on" for proper string handling

### Authentication mechanisms

**MD5 Authentication** (legacy but common): The server sends 4 random salt bytes. The client computes `md5(md5(password + username) + salt)` and sends it prefixed with "md5".

**SCRAM-SHA-256** (recommended): Follows RFC 5802 with a 4-message handshake:
1. Server → Client: `AuthenticationSASL` listing mechanisms
2. Client → Server: `SASLInitialResponse` with `n,,n=<user>,r=<nonce>`
3. Server → Client: `AuthenticationSASLContinue` with `r=<combined-nonce>,s=<salt>,i=<iterations>`
4. Client → Server: `SASLResponse` with `c=<channel>,r=<nonce>,p=<proof>`
5. Server → Client: `AuthenticationSASLFinal` with `v=<signature>`

### Simple vs Extended query protocol

The **Simple Query Protocol** sends SQL as text (type `Q`) and receives results entirely in text format. Response sequence: `RowDescription` → `DataRow`* → `CommandComplete` → `ReadyForQuery`. This is sufficient for psql and basic tools.

The **Extended Query Protocol** separates parsing, binding, and execution for prepared statements:

| Message | Type | Purpose |
|---------|------|---------|
| Parse | `P` | Parse SQL with parameter placeholders, create named statement |
| Bind | `B` | Bind parameter values, create named portal |
| Describe | `D` | Request parameter types or result column metadata |
| Execute | `E` | Execute portal, optionally limit rows |
| Sync | `S` | End command sequence, triggers implicit commit/rollback |

**Critical implementation detail:** After an error in extended protocol, the server enters "ignore until Sync" mode, discarding all messages until it receives Sync.

### Data format specifications

**RowDescription** (type `T`) describes result columns:
```
Int16: number of fields
Per field:
  String: field name (null-terminated)
  Int32: table OID (0 if not a table column)
  Int16: column number (0 if not a table column)
  Int32: type OID
  Int16: type size (-1 for variable)
  Int32: type modifier
  Int16: format code (0=text, 1=binary)
```

**DataRow** (type `D`) contains values:
```
Int16: number of columns
Per column:
  Int32: length (-1 for NULL)
  Bytes: value data (if length >= 0)
```

**ErrorResponse** (type `E`) uses field codes for structured error information:
- `S`: Severity (ERROR, FATAL, WARNING)
- `C`: SQLSTATE code (always 5 characters, e.g., "42P01")
- `M`: Primary message (always present)
- `D`: Detail, `H`: Hint, `P`: Position in query
- `t`: Table name, `c`: Column name, `n`: Constraint name

## The parser transforms SQL text into executable structures

PostgreSQL's parser is a two-stage system: raw parsing (syntax only) followed by semantic analysis (catalog lookups, type resolution). This separation exists because transaction control commands must be parseable without a transaction context.

### Parser architecture and grammar

The parser uses **Flex** for lexical analysis (`scan.l` → `scan.c`) and **Bison** for grammar (`gram.y` → `gram.c`). The grammar file `gram.y` is one of the largest Bison grammars in open source—over **65,000 lines**—defining the complete PostgreSQL SQL dialect.

**Source locations:**
- Lexer: `src/backend/parser/scan.l`
- Grammar: `src/backend/parser/gram.y`
- Entry point: `raw_parser()` in `src/backend/parser/parser.c`
- Parse nodes: `src/include/nodes/parsenodes.h`

**For emulator implementers**, the recommended approach is to use **libpg_query** (github.com/pganalyze/libpg_query), a standalone C library containing PostgreSQL's parser. This provides JSON parse trees without running a PostgreSQL server, with bindings available for Go, Rust, Python, Ruby, and Node.js.

### Key parse tree node types

Statements produce specific node types in `parsenodes.h`:

```c
// SELECT produces SelectStmt
typedef struct SelectStmt {
    NodeTag type;
    List *distinctClause;
    List *targetList;      // ResTarget nodes for SELECT items
    List *fromClause;      // RangeVar, JoinExpr nodes
    Node *whereClause;     // A_Expr tree
    List *sortClause;      // SortBy nodes
    Node *limitOffset;
    Node *limitCount;
    SetOperation op;       // UNION, INTERSECT, EXCEPT
    // ...
} SelectStmt;
```

**Expression nodes:**
- `A_Expr`: Operators (`col > 5`, `a + b`)
- `A_Const`: Literals
- `ColumnRef`: Column references
- `FuncCall`: Function invocations
- `TypeCast`: Explicit casts (`x::int`)
- `SubLink`: Subqueries (IN, EXISTS, ANY)

### PostgreSQL-specific SQL extensions

PostgreSQL deviates from SQL standard in several ways that must be supported:

- **RETURNING clause**: `INSERT/UPDATE/DELETE ... RETURNING expr, ...`
- **ON CONFLICT (UPSERT)**: `INSERT ... ON CONFLICT (col) DO UPDATE SET ...`
- **DISTINCT ON**: `SELECT DISTINCT ON (col) ...`
- **Dollar-quoted strings**: `$$literal text$$` or `$tag$text$tag$`
- **Array subscripts**: `arr[1]`, `arr[2:5]`
- **LIMIT/OFFSET** (vs SQL standard FETCH FIRST)

**Operator precedence** (highest to lowest): `::` (typecast) → `[]` → unary `-/+` → `^` → `*/%` → `+-` → comparison → `NOT` → `AND` → `OR`

## The type system requires precise OID mappings

Type handling is critical for client compatibility. JDBC, in particular, relies heavily on type OIDs for parameter binding and result processing.

### Essential type OIDs

| Type | OID | Array OID | Binary Size |
|------|-----|-----------|-------------|
| bool | 16 | 1000 | 1 byte |
| int2 | 21 | 1005 | 2 bytes |
| int4 | 23 | 1007 | 4 bytes |
| int8 | 20 | 1016 | 8 bytes |
| float4 | 700 | 1021 | 4 bytes |
| float8 | 701 | 1022 | 8 bytes |
| numeric | 1700 | 1231 | variable |
| text | 25 | 1009 | variable |
| varchar | 1043 | 1015 | variable |
| timestamp | 1114 | 1115 | 8 bytes |
| timestamptz | 1184 | 1185 | 8 bytes |
| date | 1082 | 1182 | 4 bytes |
| uuid | 2950 | 2951 | 16 bytes |
| json | 114 | 199 | variable |
| jsonb | 3802 | 3807 | variable |

### Wire format encoding

**Text format** (format code 0) uses human-readable strings. **Binary format** (format code 1) uses network byte order (big-endian).

**Integer encoding:**
- int2: 2 bytes, big-endian signed
- int4: 4 bytes, big-endian signed  
- int8: 8 bytes, big-endian signed

**Date/time encoding:**
- date: Int32, days since 2000-01-01
- timestamp: Int64, microseconds since 2000-01-01 00:00:00
- timestamptz: Int64, microseconds since 2000-01-01 00:00:00 UTC

**Array binary format:**
```
Int32 ndim           // Number of dimensions
Int32 flags          // Has nulls (0 or 1)  
Int32 element_oid    // Element type OID
Per dimension:
  Int32 size         // Dimension size
  Int32 lower_bound  // Lower bound (default 1)
Per element:
  Int32 length       // -1 for NULL
  Bytes data         // Element in binary format
```

### Type coercion rules

PostgreSQL uses three coercion contexts stored in `pg_cast.castcontext`:
- `i` (implicit): Allowed anywhere without explicit cast
- `a` (assignment): Allowed in INSERT/UPDATE target columns
- `e` (explicit): Requires CAST() or :: operator

**Key implicit casts:** int2→int4→int8→numeric, float4→float8, varchar→text (binary-coercible)

## System catalogs provide metadata that clients expect

The `pg_catalog` schema contains system tables that PostgreSQL queries for metadata management. Many tools and ORMs query these tables directly.

### Critical system tables

**pg_class** (OID 1259): Central catalog for all relations (tables, indexes, views, sequences).
```
oid          -- Relation OID
relname      -- Relation name
relnamespace -- FK → pg_namespace.oid
relkind      -- 'r'=table, 'i'=index, 'v'=view, 'S'=sequence
reltuples    -- Estimated row count (planner stat)
relhasindex  -- Has indexes
```

**pg_attribute**: Column definitions, one row per column per relation.
```
attrelid     -- FK → pg_class.oid
attname      -- Column name
atttypid     -- FK → pg_type.oid
attnum       -- Column position (1-based; negatives for system columns)
attnotnull   -- NOT NULL constraint
attisdropped -- Column has been dropped
```

**pg_type** (OID 1247): All data types.
```
oid          -- Type OID
typname      -- Type name
typlen       -- Fixed size (-1 for variable)
typelem      -- Array element type
typarray     -- Array type of this type
typinput     -- Text input function
typreceive   -- Binary receive function
```

**pg_namespace** (OID 2615): Schemas.
```
oid          -- Namespace OID
nspname      -- Schema name
```
Well-known namespaces: 11=pg_catalog, 2200=public

**pg_index**: Index metadata linking to pg_class.
```
indexrelid   -- FK → pg_class.oid (index itself)
indrelid     -- FK → pg_class.oid (indexed table)
indisunique  -- Is unique index
indisprimary -- Is primary key
indkey       -- Array of column numbers
```

### Minimum pg_catalog for client compatibility

For JDBC and most ORMs to function, implement at minimum:
- **pg_type**: Required for parameter type discovery
- **pg_class**: Required for table metadata queries
- **pg_attribute**: Required for column information
- **pg_namespace**: Required for schema qualification
- **pg_database**: Required for connection validation

CockroachDB implements these as **read-only virtual views** computed on demand—a practical approach that avoids maintaining stateful system tables.

## The query execution pipeline processes SQL in five stages

Understanding the execution pipeline helps when debugging compatibility issues.

**Parser → Analyzer → Rewriter → Planner → Executor**

1. **Parser**: Converts SQL text to raw parse tree (syntax only, no catalog access)
2. **Analyzer**: Transforms parse tree to query tree, resolving names and types via catalog lookups
3. **Rewriter**: Applies rules (view expansion, rule system), potentially producing multiple queries
4. **Planner**: Generates execution plan with cost estimates using statistics from pg_statistic
5. **Executor**: Runs plan using Volcano-style demand-pull iterator model

**Prepared statements** are parsed, analyzed, and rewritten at PREPARE time, but planned at EXECUTE time with actual parameter values. After 5 executions, PostgreSQL may switch to a generic plan if cost-effective.

**Portals** hold execution state for queries. Cursors are implemented as portals with names. `WITH HOLD` cursors survive transaction boundaries by materializing results.

## Lessons from existing PostgreSQL emulation projects

### Two fundamental architectural approaches

**Embed PostgreSQL** (YugabyteDB, Aurora): Fork PostgreSQL code and replace the storage layer. YugabyteDB runs actual PostgreSQL postmaster processes, achieving **92%+ query pattern compatibility**. The challenge is merging changes as PostgreSQL releases new versions.

**Reimplement the protocol** (CockroachDB, QuestDB): Build from scratch. CockroachDB implements pgwire entirely in Go, with system catalogs as virtual views. More flexible but requires tracking PostgreSQL's evolving syntax and semantics.

### CockroachDB compatibility insights

CockroachDB provides valuable lessons on what can differ:
- Integer division returns decimal (not truncated integer)
- Different operator precedence for bitwise operators
- Limited implicit type coercions compared to PostgreSQL
- Cannot support system attributes (xmin, xmax) correctly
- Virtual pg_catalog tables can be slow in distributed queries

### Wire protocol libraries for implementation

| Language | Library | Notes |
|----------|---------|-------|
| Go | jackc/pgproto3 | Protocol encoder/decoder, foundation for pgx driver |
| Go | jeroenrinzema/psql-wire | Full server framework with SSL, SCRAM, COPY |
| Rust | sunng87/pgwire | "Like hyper, but for postgres protocol" |
| Java | Google PGAdapter | Translates pgwire to Cloud Spanner |

## Implementation strategy and testing approach

### Recommended implementation order

1. **Wire protocol basics**: StartupMessage handling, AuthenticationOk, ParameterStatus, ReadyForQuery
2. **Simple Query Protocol**: Query message → RowDescription → DataRow → CommandComplete cycle
3. **Core type handling**: int4, int8, text, bool, timestamp with text format encoding
4. **Minimal pg_catalog**: pg_type, pg_class, pg_attribute as virtual views
5. **Extended Query Protocol**: Parse, Bind, Describe, Execute, Sync
6. **Binary format**: For performance-critical clients
7. **COPY protocol**: For bulk data operations

### Testing tools and approaches

**Protocol analysis:**
- **Wireshark** with `pgsql` filter for packet inspection
- **pgproto3 Trace()** for programmatic tracing
- **pgs-debug/pgshark** for command-line protocol dumps

**Compatibility testing:**
- **PostgreSQL regression suite**: `src/test/regress/` (note: ~28% pass rate on non-PostgreSQL databases due to Postgres-specific features)
- **sqllogictest**: Portable SQL semantics tests with 98%+ pass rates across compatible systems
- **pgbench**: Stress testing for connection and transaction handling
- **JDBC driver tests**: Most demanding client for compatibility verification

### Common pitfalls to avoid

- **Startup message has no type byte** — check magic bytes for SSLRequest vs StartupMessage
- **Message length includes itself** — a 4-byte length of "8" means 4 more payload bytes
- **Multiple messages use type 'p'** — context (auth state) determines interpretation
- **Error handling in extended protocol** — server ignores messages until Sync after error
- **Implicit transactions** — multiple statements in one Query share a transaction
- **Empty query** — returns EmptyQueryResponse (type `I`), not error

## Source code reference guide

| Component | Location | Key Files |
|-----------|----------|-----------|
| Wire protocol | src/backend/libpq/ | pqcomm.c, pqformat.c, auth.c, auth-scram.c |
| Parser | src/backend/parser/ | gram.y, scan.l, parser.c, analyze.c |
| Node types | src/include/nodes/ | parsenodes.h, primnodes.h, plannodes.h |
| Catalogs | src/include/catalog/ | pg_type.h, pg_class.h, pg_attribute.h |
| Type I/O | src/backend/utils/adt/ | Type-specific input/output functions |
| Executor | src/backend/executor/ | execMain.c, execProcnode.c, node*.c |

**Online resources:**
- Protocol specification: postgresql.org/docs/current/protocol.html
- System catalogs: postgresql.org/docs/current/catalogs.html
- Source browser: doxygen.postgresql.org
- Standalone parser: github.com/pganalyze/libpg_query
- Go protocol library: github.com/jackc/pgx/tree/master/pgproto3
- Rust protocol library: github.com/sunng87/pgwire

Building a PostgreSQL-compatible server is achievable by focusing first on the wire protocol, implementing minimal but correct system catalogs, and leveraging existing parser libraries. The key insight from successful projects is that **wire protocol compatibility gets you client connectivity**, while **true SQL compatibility requires careful attention to type coercion, operator behavior, and the hundreds of edge cases that real applications depend on**.