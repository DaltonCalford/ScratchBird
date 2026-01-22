# Ruby Driver Specification

Status: Draft (Target). This specification defines the Ruby Driver Specification for ScratchBird V2 using the native wire protocol only.

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
- API: Ruby DBI/Sequel/ActiveRecord adapter.
- Package/module: scratchbird.
- Parameter style: $1 and :name (rewrite ? to $1..N).
- Transport: TCP with TLS 1.3; optional Unix socket where supported.

## Ruby API surface (required signatures)
The driver must expose a Ruby-native API and adapter hooks for DBI/Sequel/AR.

```ruby
module Scratchbird
  def self.connect(uri_or_opts) -> Connection; end
end

class Connection
  def exec(sql, params = nil) -> Result; end
  def query(sql, params = nil) -> Result; end
  def prepare(sql) -> Statement; end
  def begin; end
  def commit; end
  def rollback; end
  def transaction(&block); end
  def close; end
end

class Statement
  def bind_param(index_or_name, value); end
  def execute(params = nil) -> Result; end
  def fetch; end
  def fetch_all; end
  def finish; end
end

class Result
  def each(&block); end
  def fields; end
  def row_count; end
end
```

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
| ScratchBird type | Ruby type | Notes |
| --- | --- | --- |
| BOOLEAN | TrueClass/FalseClass | |
| INT* / UINT* | Integer | bignum |
| DECIMAL/MONEY | BigDecimal | |
| FLOAT* | Float | |
| TEXT | String | UTF-8 |
| BINARY | String (BINARY) | |
| DATE/TIME/TIMESTAMP | Date/Time/DateTime | timezone-aware |
| UUID | String | canonical 8-4-4-4-12 |
| JSON | Hash/Array/String | |
| JSONB | Scratchbird::JSONB | |
| XML | String | |
| ARRAY | Array | |
| RANGE | Scratchbird::RangeValue | |
| VECTOR | Array | |
| INET/CIDR | IPAddr | |
| GEOMETRY | Scratchbird::Geometry | optional RGeo |

## Type mapping (full target)
| ScratchBird type | Ruby type | Notes |
| --- | --- | --- |
| NULL | nil | for metadata only |
| BOOLEAN | TrueClass/FalseClass | |
| INT8 | Integer | |
| UINT8 | Integer | |
| INT16 | Integer | |
| UINT16 | Integer | |
| INT32 | Integer | |
| UINT32 | Integer | |
| INT64 | Integer | |
| UINT64 | Integer | bignum |
| INT128 | Integer | |
| UINT128 | Integer | |
| DECIMAL | BigDecimal | |
| MONEY | BigDecimal | scale 4 by default |
| FLOAT32 | Float | |
| FLOAT64 | Float | |
| CHAR | String | fixed length |
| VARCHAR | String | |
| TEXT | String | |
| JSON | Hash/Array/String | configurable decoder |
| JSONB | Scratchbird::JSONB | wrapper type |
| XML | String | |
| BINARY | String (BINARY) | fixed length |
| VARBINARY | String (BINARY) | |
| BLOB/BYTEA | String (BINARY) | |
| DATE | Date | |
| TIME | Time | |
| TIMESTAMP | Time/DateTime | |
| INTERVAL | ActiveSupport::Duration | or custom |
| UUID | String | canonical |
| ARRAY | Array | element type preserved |
| RANGE | Scratchbird::RangeValue | wrapper type |
| COMPOSITE/ROW | Array/Struct | |
| VARIANT | Object | includes type tag |
| VECTOR | Array | |
| TSVECTOR/TSQUERY | String | |
| INET | IPAddr | |
| CIDR | IPAddr | |
| MACADDR/MACADDR8 | String | |
| GEOMETRY/POINT/LINESTRING/POLYGON/MULTI*/GEOMETRYCOLLECTION | Scratchbird::Geometry | WKB/WKT recommended |

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
| SQLSTATE class | Meaning | Ruby exception |
| --- | --- | --- |
| 01 | Warning | Scratchbird::Warning |
| 02 | No data | Scratchbird::NoDataError |
| 08 | Connection exception | Scratchbird::ConnectionError |
| 0A | Feature not supported | Scratchbird::NotSupportedError |
| 22 | Data exception | Scratchbird::DataError |
| 23 | Integrity constraint violation | Scratchbird::IntegrityError |
| 28 | Invalid authorization | Scratchbird::AuthError |
| 40 | Transaction rollback | Scratchbird::TransactionError |
| 42 | Syntax/access violation | Scratchbird::SyntaxError |
| 53 | Insufficient resources | Scratchbird::ResourceError |
| 54 | Program limit exceeded | Scratchbird::LimitError |
| 57 | Operator intervention | Scratchbird::OperatorInterventionError |
| 58 | System error | Scratchbird::SystemError |
| XX | Internal error | Scratchbird::InternalError |

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
- Pooling: external pool (connection_pool/ActiveRecord); driver does not pool by default.
- Logging: disabled by default; integrate with Ruby Logger.

