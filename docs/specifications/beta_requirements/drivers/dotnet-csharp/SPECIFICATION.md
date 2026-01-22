# C# / .NET Driver Specification

Status: Draft (Target). This specification defines the C# / .NET Driver Specification for ScratchBird V2 using the native wire protocol only.

## Scope
- Protocol: ScratchBird Native Wire Protocol (SBWP) v1.1 only.
- Default port: 3092.
- TLS 1.3 required.
- Emulated protocol drivers (PostgreSQL/MySQL/Firebird/TDS) are out of scope.

## Dependencies and references
- `../DRIVER_BASELINE_SPEC.md`
- `../../wire_protocols/scratchbird_native_wire_protocol.md`
- `../../DATA_TYPE_PERSISTENCE_AND_CASTS.md`

## Driver architecture
- API: ADO.NET provider with optional EF Core integration.
- Package/module: ScratchBird.Data.
- Parameter style: $1 and :name (rewrite ? to $1..N).
- Transport: TCP with TLS 1.3; optional Unix socket where supported.

## ADO.NET API surface (required types)
These are the minimum ADO.NET entry points the provider must implement.
Unsupported features must throw NotSupportedException.

### Provider factory
- DbProviderFactory (ScratchBirdFactory):
  - DbConnection CreateConnection()
  - DbCommand CreateCommand()
  - DbParameter CreateParameter()
  - DbDataAdapter CreateDataAdapter() (optional)

### Connection
- DbConnection:
  - string ConnectionString { get; set; }
  - string Database { get; }
  - ConnectionState State { get; }
  - void Open() / Task OpenAsync(CancellationToken)
  - void Close()
  - DbTransaction BeginTransaction(IsolationLevel)
  - DbCommand CreateCommand()
  - void ChangeDatabase(string database) (optional)

### Command
- DbCommand:
  - string CommandText { get; set; }
  - CommandType CommandType { get; set; } (Text required)
  - int CommandTimeout { get; set; }
  - DbParameterCollection Parameters { get; }
  - void Prepare()
  - int ExecuteNonQuery() / Task<int> ExecuteNonQueryAsync(...)
  - object ExecuteScalar() / Task<object> ExecuteScalarAsync(...)
  - DbDataReader ExecuteReader() / Task<DbDataReader> ExecuteReaderAsync(...)
  - void Cancel()

### Data reader
- DbDataReader:
  - bool Read() / Task<bool> ReadAsync(...)
  - bool NextResult() / Task<bool> NextResultAsync(...)
  - int FieldCount { get; }
  - string GetName(int ordinal)
  - Type GetFieldType(int ordinal)
  - object GetValue(int ordinal)
  - GetBoolean/GetInt32/GetInt64/GetDecimal/GetDateTime/GetString/GetGuid/GetBytes
  - bool IsDBNull(int ordinal)
  - int RecordsAffected { get; }

### Parameters and transactions
- DbParameter:
  - string ParameterName { get; set; }
  - DbType DbType { get; set; }
  - object Value { get; set; }
  - ParameterDirection Direction { get; set; }
  - byte Precision { get; set; } / byte Scale { get; set; }
  - int Size { get; set; }
- DbTransaction:
  - void Commit()
  - void Rollback()
  - IsolationLevel IsolationLevel { get; }

## Connection string
- URI form: `scratchbird://user:password@host:3092/database?param=value`.
- Key-value form (if supported): `host=... port=3092 dbname=... user=... password=...`.

### Required parameters
- host
- database
- user

### Optional parameters
- port (default 3092)
- sslmode (disable/allow/prefer/require/verify-full)
- sslrootcert / sslcert / sslkey
- connect_timeout / socket_timeout
- application_name
- search_path
- binary_transfer (default true)
- compression (zstd on/off)

## Authentication
- Must support SCRAM-SHA-256.
- Support TLS client certificate auth.
- Password auth only if server configuration allows it.

## Type mapping (initial)
| ScratchBird type | C# type | Notes |
| --- | --- | --- |
| BOOLEAN | bool | |
| INT8/16/32/64 | sbyte/short/int/long | |
| UINT* | byte/ushort/uint/ulong | |
| INT128/UINT128 | BigInteger | or Int128/UInt128 (.NET 7) |
| DECIMAL/MONEY | decimal | |
| FLOAT* | float/double | |
| TEXT | string | UTF-8 |
| BINARY | byte[] | |
| DATE | DateOnly | |
| TIME | TimeOnly | |
| TIMESTAMP | DateTime/DateTimeOffset | |
| UUID | Guid | |
| JSON | string/JsonDocument | |
| JSONB | ScratchBirdJsonb | |
| XML | string/XmlDocument | |
| ARRAY | List<T> | |
| RANGE | ScratchBirdRange<T> | custom |
| VECTOR | float[] | |
| INET/CIDR | IPAddress | custom netmask |
| GEOMETRY | ScratchBirdGeometry | optional NetTopologySuite |

