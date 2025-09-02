[ScratchBird Analysis Documentation](../../index.md)

### FDW and Database Links

This document describes the FDW core SPI, builtin adapters (CSV/JSON/PostgreSQL), database links, and their catalog, security, and error-handling integration.

## FDW SPI and adapter architecture

FDWs plug into the engine via a stable SPI defined in `scratchbird/engine/fdw.h`:

- ForeignDataWrapper: abstract interface providing connection management, schema introspection, query and DML execution, transaction hooks, pushdown predicates, and cost estimation.
- ForeignResultIterator: streaming row iterator returned by `execute_select` for remote scans.
- FdwRegistry: in-process plugin registry and dynamic loader (`dlopen`) for FDW implementations (expects a `create_fdw_plugin` symbol).
- FdwManager: coordinator bound to the catalog that resolves servers/user mappings/tables, negotiates capabilities, and executes foreign queries.

Capabilities are declared with `FdwCapability` flags and probed with `has_capability(...)`. Query planning consults each adapter’s `can_pushdown_*` and cost estimators.

Key headers and sources:
- `include/scratchbird/engine/fdw.h`
- `src/engine/fdw.cpp`
- Executor integration: `src/engine/executor.cpp` (uses `FdwManager::execute_foreign_query`)

## Adapters

### CSV FDW (`csv_fdw`)

Overview: Simple file-backed adapter providing sequential scans over RFC4180-like CSV files. Connection is a lightweight handle to a base directory.

- Capabilities: Select, SchemaIntrospection (no DML, no transactions, no pushdown)
- Server options: `base_path` (directory), optional `delimiter`, `quote_char`, `escape_char`, `has_header`, `encoding`, `null_string`
- Table options: `file_path` (overrides `base_path`/name.csv resolution)
- Implementation: `include/scratchbird/engine/fdw_csv.h`, `src/engine/fdw_csv.cpp`

#### Catalog integration {#csv-catalog}

- FOREIGN SERVER row in `SDB$FOREIGN_SERVERS` with `fdw_name='csv_fdw'` and server `options` for CSV parsing and base path.
- FOREIGN TABLE rows in `SDB$FOREIGN_TABLES` reference the server and store per-table `options` (e.g., `file_path`). Column definitions are materialized in `SDB$FOREIGN_TABLE_COLUMNS` (type inference on sample rows).

DDL examples:

```sql
CREATE FOREIGN SERVER csv_srv
  OPTIONS (base_path '/data/csv', delimiter ',', has_header 'true');

CREATE FOREIGN TABLE people (
  name varchar, age bigint, active boolean
) SERVER csv_srv OPTIONS (file_path '/data/csv/people.csv');
```

#### Query execution {#csv-exec}

- `SELECT` streams lines, parses fields, performs projection locally; no WHERE/JOIN/AGG pushdown.
- DML: INSERT/UPDATE/DELETE not supported; transactions not supported.
- Costing: simple file-size/rows heuristic; joins are not pushed down (very high cost).

Implementation references: `CsvForeignDataWrapper::execute_select`, `CsvResultIterator`.

### JSON FDW (`json_fdw`)

Overview: File-backed adapter that treats JSON arrays as tables and objects as single-row tables. Validates base path and infers schema per file.

- Capabilities: Select, SchemaIntrospection, LimitPushdown (no DML, no transactions; WHERE/JOIN/AGG pushdown currently disabled)
- Server options: `base_path` (required), plus `root_path`, `encoding`, `flatten_objects`, `array_as_table`, `max_depth`, `strict_mode`
- Table options: `file_path`
- Implementation: `include/scratchbird/engine/fdw_json.h`, `src/engine/fdw_json.cpp`

#### Catalog integration {#json-catalog}

- FOREIGN SERVER in `SDB$FOREIGN_SERVERS` with `fdw_name='json_fdw'` and base path plus JSON options.
- FOREIGN TABLE entries in `SDB$FOREIGN_TABLES`; columns inferred from sample JSON content and stored in `SDB$FOREIGN_TABLE_COLUMNS`.

DDL examples:

```sql
CREATE FOREIGN SERVER json_srv
  OPTIONS (base_path '/data/json', array_as_table 'true');

CREATE FOREIGN TABLE events (
  id bigint, name varchar, active boolean
) SERVER json_srv OPTIONS (file_path '/data/json/events.json');
```

#### Query execution {#json-exec}

- `SELECT` streams array elements or object properties via `JsonResultIterator`.
- LIMIT pushdown: iterator can stop early; WHERE/joins/aggregates are evaluated locally (no pushdown at present).
- DML/transactions: not supported.

Implementation references: `JsonForeignDataWrapper::execute_select`, `JsonResultIterator`.

### PostgreSQL FDW (`postgresql_fdw`)

Overview: Network adapter for PostgreSQL via libpq (or mock when not compiled with libpq). Supports broad pushdown and transactional semantics.

- Capabilities: Select/Insert/Update/Delete, Where/Join/Aggregate/Limit pushdown, TransactionSupport, BulkOperations, SchemaIntrospection
- Server config: `host`, `port` (default 5432), `database`, `use_ssl`, SSL paths; optional GSSAPI/Kerberos options (e.g., `gssencmode`, `krbsrvname`)
- User mapping: remote/user credentials; SSL/Kerberos depend on server options
- Implementation: `include/scratchbird/engine/fdw_postgresql.h`, `src/engine/fdw_postgresql.cpp`

#### Catalog integration {#pg-catalog}

