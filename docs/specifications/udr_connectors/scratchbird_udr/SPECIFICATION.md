# ScratchBird UDR Specification (Untrusted)

Status: Draft (Beta). This specification defines a ScratchBird-to-ScratchBird
UDR connector that uses SBWP v1.1 over TLS. The remote ScratchBird instance is
treated as untrusted (non-cluster); no federation or cluster PKI is used.

## Scope
- SBWP v1.1 client over TCP/TLS (port 3092 by default).
- Untrusted remote: **no cluster trust**, **no federation features**.
- Supports pass-through DDL/DML/PSQL via sys.remote_exec/remote_query.
- Supports schema discovery for migration and emulated schema generation.

## References
- ../UDR_CONNECTOR_BASELINE.md
- ../../Alpha Phase 2/11-Remote-Database-UDR-Specification.md
- ../../Alpha Phase 2/11i-ScratchBird-Client-Implementation.md
- ../../wire_protocols/scratchbird_native_wire_protocol.md
- ../../drivers/DRIVER_METADATA_QUERY_CONTRACT.md

## UDR Module
- Library: libscratchbird_scratchbird_udr.so
- FDW handler: scratchbird_udr_handler
- FDW validator: scratchbird_udr_validator
- Capabilities: network

## Connection and Authentication
- TLS 1.3 required; server cert must be validated.
- Supported auth methods: password, client certificate, optional MFA challenge.
- Cluster PKI is **not** used; `FEATURE_FEDERATION` is disabled.

## Server Options

| Option | Default | Description |
| --- | --- | --- |
| host | required | Server host (allowlisted) |
| port | 3092 | Server port |
| dbname | required | Database name |
| user | required | Remote username |
| password | "" | Password (if auth_method=password) |
| auth_method | password | password | cert | token |
| tls_verify_server | true | Verify server certificate |
| tls_ca | "" | CA cert path |
| tls_cert | "" | Client cert path |
| tls_key | "" | Client key path |
| connect_timeout | 5000 | ms |
| query_timeout | 30000 | ms |
| allow_ddl | false | Allow CREATE/ALTER/DROP |
| allow_dml | true | Allow INSERT/UPDATE/DELETE |
| allow_psql | false | Allow CALL/EXECUTE/PSQL |
| allow_passthrough | false | Allow sys.remote_exec/query |
| allow_sblr | false | Allow SBLR bytecode execution |
| allow_streaming | true | Allow streaming results |
| allow_compression | true | Allow zstd compression |
| allow_checksums | true | Allow CRC32C checksums |

## Security Rules
- `FEATURE_FEDERATION` must not be negotiated.
- `allow_sblr=false` by default; when enabled, remote SBLR still undergoes
  standard SBLR verification on the server side.
- No shared cluster keys, no cluster role trust, no cross-node identity.

## Metadata Discovery (Required)
Use `sys.*` views defined by the metadata contract.

### Introspection Examples (ScratchBird)
```sql
-- Schemas
SELECT schema_id, schema_name, owner_id
FROM sys.schemas
WHERE is_valid = 1;

-- Tables / views
SELECT table_id, schema_id, table_name, table_type
FROM sys.tables
WHERE is_valid = 1;

-- Columns
SELECT column_id, table_id, column_name, data_type_id, ordinal_position,
       is_nullable, default_value
FROM sys.columns
WHERE is_valid = 1;

-- Indexes
SELECT index_id, table_id, index_name, index_type, is_unique
FROM sys.indexes
WHERE is_valid = 1;

-- Constraints
SELECT constraint_id, table_id, constraint_name, constraint_type
FROM sys.constraints
WHERE is_valid = 1;

-- Routines
SELECT procedure_id, schema_id, procedure_name, routine_type
FROM sys.procedures
WHERE is_valid = 1;

SELECT function_id, schema_id, function_name
FROM sys.functions
WHERE is_valid = 1;

-- Triggers / Sequences
SELECT trigger_id, table_id, trigger_name
FROM sys.triggers
WHERE is_valid = 1;

SELECT sequence_id, schema_id, sequence_name
FROM sys.sequences
WHERE is_valid = 1;
```

## Schema Mounting
ScratchBird schemas map directly. Imported schemas are mounted under
`legacy_<server>` (or `mount_root` override) and emulated schemas are created
under `emulated_<server>` per the Remote Database UDR spec.

## Pass-through and Migration
- Pass-through: sys.remote_exec, sys.remote_query, sys.remote_call
- Migration: optional and explicit; uses the migration state machine defined in
  UDR_CONNECTOR_BASELINE.md and the Remote Database UDR spec.

## Trusted Cluster vs Untrusted SBWP

Use **untrusted SBWP (this connector)** when:
- You want a standard remote database connection with no shared trust.
- The remote ScratchBird is managed by a different team/org or is not in the cluster.
- You need migration/passthrough without federation semantics.

Use **cluster federation** when:
- Nodes share cluster PKI and are part of the same trust domain.
- You require cross-node identity, routing, and federation primitives.
- You need cluster-wide query planning or distributed transaction coordination.

## Testing Checklist
- TLS handshake + auth over SBWP v1.1.
- FEATURE_FEDERATION is rejected/disabled for untrusted mode.
- sys.* metadata queries return expected columns.
- Pass-through denied when allow_passthrough=false.
- SBLR disabled by default; enabled only when allow_sblr=true.
