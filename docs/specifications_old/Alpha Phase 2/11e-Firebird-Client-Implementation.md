# Firebird Client Implementation

Firebird wire protocol client adapter for Remote Database UDR.

See: 11-Remote-Database-UDR-Specification.md and wire_protocols/firebird_wire_protocol.md

## 1. Scope
- Protocol: Firebird remote protocol v10-17 (Firebird 1.0-5.0)
  - Primary target: Firebird 5.0
- Transport: TCP with optional encryption
- Authentication: SRP (Firebird 3+), legacy password

## 2. Connection and Authentication

### 2.1 Handshake Overview
- op_connect / op_accept handshake establishes protocol version
- op_attach attaches to a database (filename or alias)
- Authentication is plugin-based (e.g., Srp, Legacy_Auth)
- op_cont_auth is used for multi-step auth

### 2.2 Encryption
- Firebird supports wire encryption; if enabled, op_crypt/op_crypt_key_callback
  are used during handshake

## 3. Statement Execution

### 3.1 DSQL Execution
- Allocate statement: op_allocate_statement
- Prepare: op_prepare_statement
- Execute: op_execute / op_execute2
- Fetch: op_fetch / op_fetch_response
- Free: op_free_statement

### 3.2 Blob Handling
- op_create_blob/op_open_blob + op_put_segment/op_get_segment
- Inline blob optimization where supported (protocol 17)

### 3.3 Batch
- Protocol 17+ supports op_batch_* operations
- Batch execution is optional for Alpha

## 4. Cancellation
- Use op_cancel for running requests
- Expect op_response with error status

## 5. Transactions
- op_transaction to start
- op_commit/op_rollback/op_commit_retaining/op_rollback_retaining

## 6. Error Mapping
- Firebird responses contain status vector
- Map GDS codes to ScratchBird error catalog

## 7. Type Mapping (Minimum)
- SMALLINT/INTEGER/BIGINT -> INT
- FLOAT/DOUBLE -> FLOAT/DOUBLE
- NUMERIC/DECIMAL -> DECIMAL
- CHAR/VARCHAR -> STRING
- BLOB -> BYTES
- DATE/TIME/TIMESTAMP -> DATE/TIME/TIMESTAMP
- BOOLEAN -> BOOL

Unsupported types map to STRING unless strict_types is enabled.

## 8. Schema Introspection
Use RDB$ system tables:
- RDB$RELATIONS, RDB$RELATION_FIELDS
- RDB$FIELDS, RDB$TYPES
- RDB$INDICES, RDB$INDEX_SEGMENTS
- RDB$RELATION_CONSTRAINTS, RDB$REF_CONSTRAINTS
- RDB$TRIGGERS, RDB$PROCEDURES

## 9. Required Capabilities
- prepare/execute/fetch
- cancellation
- schema introspection

## 10. References
- wire_protocols/firebird_wire_protocol.md
- https://raw.githubusercontent.com/FirebirdSQL/firebird/master/src/remote/protocol.h
