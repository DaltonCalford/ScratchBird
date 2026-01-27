# UDR Connector Baseline Specification

Status: Draft (Target). This document defines the shared requirements for all
connector UDRs shipped in the first Beta (ODBC, JDBC, PostgreSQL, MySQL,
FirebirdSQL, MSSQL, ScratchBird) plus local file/script connectors.

## Scope
- UDRs are separate plugin modules that expose external resources through the
  ScratchBird UDR system and SQL/MED (FDW) surfaces.
- No vendor-provided client drivers are used for wire-protocol connectors.
  PostgreSQL/MySQL/Firebird/MSSQL UDRs must implement the native wire protocols
  directly from ScratchBird specifications.
- ODBC and JDBC UDRs are packaged with an embedded driver manager and any
  required drivers inside the UDR bundle so the host OS does not need external
  client libraries installed.
- All connectors must provide schema discovery and mounting support to map
  remote objects into `legacy_<server>` and `emulated_<server>` namespaces.
- UDRs do not create or maintain connections on load. Connections are created
  only after explicit ADMIN + SQL configuration.
- Pass-through DDL/DML/PSQL is supported and permissioned by configuration.

### Required Beta Connector Set
- `postgresql_udr` (PostgreSQL wire protocol)
- `mysql_udr` (MySQL wire protocol)
- `firebird_udr` (Firebird wire protocol)
- `mssql_udr` (TDS wire protocol)
- `scratchbird_udr` (SBWP wire protocol, untrusted)
- `odbc_udr` (embedded driver manager + bundled drivers)
- `jdbc_udr` (embedded driver manager + bundled drivers)

## Non-goals
- This does not define language client drivers or protocol emulation listeners.
- This does not allow remote file access over network connections.
- This does not grant automatic rights; all capabilities must be explicitly
  enabled and audited.

