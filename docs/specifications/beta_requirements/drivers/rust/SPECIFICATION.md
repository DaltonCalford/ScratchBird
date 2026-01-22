# Rust Driver Specification

Status: Draft (Target). This specification defines the Rust Driver Specification for ScratchBird V2 using the native wire protocol only.

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
- API: Async-first client (Tokio) with blocking wrapper.
- Package/module: scratchbird.
- Parameter style: $1 and :name (rewrite ? to $1..N).
- Transport: TCP with TLS 1.3; optional Unix socket where supported.

## Rust API surface (required types)
The driver must provide async APIs and a blocking wrapper with equivalent
capabilities.

```rust
pub struct Client;
impl Client {
  pub async fn connect(opts: Config) -> Result<Self, Error>;
  pub async fn query(&self, sql: &str, params: Params) -> Result<Vec<Row>, Error>;
  pub async fn execute(&self, sql: &str, params: Params) -> Result<CommandResult, Error>;
  pub async fn prepare(&self, sql: &str) -> Result<Statement, Error>;
  pub async fn begin(&self) -> Result<Transaction, Error>;
  pub async fn close(self) -> Result<(), Error>;
}

pub struct Statement;
impl Statement {
  pub async fn bind(&mut self, index: usize, value: Value) -> Result<(), Error>;
  pub async fn bind_named(&mut self, name: &str, value: Value) -> Result<(), Error>;
  pub async fn execute(&self) -> Result<CommandResult, Error>;
  pub async fn query(&self) -> Result<Vec<Row>, Error>;
}

pub struct Transaction;
impl Transaction {
  pub async fn commit(self) -> Result<(), Error>;
  pub async fn rollback(self) -> Result<(), Error>;
}

pub struct Row;
impl Row {
  pub fn get<T: FromValue>(&self, index: usize) -> Result<T, Error>;
  pub fn get_named<T: FromValue>(&self, name: &str) -> Result<T, Error>;
}

pub mod blocking {
  pub struct Client;
  impl Client {
    pub fn connect(opts: Config) -> Result<Self, Error>;
    pub fn query(&self, sql: &str, params: Params) -> Result<Vec<Row>, Error>;
    pub fn execute(&self, sql: &str, params: Params) -> Result<CommandResult, Error>;
    pub fn prepare(&self, sql: &str) -> Result<Statement, Error>;
    pub fn begin(&self) -> Result<Transaction, Error>;
    pub fn close(self) -> Result<(), Error>;
  }
}
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
| ScratchBird type | Rust type | Notes |
| --- | --- | --- |
| BOOLEAN | bool | |
| INT* | i8/i16/i32/i64/i128 | |
| UINT* | u8/u16/u32/u64/u128 | |
| DECIMAL/MONEY | rust_decimal::Decimal | or bigdecimal |
| FLOAT* | f32/f64 | |
| TEXT | String | UTF-8 |
| BINARY | Vec<u8> | |
| DATE/TIME/TIMESTAMP | chrono::NaiveDate/NaiveTime/DateTime<FixedOffset> | |
| UUID | uuid::Uuid | |
| JSON | serde_json::Value | |
| JSONB | scratchbird::types::Jsonb | |
| XML | String | |
| ARRAY | Vec<T> | |
| RANGE | scratchbird::types::Range<T> | custom struct |
| VECTOR | Vec<f32> | |
| INET/CIDR | std::net::IpAddr/IpNet | |
| GEOMETRY | scratchbird::types::Geometry | WKB/WKT recommended |

## Type mapping (full target)
| ScratchBird type | Rust type | Notes |
| --- | --- | --- |
| NULL | Option<T> | for metadata only |
| BOOLEAN | bool | |
| INT8 | i8 | |
| UINT8 | u8 | |
| INT16 | i16 | |
| UINT16 | u16 | |
| INT32 | i32 | |
| UINT32 | u32 | |
| INT64 | i64 | |
| UINT64 | u64 | use BigUint if out of range |
| INT128 | i128 | |
| UINT128 | u128 | |
| DECIMAL | rust_decimal::Decimal | or bigdecimal |
| MONEY | rust_decimal::Decimal | scale 4 by default |
| FLOAT32 | f32 | |
| FLOAT64 | f64 | |
| CHAR | char/String | fixed length |
| VARCHAR | String | |
| TEXT | String | |
| JSON | serde_json::Value | |
| JSONB | scratchbird::types::Jsonb | wrapper type |
| XML | String | |
| BINARY | Vec<u8> | fixed length |
| VARBINARY | Vec<u8> | |
| BLOB/BYTEA | Vec<u8> | |
| DATE | chrono::NaiveDate | |
| TIME | chrono::NaiveTime | |
| TIMESTAMP | chrono::DateTime<FixedOffset> | |
| INTERVAL | chrono::Duration | custom for months/years |
| UUID | uuid::Uuid | |
| ARRAY | Vec<T> | element type preserved |
| RANGE | scratchbird::types::Range<T> | custom struct |
| COMPOSITE/ROW | struct/tuple | |
| VARIANT | enum Value | includes type tag |
| VECTOR | Vec<f32> | |
| TSVECTOR/TSQUERY | String | |
| INET | std::net::IpAddr | |
| CIDR | ipnet::IpNet | |
| MACADDR/MACADDR8 | [u8;6]/[u8;8] | or macaddr crate |
| GEOMETRY/POINT/LINESTRING/POLYGON/MULTI*/GEOMETRYCOLLECTION | scratchbird::types::Geometry | WKB/WKT recommended |

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
| SQLSTATE class | Meaning | Rust error |
| --- | --- | --- |
| 01 | Warning | Error::Warning |
| 02 | No data | Error::NoData |
| 08 | Connection exception | Error::Connection |
| 0A | Feature not supported | Error::NotSupported |
| 22 | Data exception | Error::Data |
| 23 | Integrity constraint violation | Error::Integrity |
| 28 | Invalid authorization | Error::Auth |
| 40 | Transaction rollback | Error::Transaction |
| 42 | Syntax/access violation | Error::Syntax |
| 53 | Insufficient resources | Error::Resource |
| 54 | Program limit exceeded | Error::Limit |
| 57 | Operator intervention | Error::OperatorIntervention |
| 58 | System error | Error::System |
| XX | Internal error | Error::Internal |

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
- Pooling: external pool (deadpool/bb8); driver does not pool by default.
- Logging: disabled by default; integrate with the log crate.

