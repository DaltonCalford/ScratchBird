# PHP Driver Specification

Status: Draft (Target). This specification defines the PHP Driver Specification for ScratchBird V2 using the native wire protocol only.

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
- API: PDO driver (pdo_scratchbird) with mysqli compatibility layer.
- Package/module: scratchbird/pdo-scratchbird.
- Parameter style: :name and $1 (rewrite ? to $1..N).
- Transport: TCP with TLS 1.3; optional Unix socket where supported.

## PDO API surface (required signatures)
The driver must expose a PDO-compatible API. Unsupported features must raise
PDOException or return false as appropriate.

### PDO
```php
public function __construct(string $dsn, ?string $username = null, ?string $password = null, array $options = []);
public function prepare(string $statement, array $options = []): PDOStatement|false;
public function query(string $statement, mixed ...$fetch_mode_args): PDOStatement|false;
public function exec(string $statement): int|false;
public function beginTransaction(): bool;
public function commit(): bool;
public function rollBack(): bool;
public function lastInsertId(?string $name = null): string|false;
public function setAttribute(int $attribute, mixed $value): bool;
public function getAttribute(int $attribute): mixed;
public function errorInfo(): array;
public function errorCode(): ?string;
```

### PDOStatement
```php
public function bindParam(string|int $param, mixed &$var, int $type = PDO::PARAM_STR, int $length = 0, mixed $driver_options = null): bool;
public function bindValue(string|int $param, mixed $value, int $type = PDO::PARAM_STR): bool;
public function execute(?array $params = null): bool;
public function fetch(int $mode = PDO::FETCH_ASSOC, mixed ...$args): mixed;
public function fetchAll(int $mode = PDO::FETCH_ASSOC, mixed ...$args): array;
public function fetchColumn(int $column = 0): mixed;
public function rowCount(): int;
public function columnCount(): int;
public function getColumnMeta(int $column): array;
public function closeCursor(): bool;
public function setFetchMode(int $mode, mixed ...$args): bool;
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
| ScratchBird type | PHP type | Notes |
| --- | --- | --- |
| BOOLEAN | bool | |
| INT32/INT64 | int | 64-bit builds |
| UINT64 | string | if overflow |
| INT128/UINT128 | string | |
| DECIMAL/MONEY | string | use bcmath if needed |
| FLOAT* | float | |
| TEXT | string | UTF-8 |
| BINARY | string | binary-safe |
| DATE/TIME/TIMESTAMP | DateTimeImmutable | |
| UUID | string | canonical |
| JSON | string/array | |
| JSONB | ScratchBird\\PDO\\Jsonb | |
| XML | string | |
| ARRAY | array | |
| RANGE | ScratchBird\\PDO\\Range | |
| VECTOR | array of float | |
| INET/CIDR/MACADDR | string | |
| GEOMETRY | ScratchBird\\PDO\\Geometry | WKB/WKT or GeoJSON |

## Type mapping (full target)
| ScratchBird type | PHP type | Notes |
| --- | --- | --- |
| NULL | null | for metadata only |
| BOOLEAN | bool | |
| INT8/INT16/INT32 | int | 64-bit builds |
| UINT8/UINT16/UINT32 | int | range checks |
| INT64 | int | 64-bit builds |
| UINT64 | string | if overflow |
| INT128/UINT128 | string | |
| DECIMAL | string | or ext-bcmath |
| MONEY | string | scale 4 by default |
| FLOAT32/FLOAT64 | float | |
| CHAR/VARCHAR/TEXT | string | UTF-8 |
| JSON | array/object/string | configurable decode |
| JSONB | ScratchBird\\PDO\\Jsonb | wrapper type |
| XML | string | optional SimpleXML |
| BINARY/VARBINARY | string | binary-safe |
| BLOB/BYTEA | string/resource | stream for large values |
| DATE | DateTimeImmutable | or string |
| TIME | DateTimeImmutable | or string |
| TIMESTAMP | DateTimeImmutable | tz-aware |
| INTERVAL | DateInterval | |
| UUID | string | canonical |
| ARRAY | array | element type preserved |
| RANGE | ScratchBird\\PDO\\Range | wrapper type |
| COMPOSITE/ROW | array | or DTO mapping |
| VARIANT | mixed | includes type tag |
| VECTOR | array of float | |
| TSVECTOR/TSQUERY | string | |
| INET | string | |
| CIDR | string | |
| MACADDR/MACADDR8 | string | |
| GEOMETRY/POINT/LINESTRING/POLYGON/MULTI*/GEOMETRYCOLLECTION | ScratchBird\\PDO\\Geometry | WKB/WKT or GeoJSON |

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
| SQLSTATE class | Meaning | PHP exception |
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
- Pooling: no built-in pool; use persistent connections when enabled by runtime.
- Logging: disabled by default; integrate with PSR-3 logger.