## References
- 10-UDR-System-Specification.md
- 09_DDL_FOREIGN_DATA.md
- Alpha Phase 2/11-Remote-Database-UDR-Specification.md
- wire_protocols/*

## Connector Packaging and Signing
Each UDR module ships with:
- Shared library: `libscratchbird_<connector>.so` (or .dll)
- Manifest: `scratchbird_udr_<connector>.json`
- Signature: `scratchbird_udr_<connector>.sig`

### Manifest schema (minimum)
```json
{
  "name": "postgresql_udr",
  "version": "1.0.0",
  "git_commit": "<commit-hash>",
  "build_id": "<ci-build-id>",
  "udr_api": "1.0.0",
  "capabilities": ["network"],
  "connectors": ["postgresql"],
  "supported_server_versions": {
    "postgresql": ["9.6", "10", "11", "12", "13", "14", "15", "16", "17"]
  },
  "protocol_versions": {
    "postgresql": "3.0"
  },
  "checksum_sha256": "<sha256>",
  "signature_alg": "ed25519",
  "signature_key_id": "sb-udr-release-01"
}
```

### Required verification rules
- Library file must not be world-writable.
- SHA256 must match manifest.
- Signature must validate against an engine-trusted public key.
- `git_commit` must exist in the approved repository and tag set.
- `supported_server_versions` must include the target remote DB version.

## Admin and SQL Commands

### 1) Install and verify the UDR plugin (ADMIN)
Required commands (CLI or ADMIN SQL):
```
# Example CLI (preferred)
sb_admin udr install --plugin /opt/scratchbird/udr/libscratchbird_postgresql_udr.so \
  --manifest /opt/scratchbird/udr/scratchbird_udr_postgresql.json \
  --signature /opt/scratchbird/udr/scratchbird_udr_postgresql.sig

sb_admin udr verify postgresql_udr
sb_admin udr list
```

### 2) Register FDW handler (SQL)
```sql
CREATE FOREIGN DATA WRAPPER postgresql_fdw
    HANDLER postgresql_udr_handler
    VALIDATOR postgresql_udr_validator;

GRANT USAGE ON FOREIGN DATA WRAPPER postgresql_fdw TO migration_role;
```

### 3) Configure server and user mapping (SQL)
```sql
CREATE SERVER legacy_pg
    FOREIGN DATA WRAPPER postgresql_fdw
    OPTIONS (host 'legacy-db', port '5432', dbname 'production',
             allow_ddl 'true', allow_dml 'true', allow_psql 'true',
             allow_passthrough 'true');

CREATE USER MAPPING FOR migration_role
    SERVER legacy_pg
    OPTIONS (user 'legacy_user', password '***');
```

### 4) Create foreign tables or run passthrough (SQL)
```sql
IMPORT FOREIGN SCHEMA public FROM SERVER legacy_pg INTO legacy_schema;
CALL sys.remote_exec('legacy_pg', 'CREATE TABLE tmp(id int)');
```

## Permissions and Capability Model
Capabilities are declared in the manifest and must be enabled per server.

- `network`: outbound TCP to allowlisted hosts/ports
- `file_read`: read-only local filesystem access
- `file_write`: write/append to local filesystem
- `subprocess`: run local scripts (no network)

Server-level options (SQL):
- `allow_passthrough`: allow sys.remote_exec/remote_query
- `allow_ddl`: allow CREATE/ALTER/DROP on remote
- `allow_dml`: allow INSERT/UPDATE/DELETE on remote
- `allow_psql`: allow CALL/EXECUTE/PSQL on remote
- `allowed_hosts`: comma list (network UDRs)
- `allowed_ports`: comma list (network UDRs)
- `root_dir`: local filesystem root (file/script UDRs)
- `path_allowlist`: glob list, relative to root
- `path_denylist`: glob list, relative to root
- `max_rows`: hard row limit for file/script UDRs
- `max_bytes`: hard output limit for file/script UDRs

## Connection Lifecycle
- A UDR module is loaded and verified at engine startup or by ADMIN action.
- Connections are created only when a query is executed or admin verifies
  connectivity (`CALL sys.udr_test_connection(server)`).
- Idle connections are pooled per server.
- All credentials are stored in user mappings or external secrets.

## Pass-through Execution Interface (Required)
All remote DB UDRs must implement these system procedures/functions.

```sql
-- DDL/DML/PSQL pass-through
CALL sys.remote_exec(server_name, sql_text [, params_json [, options_json]]);

-- Query pass-through
SELECT * FROM sys.remote_query(server_name, sql_text [, params_json [, options_json]]);

-- Stored procedure call
CALL sys.remote_call(server_name, routine_name [, params_json [, options_json]]);
```

Options include:
- `timeout_ms`
- `transaction_mode` (auto, join, new)
- `read_only` (true/false)
- `fetch_size`

## Security and Sandboxing
- Network UDRs must use TLS when supported by the remote server.
- Local file/script UDRs must deny network access at the OS sandbox level.
- Canonicalize all paths and reject path traversal and symlinks by default.
- Enforce per-UDR memory, time, and file handle limits.
- All UDR calls are audited with server name, SQL text hash, user, and result.

## Migration and Cutover Support
All remote DB UDRs must support a controlled migration window:

- Emulated schema tree: foreign schema import creates a `legacy_<db>` schema.
  A translator generates native ScratchBird tables, views, and PSQL routines
  under `emulated_<db>` based on remote definitions.
- Pass-through period: DDL/DML/PSQL is executed on the legacy DB while
  replication keeps ScratchBird synchronized.
- Verification gate: counts, checksums, and sampled queries must match before
  cutover is allowed.

### Migration state machine (minimum)
```
DISCOVERY -> REPLICATING -> VERIFYING -> CUTOVER_READY -> CUTOVER_DONE -> RETIRED
```

### Required admin procedures
```sql
CALL sys.migration_begin(server_name, target_schema => 'emulated_pg',
                         mode => 'passthrough');
CALL sys.migration_verify(server_name);
CALL sys.migration_cutover(server_name);
CALL sys.migration_retire(server_name);
```

## Example UDR Entry Point (C++)
```cpp
extern "C" IPluginModule* pluginEntryPoint() {
  return new ConnectorUdrModule();
}
```

## Testing Requirements (minimum)
- Signature verification failure blocks load.
- Capability enforcement for network/file/script access.
- Pass-through DDL/DML/PSQL succeeds or is denied per server config.
- Migration state machine transitions are audited.
