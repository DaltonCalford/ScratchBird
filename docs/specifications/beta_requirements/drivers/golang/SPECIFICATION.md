# Go (Golang) Driver Specification

Status: Draft (Target). This specification defines the Go (Golang) Driver Specification for ScratchBird V2 using the native wire protocol only.

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
- API: database/sql driver with optional sqlx and GORM support.
- Package/module: github.com/scratchbird/scratchbird-go.
- Parameter style: $1 and :name (rewrite ? to $1..N).
- Transport: TCP with TLS 1.3; optional Unix socket where supported.

## database/sql API surface (required interfaces)
These are the minimum database/sql/driver interfaces the driver must implement.
Unsupported features should return driver.ErrSkip or a descriptive error.

### Driver and connector
- driver.Driver: Open(name string) (driver.Conn, error)
- driver.DriverContext: OpenConnector(name string) (driver.Connector, error)
- driver.Connector: Connect(ctx context.Context) (driver.Conn, error); Driver() driver.Driver

### Connection
- PrepareContext(ctx context.Context, query string) (driver.Stmt, error)
- BeginTx(ctx context.Context, opts driver.TxOptions) (driver.Tx, error)
- ExecContext(ctx context.Context, query string, args []driver.NamedValue) (driver.Result, error)
- QueryContext(ctx context.Context, query string, args []driver.NamedValue) (driver.Rows, error)
- Ping(ctx context.Context) error
- ResetSession(ctx context.Context) error (optional but preferred)
- Close() error
- NamedValueChecker.CheckNamedValue(*driver.NamedValue) error (to support :name)

### Statement
- ExecContext(ctx context.Context, args []driver.NamedValue) (driver.Result, error)
- QueryContext(ctx context.Context, args []driver.NamedValue) (driver.Rows, error)
- NumInput() int (use -1 for variable)
- Close() error

### Transaction
- Commit() error
- Rollback() error

### Rows and column metadata
- Columns() []string
- Next(dest []driver.Value) error
- Close() error
- Optional: NextResultSet(), ColumnTypeDatabaseTypeName(), ColumnTypeNullable(),
  ColumnTypeLength(), ColumnTypePrecisionScale(), ColumnTypeScanType()

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
| ScratchBird type | Go type | Notes |
| --- | --- | --- |
| BOOLEAN | bool | |
| INT* | int8/int16/int32/int64 | |
| UINT* | uint8/uint16/uint32/uint64 | |
| INT128/UINT128 | *big.Int | |
| DECIMAL/MONEY | decimal.Decimal | shopspring or apd |
| FLOAT* | float32/float64 | |
| TEXT | string | UTF-8 |
| BINARY | []byte | |
| DATE/TIME/TIMESTAMP | time.Time | local/date wrappers |
| UUID | uuid.UUID | github.com/google/uuid |
| JSON | json.RawMessage | |
| JSONB | scratchbird.JSONB | |
| ARRAY | []any/[]T | |
| RANGE | scratchbird.Range[T] | |
| VECTOR | []float32 | |
| INET/CIDR | net.IP/net.IPNet | |
| GEOMETRY | scratchbird.Geometry | WKB/WKT recommended |

## Type mapping (full target)
| ScratchBird type | Go type | Notes |
| --- | --- | --- |
| NULL | nil | for metadata only |
| BOOLEAN | bool | |
| INT8 | int8 | |
| UINT8 | uint8 | |
| INT16 | int16 | |
| UINT16 | uint16 | |
| INT32 | int32 | |
| UINT32 | uint32 | |
| INT64 | int64 | |
| UINT64 | uint64 | use *big.Int if > MaxInt64 |
| INT128 | *big.Int | |
| UINT128 | *big.Int | |
| DECIMAL | decimal.Decimal | shopspring or apd |
| MONEY | decimal.Decimal | scale 4 by default |
| FLOAT32 | float32 | |
| FLOAT64 | float64 | |
| CHAR | string | space padded |
| VARCHAR | string | |
| TEXT | string | |
| JSON | json.RawMessage | configurable decode |
| JSONB | scratchbird.JSONB | wrapper type |
| XML | string | |
| BINARY | []byte | fixed length |
| VARBINARY | []byte | |
| BLOB/BYTEA | []byte | |
| DATE | time.Time | date-only in UTC |
| TIME | time.Duration/time.Time | driver-defined |
| TIMESTAMP | time.Time | timezone preserved |
| INTERVAL | time.Duration | custom for months/years |
| UUID | uuid.UUID | github.com/google/uuid |
| ARRAY | []T | element type preserved |
| RANGE | scratchbird.Range[T] | custom struct |
| COMPOSITE/ROW | struct/[]any | |
| VARIANT | any | includes type tag |
| VECTOR | []float32 | |
| TSVECTOR/TSQUERY | string | |
| INET | net.IP | |
| CIDR | net.IPNet | |
| MACADDR/MACADDR8 | net.HardwareAddr/string | |
| GEOMETRY/POINT/LINESTRING/POLYGON/MULTI*/GEOMETRYCOLLECTION | scratchbird.Geometry | WKB/WKT recommended |

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
| SQLSTATE class | Meaning | Go error type |
| --- | --- | --- |
| 01 | Warning | scratchbird.Error{Kind: ErrWarning} |
| 02 | No data | scratchbird.Error{Kind: ErrNoData} |
| 08 | Connection exception | scratchbird.Error{Kind: ErrConnection} |
| 0A | Feature not supported | scratchbird.Error{Kind: ErrNotSupported} |
| 22 | Data exception | scratchbird.Error{Kind: ErrData} |
| 23 | Integrity constraint violation | scratchbird.Error{Kind: ErrIntegrity} |
| 28 | Invalid authorization | scratchbird.Error{Kind: ErrAuth} |
| 40 | Transaction rollback | scratchbird.Error{Kind: ErrTransaction} |
| 42 | Syntax/access violation | scratchbird.Error{Kind: ErrSyntax} |
| 53 | Insufficient resources | scratchbird.Error{Kind: ErrResource} |
| 54 | Program limit exceeded | scratchbird.Error{Kind: ErrLimit} |
| 57 | Operator intervention | scratchbird.Error{Kind: ErrOperator} |
| 58 | System error | scratchbird.Error{Kind: ErrSystem} |
| XX | Internal error | scratchbird.Error{Kind: ErrInternal} |

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
- Pooling: use database/sql pool defaults (sql.DB settings).
- Logging: disabled by default; integrate with log/slog.

