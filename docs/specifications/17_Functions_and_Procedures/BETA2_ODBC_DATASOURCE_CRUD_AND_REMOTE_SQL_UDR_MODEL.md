# Beta 2 ODBC Datasource CRUD And Remote SQL UDR Model

## Purpose

This document defines the UDR package family used to access foreign databases
through an `ODBC` datasource with governed metadata, query, prepared
statement, and remote `CRUD` semantics.

The package is intended to let ScratchBird fully utilize any foreign ODBC
driver that supports the same capability surfaces currently required by the
ScratchBird ODBC driver baseline.

## Owning package

- `sb_pkg_odbc_connector_udr`

## Scope

This package owns:

- ODBC datasource connection and capability admission
- remote metadata discovery
- prepared statement lifecycle
- parameter binding
- remote query execution
- remote insert/update/delete execution
- remote transaction participation where supported
- result streaming
- generated key retrieval
- SQLSTATE/native-code mapping

## Hard baseline rule

The ODBC connector UDR capability target must meet or exceed the capability
surface currently documented in
`docs/specifications/30_Client_Tooling/DRIVER_ODBC_BASELINE_SPECIFICATION.md`
for every foreign ODBC driver that advertises support for those same
capabilities.

The connector must not be intentionally weaker than the ScratchBird-driver ODBC
baseline on a capability that the foreign ODBC driver itself supports.

## Explicit non-goals

This package does not claim:

- universal parity with every ODBC driver regardless of its own limits
- unrestricted passthrough outside connector policy
- implicit trust of arbitrary DSNs
- unmanaged host-driver installation or system-wide DSN authoring

## Connector admission model

1. Every ODBC datasource must be represented by a governed connector record.
2. The connector record must include:
   - connector id
   - connector name
   - datasource kind `odbc`
   - DSN or connection-string descriptor
   - auth profile
   - policy version
   - capability profile
   - state
3. Connector execution is forbidden until the connector is attested and
   policy-admitted.

## Required routine families

At minimum the package shall provide:

- `sb_odbc.connect_describe(...)`
- `sb_odbc.capabilities(...)`
- `sb_odbc.tables(...)`
- `sb_odbc.columns(...)`
- `sb_odbc.prepare(...)`
- `sb_odbc.execute_query(...)`
- `sb_odbc.execute_nonquery(...)`
- `sb_odbc.execute_batch(...)`
- `sb_odbc.fetch_more(...)`
- `sb_odbc.get_generated_keys(...)`
- `sb_odbc.begin_remote_txn(...)`
- `sb_odbc.commit_remote_txn(...)`
- `sb_odbc.rollback_remote_txn(...)`

## Example contract

```sql
select *
from sb_odbc.execute_query(
    connector_name => 'erp_sqlserver',
    sql_text => 'select customer_id, balance from dbo.customers where status = ?',
    parameters => json_array('ACTIVE')
);

call sb_odbc.execute_nonquery(
    connector_name => 'erp_sqlserver',
    sql_text => 'update dbo.customers set balance = ? where customer_id = ?',
    parameters => json_array(102.55, 42)
);
```

## Capability matrix

The package must publish a connector-local capability profile covering at
minimum:

- connect/auth
- autocommit control
- remote transaction control
- prepared statements
- positional parameter binding
- parameter arrays/batch execution
- multiple result sets
- generated keys
- metadata discovery
- type info discovery
- streaming fetch
- timeout/cancel
- SQLSTATE/native diagnostic retrieval

A capability may be used only when both of the following are true:

1. the package/runtime supports the capability
2. the foreign ODBC driver advertises and proves the capability

## CRUD contract

The connector package must support:

- `create` via remote insert execution
- `read` via remote query execution
- `update` via remote update execution
- `delete` via remote delete execution

The package must also support:

- parameterized prepared variants of all four
- batch variants when the foreign driver supports them
- generated-key retrieval after insert when the foreign driver supports it

## Metadata contract

The package must support metadata routines equivalent in coverage to the
ScratchBird ODBC baseline when the foreign driver exposes them, including:

- tables
- columns
- primary keys
- foreign keys
- statistics/indexes
- procedures
- procedure parameters
- type info

Metadata payloads must preserve foreign catalog/schema/object identity and
must be representable through the recursive ScratchBird schema tree when
mounted as overlays.

## Prepared statement contract

1. Prepared handles shall be catalog-tracked or session-tracked with explicit
   lifecycle state.
2. Positional parameter binding is mandatory.
3. Parameter/result metadata must be discoverable when the foreign driver
   exposes it.
4. Batch/array execution must be used when the foreign driver supports it.

## Transaction contract

1. The connector must expose remote autocommit state when the foreign driver
   supports it.
2. Remote explicit begin/commit/rollback must be supported when the foreign
   driver supports transaction control.
3. If a foreign driver does not support a transaction capability, the package
   must fail closed rather than emulate a false guarantee.
4. Remote transaction identity and terminal state must be tracked explicitly.

## Type mapping contract

The connector package must provide deterministic mapping between foreign ODBC
types and admitted ScratchBird types for:

- integer families
- exact numerics
- floating numerics
- booleans
- char/varchar/text
- binary/blob
- date/time/timestamp
- uuid/guid where exposed
- json/xml-like payloads where exposed
- array/struct/ref-like or advanced families where the foreign driver exposes
  them through a mappable ODBC contract

Unsupported types must fail with a structured capability/mapping error.

## Error and diagnostics contract

The package must expose:

- foreign `SQLSTATE`
- foreign native error code
- normalized ScratchBird error id
- structured diagnostic records
- timeout/cancel/auth/network/metadata classes

The package must preserve foreign diagnostics closely enough that a caller can
reason about the remote driver and remote database error truth.

## Security and policy rules

1. Credentials must be stored and referenced through governed secret material,
   not plain text in user-visible routine calls.
2. Each connector must carry explicit passthrough policy for:
   - query
   - mutation
   - ddl
   - procedure calls
   - transaction control
3. A connector may be metadata-only, read-only, or full-crud according to
   policy.
4. Policy must be able to forbid local callers from using a foreign driver
   capability even if the driver supports it.

## Observability and metrics

Every remote operation shall emit:

- connector id/name
- foreign driver identity/version
- capability profile id
- bytes sent/received where available
- rows returned/affected
- prepare/execute/fetch timing
- timeout/cancel counts
- remote error-class counts

## Deterministic error vocabulary

At minimum the package shall surface:

- `ODBC_CONNECTOR_NOT_FOUND`
- `ODBC_CONNECTOR_NOT_READY`
- `ODBC_CONNECTOR_CAPABILITY_MISSING`
- `ODBC_CONNECTOR_POLICY_FORBIDS_OPERATION`
- `ODBC_CONNECTOR_SQLSTATE_ERROR`
- `ODBC_CONNECTOR_NATIVE_DRIVER_ERROR`
- `ODBC_CONNECTOR_TRANSACTION_UNSUPPORTED`
- `ODBC_CONNECTOR_GENERATED_KEYS_UNSUPPORTED`
- `ODBC_CONNECTOR_BATCH_UNSUPPORTED`
- `ODBC_CONNECTOR_TYPE_MAPPING_FAILED`

## Cross-section dependencies

- section `24` for connector identity, attestation, capability rows, and
  passthrough policy
- section `30` for the ODBC baseline capability contract that this package must
  meet or exceed where the foreign driver supports it
- section `20` for diagnostics and metrics
- section `23` for result streaming and table-producing execution semantics
- section `33` for memory/buffer policy during fetch and batch paths