- `SDB$FOREIGN_SERVERS` rows with `fdw_name='postgresql_fdw'` contain connection and SSL/auth options; user credentials are stored via security/credential managers and referenced by `SDB$USER_MAPPINGS`.
- `SDB$FOREIGN_TABLES` map local to remote `schema.table`; `IMPORT FOREIGN SCHEMA` populates tables and columns.

DDL examples:

```sql
CREATE FOREIGN SERVER pg_srv
  OPTIONS (host 'db.example.com', port '5432', database 'sales', use_ssl 'true');

CREATE USER MAPPING FOR current_user SERVER pg_srv
  OPTIONS (user 'reporter', password '••••');

IMPORT FOREIGN SCHEMA public FROM SERVER pg_srv INTO ext_public;

CREATE FOREIGN TABLE ext_public.orders
  SERVER pg_srv OPTIONS (remote_schema 'public', remote_table 'orders');
```

#### Query execution {#pg-exec}

- `execute_select` dispatches to PostgreSQL and streams results; WHERE/JOIN/AGG/LIMIT commonly pushed down.
- DML supported; transactions supported with BEGIN/COMMIT/ROLLBACK; SSL honored when configured.
- Costing: network- and row-based models; join pushdown enabled when possible.

Implementation references: `PostgreSqlForeignDataWrapper`, `PostgreSqlResultIterator`.

## Database links

Database links provide cross-database access via `table@link` syntax and explicit link DDL. Links wrap an FDW plus connection metadata and are managed by `DatabaseLinkManager`.

Syntax:

- Create: `CREATE DATABASE LINK link_name CONNECT TO 'host:port/database?opt1=val1&opt2=val2' USING 'fdw_name';`
- Drop: `DROP DATABASE LINK link_name;`
- Alter: `ALTER DATABASE LINK link_name CONNECT TO 'new_connect_string';`
- Reference in queries: `SELECT * FROM remote_table@link_name WHERE ...;`

Execution and transactions:
- `DatabaseLinkManager` parses `table@link`, resolves the link, establishes/validates the FDW connection, and delegates `execute_select`.
- Best-effort distributed transactions are coordinated via `begin_distributed_transaction` / `commit_distributed_transaction` / `rollback_distributed_transaction`, invoking each FDW’s transaction hooks.

Implementation references: `include/scratchbird/engine/database_link.h`, `src/engine/database_link.cpp`.

## Catalog, DDL, and integration

Catalog objects (see `include/scratchbird/engine/fdw_catalog.h` and `src/engine/fdw_catalog.cpp`):
- `SDB$FOREIGN_DATA_WRAPPERS`: registered FDWs and capabilities
- `SDB$FOREIGN_SERVERS`: servers, connection/options, health
- `SDB$USER_MAPPINGS`: per-user credentials and auth method
- `SDB$FOREIGN_TABLES`, `SDB$FOREIGN_TABLE_COLUMNS`: foreign tables and columns
- `SDB$DATABASE_LINKS`: named database links and status
- `SDB$FDW_STATISTICS`, `SDB$FDW_OPTIONS`: stats and options overlay

Supported DDL (executed via `FdwCatalogDDLExecutor`):
- CREATE/DROP FOREIGN DATA WRAPPER
- CREATE/ALTER/DROP FOREIGN SERVER
- CREATE/ALTER/DROP USER MAPPING
- CREATE/ALTER/DROP FOREIGN TABLE
- IMPORT FOREIGN SCHEMA ... FROM SERVER ... INTO ...
- CREATE/ALTER/DROP DATABASE LINK
- GRANT/REVOKE on FOREIGN SERVER and DATABASE LINK

## Security and permissions

Security components (see `include/scratchbird/engine/fdw_security.h`, `src/engine/fdw_security.cpp`):
- Permissions: `FdwPermission` (USAGE/CONNECT/SELECT/INSERT/UPDATE/DELETE/...) enforced by `FdwPermissionManager`.
- Credentials: `FdwCredentialManager` stores encrypted per-user credentials referenced by user mappings.
- Row-level security: policies bound to foreign tables; optional enabling per table.
- Audit: `FdwAuditLogger` records connection and query events.
- High-level `FdwSecurityManager` validates context, authorizes server/table operations, checks query safety, and enforces connection security (e.g., SSL requirement).

## Error handling and diagnostics

Subsystems (see `include/scratchbird/engine/fdw_error_handling.h`, `src/engine/fdw_error_handling.cpp`):
- Structured errors: `FdwError` with category/severity and recommended recovery action.
- Recovery: retry/reconnect/backoff guidance; circuit breaker in `FdwConnectionMonitor`.
- Diagnostics: connectivity/performance analysis and health dashboards via `FdwDiagnostics` and `FdwDiagnosticsManager`.

## Implementation References
- `include/scratchbird/engine/fdw.h`
- `src/engine/fdw.cpp`
- `include/scratchbird/engine/fdw_csv.h`, `src/engine/fdw_csv.cpp`
- `include/scratchbird/engine/fdw_json.h`, `src/engine/fdw_json.cpp`
- `include/scratchbird/engine/fdw_postgresql.h`, `src/engine/fdw_postgresql.cpp`
- `include/scratchbird/engine/fdw_catalog.h`, `src/engine/fdw_catalog.cpp`
- `include/scratchbird/engine/fdw_security.h`, `src/engine/fdw_security.cpp`
- `include/scratchbird/engine/fdw_error_handling.h`, `src/engine/fdw_error_handling.cpp`
- `include/scratchbird/engine/database_link.h`, `src/engine/database_link.cpp`

## Spec Trace
- [REQ-FDW-CORE](../../traceability/spec/requirements.md#req-fdw-core)

## Related
- [ScratchBird Analysis Documentation](../../index.md)