## Type mapping (full target)
| ScratchBird type | C# type | Notes |
| --- | --- | --- |
| NULL | DBNull.Value/null | for metadata only |
| BOOLEAN | bool | |
| INT8 | sbyte | |
| UINT8 | byte | |
| INT16 | short | |
| UINT16 | ushort | |
| INT32 | int | |
| UINT32 | uint | |
| INT64 | long | |
| UINT64 | ulong | use BigInteger if out of range |
| INT128 | BigInteger | or Int128 (.NET 7) |
| UINT128 | BigInteger | or UInt128 (.NET 7) |
| DECIMAL | decimal | |
| MONEY | decimal | scale 4 by default |
| FLOAT32 | float | |
| FLOAT64 | double | |
| CHAR | char/string | fixed length |
| VARCHAR | string | |
| TEXT | string | |
| JSON | string/JsonDocument | optional System.Text.Json |
| JSONB | ScratchBirdJsonb | wrapper type |
| XML | string/XDocument | |
| BINARY | byte[] | fixed length |
| VARBINARY | byte[] | |
| BLOB/BYTEA | byte[]/Stream | |
| DATE | DateOnly/DateTime | DateOnly preferred |
| TIME | TimeOnly/TimeSpan | |
| TIMESTAMP | DateTime/DateTimeOffset | |
| INTERVAL | TimeSpan | custom for months/years |
| UUID | Guid | |
| ARRAY | T[]/List<T> | |
| RANGE | ScratchBirdRange<T> | wrapper type |
| COMPOSITE/ROW | object[]/record/class | |
| VARIANT | object | includes type tag |
| VECTOR | float[] | |
| TSVECTOR/TSQUERY | string | |
| INET | IPAddress | |
| CIDR | IPNetwork/custom | |
| MACADDR/MACADDR8 | byte[]/string | |
| GEOMETRY/POINT/LINESTRING/POLYGON/MULTI*/GEOMETRYCOLLECTION | ScratchBirdGeometry | WKB/WKT recommended |

## Prepared statements
- Support server-side prepare and bind using SBWP PARSE/BIND/EXECUTE.
- Provide statement cache with configurable size.
- Allow explicit statement names for long-lived prepared statements.

## Result handling
- Support streaming result sets for large data.
- Preserve column metadata: name, type, precision/scale, nullability.

## Error handling
- Map SQLSTATE and server diagnostics to driver-native exceptions.
- Expose server error fields when available (message, detail, hint, position).

## SQLSTATE mapping (minimum)
| SQLSTATE class | Meaning | .NET exception |
| --- | --- | --- |
| 01 | Warning | ScratchbirdWarning |
| 02 | No data | ScratchbirdNoDataException |
| 08 | Connection exception | ScratchbirdConnectionException |
| 0A | Feature not supported | ScratchbirdNotSupportedException |
| 22 | Data exception | ScratchbirdDataException |
| 23 | Integrity constraint violation | ScratchbirdIntegrityException |
| 28 | Invalid authorization | ScratchbirdAuthException |
| 40 | Transaction rollback | ScratchbirdTransactionException |
| 42 | Syntax/access violation | ScratchbirdSyntaxException |
| 53 | Insufficient resources | ScratchbirdResourceException |
| 54 | Program limit exceeded | ScratchbirdLimitException |
| 57 | Operator intervention | ScratchbirdOperatorInterventionException |
| 58 | System error | ScratchbirdSystemException |
| XX | Internal error | ScratchbirdInternalException |

## Performance features
- Default to binary format for result data.
- Optional compression negotiation (zstd).
- Optional SBLR execution for repeated queries if supported.

## Canonical defaults
- LOB streaming: BLOB/TEXT/JSONB/GEOMETRY stream by default over binary protocol.
- Inline threshold: 8 MiB; chunk size: 128 KiB; backpressure via STREAM_CONTROL and native stream primitives.
- COPY/bulk: explicit CopyIn/CopyOut API only; binary default; text only when requested.
- Notifications: dedicated connection required for listen/notify APIs.
- Cancel: send CANCEL; if no response within 5000 ms, close the connection.
- Statement cache: per-connection LRU, default 64 entries; auto-prepare after 5 uses; invalidate on prepare/schema errors.
- Pooling: ADO.NET provider pooling (enabled by default; configurable in connection string).
- Logging: disabled by default; integrate with Microsoft.Extensions.Logging.

