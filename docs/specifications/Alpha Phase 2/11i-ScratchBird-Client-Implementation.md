# ScratchBird Client Implementation (UDR Connector)

ScratchBird connector UDR for Remote Database UDR (untrusted, non-cluster).

**See**: [11-Remote-Database-UDR-Specification.md](11-Remote-Database-UDR-Specification.md)  
**Protocol**: SBWP v1.1 (TLS required)  
**Wire Spec**: `wire_protocols/scratchbird_native_wire_protocol.md`

---

## Overview

Implements a ScratchBird client using SBWP v1.1 to connect to a remote
ScratchBird instance as an **untrusted** server. This is not federation and
does not use cluster PKI. The connector behaves like any external database
adapter and uses sys.* metadata queries for introspection.

---

## Key Functions

### Connection Management
```c
void* create_scratchbird_connection(RemoteConnectionPoolConfig* config);
bool validate_scratchbird_connection(void* handle);
void destroy_scratchbird_connection(void* handle);
```

### Query Execution
```c
RemoteResultSet* execute_scratchbird_query(void* handle, const char* sql, IStatus* status);
void* prepare_scratchbird_statement(void* handle, const char* sql, IStatus* status);
RemoteResultSet* execute_scratchbird_prepared(void* handle, void* stmt, IMessageBuffer* params, IStatus* status);
```

### Schema Introspection
```c
List<RemoteSchemaMetadata*>* list_scratchbird_schemas(void* handle, IStatus* status);
List<RemoteTableMetadata*>* list_scratchbird_tables(void* handle, const char* schema, IStatus* status);
RemoteTableMetadata* get_scratchbird_table_metadata(void* handle, const char* schema, const char* table, IStatus* status);
```

---

## Protocol Notes (Untrusted Mode)

- TLS 1.3 required; validate server cert.
- Do **not** negotiate `FEATURE_FEDERATION`.
- `FEATURE_SBLR` is disabled unless allow_sblr=true in server options.
- `FEATURE_CHECKSUMS` and `FEATURE_STREAMING` may be enabled if allowed.

---

## Connection Options

Supported options (REGISTER REMOTE DATABASE):
- `protocol = 'scratchbird'`
- `host`, `port`, `database`, `username`, `password`
- TLS options (`tls_ca`, `tls_cert`, `tls_key`, `tls_verify_server`)
- Feature toggles (`allow_sblr`, `allow_streaming`, `allow_compression`, `allow_checksums`)

---

## Limitations

- No cluster trust or PKI identity propagation.
- No federation primitives (cross-DB identity or routing).
- Treated as a standard remote database for migration and passthrough.
