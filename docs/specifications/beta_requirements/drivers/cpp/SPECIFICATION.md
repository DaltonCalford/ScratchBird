# C++ Driver Specification

Status: Draft (Target). This specification defines the C++ Driver Specification for ScratchBird V2 using the native wire protocol only.

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
- API: C++17 RAII client and C API wrapper (libscratchbird_client).
- Package/module: libscratchbird_cpp.
- Parameter style: $1 and :name (rewrite ? to $1..N).
- Transport: TCP with TLS 1.3; optional Unix socket where supported.

## C++ API surface (required classes)
The C++ client must expose RAII classes with predictable ownership.

```cpp
namespace scratchbird {
struct ConnOptions { /* host, port, user, database, tls, timeouts */ };

class Connection {
public:
  static Connection connect(const std::string& uri);
  static Connection connect(const ConnOptions& options);
  void close();
  Transaction begin();
  void commit();
  void rollback();
  Result execute(const std::string& sql, const Params& params = {});
  Result query(const std::string& sql, const Params& params = {});
  PreparedStatement prepare(const std::string& sql);
  void set_option(const std::string& key, const std::string& value);
};

class PreparedStatement {
public:
  void bind(size_t index, const Value& value);
  void bind(const std::string& name, const Value& value);
  Result execute();
};

class Result {
public:
  bool next();
  Value get(size_t column) const;
  Value get(const std::string& name) const;
  size_t column_count() const;
  ColumnMeta column_meta(size_t column) const;
};

class Transaction {
public:
  void commit();
  void rollback();
};
}
```

### C API (wrapper, required entry points)
- sb_connect / sb_disconnect
- sb_prepare / sb_bind_index / sb_bind_name
- sb_execute / sb_query
- sb_fetch / sb_get_column_meta / sb_value_get
- sb_tx_begin / sb_tx_commit / sb_tx_rollback

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
| ScratchBird type | C++ type | Notes |
| --- | --- | --- |
| BOOLEAN | bool | |
| INT8/INT16/INT32/INT64 | int8_t/int16_t/int32_t/int64_t | |
| UINT8/UINT16/UINT32/UINT64 | uint8_t/uint16_t/uint32_t/uint64_t | |
| INT128/UINT128 | boost::multiprecision::int128_t/uint128_t | or custom | 
| FLOAT32/FLOAT64 | float/double | |
| DECIMAL/MONEY | Decimal wrapper | string fallback |
| CHAR/VARCHAR/TEXT | std::string | UTF-8 |
| BINARY/VARBINARY/BLOB/BYTEA | std::vector<uint8_t> | |
| DATE/TIME/TIMESTAMP | std::chrono types | custom if needed |
| UUID | std::array<uint8_t,16> | or uuid lib |
| JSON | std::string | optional json library |
| JSONB | scratchbird::Jsonb | |
| XML | std::string | |
| ARRAY | std::vector<T> | |
| RANGE | scratchbird::Range<T> | custom struct |
| VECTOR | std::vector<float> | |
| INET/CIDR/MACADDR | custom struct | |
| GEOMETRY | scratchbird::Geometry | WKB/WKT recommended |

## Type mapping (full target)
| ScratchBird type | C++ type | Notes |
| --- | --- | --- |
| NULL | std::nullptr_t | for metadata only |
| BOOLEAN | bool | |
| INT8 | int8_t | |
| UINT8 | uint8_t | |
| INT16 | int16_t | |
| UINT16 | uint16_t | |
| INT32 | int32_t | |
| UINT32 | uint32_t | |
| INT64 | int64_t | |
| UINT64 | uint64_t | use cpp_int if out of range |
| INT128 | boost::multiprecision::int128_t | or cpp_int |
| UINT128 | boost::multiprecision::uint128_t | or cpp_int |
| DECIMAL | Decimal wrapper | string fallback |
| MONEY | Decimal wrapper | scale 4 by default |
| FLOAT32 | float | |
| FLOAT64 | double | |
| CHAR | char/std::string | fixed length |
| VARCHAR | std::string | |
| TEXT | std::string | |
| JSON | std::string | configurable decode |
| JSONB | scratchbird::Jsonb | wrapper type |
| XML | std::string | |
| BINARY | std::vector<uint8_t> | fixed length |
| VARBINARY | std::vector<uint8_t> | |
| BLOB/BYTEA | std::vector<uint8_t> | |
| DATE | std::chrono::sys_days | |
| TIME | std::chrono::nanoseconds | |
| TIMESTAMP | std::chrono::system_clock::time_point | timezone preserved |
| INTERVAL | std::chrono::duration | custom for months/years |
| UUID | std::array<uint8_t,16> | or uuid library |
| ARRAY | std::vector<T> | element type preserved |
| RANGE | scratchbird::Range<T> | custom struct |
| COMPOSITE/ROW | struct/tuple | |
| VARIANT | std::variant/std::any | includes type tag |
| VECTOR | std::vector<float> | |
| TSVECTOR/TSQUERY | std::string | |
| INET | IpAddress | custom or asio |
| CIDR | IpNetwork | custom |
| MACADDR/MACADDR8 | std::array<uint8_t,6/8> | or string |
| GEOMETRY/POINT/LINESTRING/POLYGON/MULTI*/GEOMETRYCOLLECTION | scratchbird::Geometry | WKB/WKT recommended |

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
| SQLSTATE class | Meaning | C++ exception |
| --- | --- | --- |
| 01 | Warning | scratchbird::Warning |
| 02 | No data | scratchbird::NoDataError |
| 08 | Connection exception | scratchbird::ConnectionError |
| 0A | Feature not supported | scratchbird::NotSupportedError |
| 22 | Data exception | scratchbird::DataError |
| 23 | Integrity constraint violation | scratchbird::IntegrityError |
| 28 | Invalid authorization | scratchbird::AuthError |
| 40 | Transaction rollback | scratchbird::TransactionError |
| 42 | Syntax/access violation | scratchbird::SyntaxError |
| 53 | Insufficient resources | scratchbird::ResourceError |
| 54 | Program limit exceeded | scratchbird::LimitError |
| 57 | Operator intervention | scratchbird::OperatorInterventionError |
| 58 | System error | scratchbird::SystemError |
| XX | Internal error | scratchbird::InternalError |

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
- Pooling: no built-in pool; external pool recommended for apps needing reuse.
- Logging: disabled by default; integrate with the application logger.

