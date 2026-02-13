# MSSQL Client Implementation

MSSQL/TDS wire protocol client adapter for Remote Database UDR.

See: 11-Remote-Database-UDR-Specification.md and wire_protocols/tds_wire_protocol.md

## 1. Scope
- Protocol: TDS 7.4 (SQL Server 2012+)
- Transport: TCP with optional TLS
- Authentication: SQL Login (Windows/SSPI optional in Beta)

## 2. Connection and Authentication

### 2.1 Prelogin
- Send PRELOGIN packet with version, encryption, instance, thread id
- If encryption required, negotiate TLS before LOGIN7

### 2.2 Login7
- Send LOGIN7 with:
  - user, password (obfuscated per TDS rules)
  - database
  - client/app/host info
  - optional feature extensions
- Expect ENVCHANGE tokens and LOGINACK

## 3. Command Execution

### 3.1 SQL Batch
- Use SQL Batch token for ad-hoc SQL text
- Results are a stream of tokens:
  - COLMETADATA
  - ROW
  - DONE/DONEPROC/DONEINPROC

### 3.2 RPC / Prepared Statements
- Use RPC for parameterized calls (sp_executesql)
- Parameter metadata must include type, length, precision/scale

### 3.3 Pagination
- Prefer OFFSET/FETCH pushdown
- Cursor support is optional for Alpha

## 4. Cancellation
- Send ATTENTION packet on a dedicated interrupt channel
- Expect a DONE token with status indicating cancel

## 5. Transactions
- BEGIN TRAN / COMMIT / ROLLBACK
- Savepoints via SAVE TRAN / ROLLBACK TRAN savepoint

## 6. Error Mapping
- ERROR token includes number, state, class, message
- Map to ScratchBird error catalog

## 7. Type Mapping (Minimum)
- tinyint/smallint/int/bigint -> INT
- float/real -> FLOAT/DOUBLE
- decimal/numeric -> DECIMAL
- bit -> BOOL
- char/varchar/nchar/nvarchar -> STRING
- binary/varbinary/image -> BYTES
- date/datetime/datetime2 -> DATE/TIMESTAMP
- time -> TIME
- uniqueidentifier -> UUID

Unsupported types map to STRING unless strict_types is enabled.

## 8. Schema Introspection
Use sys.* catalogs:
- sys.schemas, sys.tables, sys.views
- sys.columns, sys.types
- sys.indexes, sys.index_columns
- sys.foreign_keys, sys.foreign_key_columns
- sys.procedures, sys.triggers

## 9. Required Capabilities
- SQL batch execution
- parameterized execution (sp_executesql)
- cancellation (ATTENTION)
- schema introspection

## 10. References
- https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-tds/

