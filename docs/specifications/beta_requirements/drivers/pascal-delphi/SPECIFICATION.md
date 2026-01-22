# Pascal/Delphi/FreePascal Driver Specification

Status: Draft (Target). This specification defines the Pascal/Delphi/FreePascal Driver Specification for ScratchBird V2 using the native wire protocol only.

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
- API: FireDAC/IBX/Zeos/SQLdb adapters for ScratchBird V2.
- Package/module: scratchbird-pascal.
- Parameter style: :name and $1 (rewrite ? to $1..N).
- Transport: TCP with TLS 1.3; optional Unix socket where supported.

## Adapter surface (required components)
The driver must expose adapters for common Pascal data access layers. Each
adapter should provide equivalent operations for connect, execute, and read.

### FireDAC (TFDConnection/TFDQuery)
- TFDConnection: Open, Close, StartTransaction, Commit, Rollback, ExecSQL, Connected
- TFDQuery/TFDCommand: SQL.Text, Prepare, ParamByName, ExecSQL, Open, Next, Eof,
  FieldByName, RowsAffected

### Zeos (TZConnection/TZQuery)
- TZConnection: Connect, Disconnect, StartTransaction, Commit, Rollback
- TZQuery: SQL.Text, Prepare, ParamByName, ExecSQL, Open, Next, EOF, FieldByName,
  RowsAffected

### SQLdb (TSQLConnection/TSQLQuery)
- TSQLConnection: Open, Close, StartTransaction, Commit, Rollback
- TSQLQuery: SQL.Text, Prepare, ParamByName, ExecSQL, Open, Next, EOF, FieldByName,
  RowsAffected

### IBX (TIBDatabase/TIBQuery)
- TIBDatabase: Open, Close, StartTransaction, Commit, Rollback
- TIBQuery: SQL.Text, Prepare, ParamByName, ExecSQL, Open, Next, EOF, FieldByName

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
| ScratchBird type | Pascal type | Notes |
| --- | --- | --- |
| BOOLEAN | Boolean | |
| INT32/INT64 | Integer/Int64 | |
| UINT32/UINT64 | Cardinal/UInt64 | |
| INT128/UINT128 | custom record or string | |
| DECIMAL/MONEY | Currency/Extended | or string |
| FLOAT* | Single/Double | |
| TEXT | string/UnicodeString | UTF-8 |
| BINARY | TBytes | |
| DATE/TIME/TIMESTAMP | TDate/TTime/TDateTime | |
| UUID | TGUID | |
| JSON | string | |
| JSONB | TScratchBirdJsonb | |
| XML | string | |
| ARRAY | dynamic array | |
| RANGE | TScratchBirdRange | |
| VECTOR | array of Single | |
| INET/CIDR/MACADDR | string/record | |
| GEOMETRY | TScratchBirdGeometry | WKB/WKT recommended |

## Type mapping (full target)
| ScratchBird type | Pascal type | Notes |
| --- | --- | --- |
| NULL | Null/Variant | for metadata only |
| BOOLEAN | Boolean | |
| INT8 | ShortInt | |
| UINT8 | Byte | |
| INT16 | SmallInt | |
| UINT16 | Word | |
| INT32 | Integer | |
| UINT32 | Cardinal | |
| INT64 | Int64 | |
| UINT64 | UInt64 | |
| INT128 | custom record/string | |
| UINT128 | custom record/string | |
| DECIMAL | Extended/string | use BigDecimal library if needed |
| MONEY | Currency | scale 4 by default |
| FLOAT32 | Single | |
| FLOAT64 | Double | |
| CHAR | Char/WideChar | fixed length |
| VARCHAR | UnicodeString | |
| TEXT | UnicodeString | |
| JSON | string/TJSONObject | optional JSON library |
| JSONB | TScratchBirdJsonb | wrapper type |
| XML | string/IXMLDocument | |
| BINARY | TBytes | fixed length |
| VARBINARY | TBytes | |
| BLOB/BYTEA | TBytes/TStream | |
| DATE | TDateTime | date-only |
| TIME | TDateTime | time-only |
| TIMESTAMP | TDateTime | |
| INTERVAL | TTimeSpan/custom record | |
| UUID | TGUID | |
| ARRAY | dynamic array | element type preserved |
| RANGE | TScratchBirdRange | wrapper type |
| COMPOSITE/ROW | record | |
| VARIANT | Variant | includes type tag |
| VECTOR | array of Single | |
| TSVECTOR/TSQUERY | string | |
| INET | string | |
| CIDR | string | |
| MACADDR/MACADDR8 | string | |
| GEOMETRY/POINT/LINESTRING/POLYGON/MULTI*/GEOMETRYCOLLECTION | TScratchBirdGeometry | WKB/WKT recommended |

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
| SQLSTATE class | Meaning | Pascal exception |
| --- | --- | --- |
| 01 | Warning | EScratchbirdWarning |
| 02 | No data | EScratchbirdNoData |
| 08 | Connection exception | EScratchbirdConnectionError |
| 0A | Feature not supported | EScratchbirdNotSupported |
| 22 | Data exception | EScratchbirdDataError |
| 23 | Integrity constraint violation | EScratchbirdIntegrityError |
| 28 | Invalid authorization | EScratchbirdAuthError |
| 40 | Transaction rollback | EScratchbirdTransactionError |
| 42 | Syntax/access violation | EScratchbirdSyntaxError |
| 53 | Insufficient resources | EScratchbirdResourceError |
| 54 | Program limit exceeded | EScratchbirdLimitError |
| 57 | Operator intervention | EScratchbirdOperatorInterventionError |
| 58 | System error | EScratchbirdSystemError |
| XX | Internal error | EScratchbirdInternalError |

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
- Pooling: optional external pool component; driver does not pool by default.
- Logging: disabled by default; integrate with application logging.

