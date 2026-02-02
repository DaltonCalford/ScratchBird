# Firebird UDR Specification

Status: Draft (Target). This specification defines the Firebird UDR that
connects using the native Firebird wire protocol without vendor drivers.

## Scope
- Supported Firebird versions: 2.5, 3.0, 4.0, 5.0 (primary target: 5.0).
- Uses Firebird wire protocol (see wire_protocols/firebird_wire_protocol.md).
- Supports pass-through DDL/DML/PSQL via sys.remote_exec/remote_query.

## References
- ../UDR_CONNECTOR_BASELINE.md
- ../../Alpha Phase 2/11-Remote-Database-UDR-Specification.md
- ../../Alpha Phase 2/11e-Firebird-Client-Implementation.md
- ../../wire_protocols/firebird_wire_protocol.md

## UDR Module
- Library: libscratchbird_firebird_udr.so
- FDW handler: firebird_udr_handler
- FDW validator: firebird_udr_validator
- Capabilities: network

## Connection and Authentication
- Auth methods: SRP (preferred), legacy auth (optional).
- DPB and TPB settings must be exposed for transaction semantics.
- Database path is a server-side path; ensure allowlist and safety.

## Server Options

| Option | Default | Description |
| --- | --- | --- |
| host | required | Server host (allowlisted) |
| port | 3050 | Server port |
| dbname | required | Database path or alias |
| charset | UTF8 | Connection charset |
| connect_timeout | 5000 | ms |
| query_timeout | 30000 | ms |
| allow_ddl | false | Allow CREATE/ALTER/DROP |
| allow_dml | true | Allow INSERT/UPDATE/DELETE |
| allow_psql | true | Allow EXECUTE PROCEDURE/PSQL |
| allow_passthrough | false | Allow sys.remote_exec/query |

## Example SQL Setup
```sql
CREATE FOREIGN DATA WRAPPER firebird_fdw
    HANDLER firebird_udr_handler
    VALIDATOR firebird_udr_validator;

CREATE SERVER legacy_fb
    FOREIGN DATA WRAPPER firebird_fdw
    OPTIONS (host 'legacy-db', port '3050', dbname '/data/legacy.fdb',
             allow_dml 'true', allow_psql 'true', allow_passthrough 'true');

CREATE USER MAPPING FOR migration_role
    SERVER legacy_fb
    OPTIONS (user 'SYSDBA', password '***');
```

## Pass-through Examples
```sql
CALL sys.remote_exec('legacy_fb', 'CREATE TABLE tmp(id int)');
SELECT * FROM sys.remote_query('legacy_fb', 'SELECT count(*) FROM users');
CALL sys.remote_call('legacy_fb', 'REBUILD_INDEXES', '{}');
```

## Metadata Discovery (Required)
Use Firebird system tables for schema analysis:
- RDB$RELATIONS, RDB$RELATION_FIELDS, RDB$FIELDS
- RDB$INDICES, RDB$INDEX_SEGMENTS
- RDB$RELATION_CONSTRAINTS, RDB$REF_CONSTRAINTS
- RDB$PROCEDURES, RDB$PROCEDURE_PARAMETERS
- RDB$TRIGGERS

### Introspection Examples (Firebird)
```sql
-- Tables / views
SELECT r.RDB$RELATION_NAME, r.RDB$SYSTEM_FLAG, r.RDB$VIEW_BLR
FROM RDB$RELATIONS r
WHERE r.RDB$SYSTEM_FLAG = 0;

-- Columns
SELECT rf.RDB$RELATION_NAME, rf.RDB$FIELD_NAME, f.RDB$FIELD_TYPE,
       f.RDB$FIELD_SUB_TYPE, f.RDB$FIELD_LENGTH, f.RDB$FIELD_SCALE,
       f.RDB$FIELD_PRECISION, rf.RDB$NULL_FLAG
FROM RDB$RELATION_FIELDS rf
JOIN RDB$FIELDS f ON f.RDB$FIELD_NAME = rf.RDB$FIELD_SOURCE
WHERE rf.RDB$RELATION_NAME = ?
ORDER BY rf.RDB$FIELD_POSITION;

-- Constraints (PK/UNIQUE/FOREIGN)
SELECT rc.RDB$CONSTRAINT_NAME, rc.RDB$CONSTRAINT_TYPE, rc.RDB$RELATION_NAME,
       rc.RDB$INDEX_NAME, ref.RDB$CONST_NAME_UQ
FROM RDB$RELATION_CONSTRAINTS rc
LEFT JOIN RDB$REF_CONSTRAINTS ref
  ON ref.RDB$CONSTRAINT_NAME = rc.RDB$CONSTRAINT_NAME;

-- Index segments
SELECT i.RDB$INDEX_NAME, s.RDB$FIELD_NAME, s.RDB$FIELD_POSITION
FROM RDB$INDICES i
JOIN RDB$INDEX_SEGMENTS s ON s.RDB$INDEX_NAME = i.RDB$INDEX_NAME
WHERE i.RDB$RELATION_NAME = ?
ORDER BY s.RDB$FIELD_POSITION;

-- Procedures
SELECT p.RDB$PROCEDURE_NAME, p.RDB$PROCEDURE_INPUTS, p.RDB$PROCEDURE_OUTPUTS
FROM RDB$PROCEDURES p
WHERE p.RDB$SYSTEM_FLAG = 0;

-- Triggers
SELECT t.RDB$TRIGGER_NAME, t.RDB$RELATION_NAME, t.RDB$TRIGGER_TYPE, t.RDB$TRIGGER_SEQUENCE
FROM RDB$TRIGGERS t
WHERE t.RDB$SYSTEM_FLAG = 0;

-- Sequences (generators)
SELECT g.RDB$GENERATOR_NAME, g.RDB$SYSTEM_FLAG
FROM RDB$GENERATORS g
WHERE g.RDB$SYSTEM_FLAG = 0;
```

## Schema Mounting
Firebird has no schema layer. Imported objects are mounted under a single
schema (default `public`) within `legacy_<server>` (or `mount_root` override).

## PSQL Considerations
- Firebird PSQL syntax is not identical to ScratchBird PSQL.
- The UDR must translate or pass-through PSQL as configured.
- View and procedure definitions must be stored in the emulated schema tree.

## Testing Checklist
- SRP auth and legacy auth.
- Pass-through PSQL execution gated by allow_psql.
- DPB/TPB settings mapped to ScratchBird transaction levels.
