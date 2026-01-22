# Python Driver Specification

Status: Draft (Target). This specification defines the Python Driver Specification for ScratchBird V2 using the native wire protocol only.

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
- API: PEP 249 DB-API 2.0 driver with optional asyncio.
- Package/module: scratchbird.
- Parameter style: :name and $1 (rewrite ? to $1..N).
- Transport: TCP with TLS 1.3; optional Unix socket where supported.

## DB-API 2.0 surface (required signatures)
The driver must implement the PEP 249 interfaces. Optional features must raise
NotSupportedError when unavailable.

### Module attributes
- apilevel = "2.0"
- threadsafety = 2 (module and connections are shareable)
- paramstyle = "named" (accept :name and $1; rewrite ? -> $1..N internally)

### connect() factory
```python
def connect(dsn=None, user=None, password=None, host=None, database=None, **kwargs) -> Connection:
    ...
```

### Connection
```python
class Connection:
    def close(self) -> None: ...
    def commit(self) -> None: ...
    def rollback(self) -> None: ...
    def cursor(self) -> "Cursor": ...
    def execute(self, sql: str, params=None) -> "Cursor": ...
    def executemany(self, sql: str, seq_of_params) -> "Cursor": ...
    def setinputsizes(self, sizes) -> None: ...
    def setoutputsize(self, size, column=None) -> None: ...
```

### Cursor
```python
class Cursor:
    def execute(self, sql: str, params=None) -> None: ...
    def executemany(self, sql: str, seq_of_params) -> None: ...
    def fetchone(self): ...
    def fetchmany(self, size=None): ...
    def fetchall(self): ...
    def close(self) -> None: ...
    def setinputsizes(self, sizes) -> None: ...
    def setoutputsize(self, size, column=None) -> None: ...
    # Attributes
    description: tuple
    rowcount: int
    arraysize: int
    lastrowid: object
```

### Async API (optional)
```python
async def connect_async(...) -> "AsyncConnection": ...
class AsyncConnection:
    async def cursor(self) -> "AsyncCursor": ...
    async def commit(self) -> None: ...
    async def rollback(self) -> None: ...
class AsyncCursor:
    async def execute(self, sql: str, params=None) -> None: ...
    async def fetchone(self): ...
    async def fetchmany(self, size=None): ...
    async def fetchall(self): ...
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

## Type mapping (summary)
| ScratchBird type | Python type | Notes |
| --- | --- | --- |
| BOOLEAN | bool | |
| INT* / UINT* | int | arbitrary precision |
| INT128/UINT128 | int | |
| DECIMAL/MONEY | decimal.Decimal | |
| FLOAT* | float | |
| TEXT | str | UTF-8 |
| BINARY | bytes/memoryview | |
| DATE/TIME/TIMESTAMP | datetime.date/time/datetime | tz-aware |
| UUID | uuid.UUID | |
| JSON | dict/list/str | |
| JSONB | scratchbird.types.Jsonb | |
| XML | str | |
| ARRAY | list/tuple | |
| RANGE | scratchbird.types.Range | |
| VECTOR | list[float]/numpy.ndarray | optional |
| INET/CIDR | ipaddress objects | |
| GEOMETRY | scratchbird.types.Geometry | optional shapely |

## Type mapping (full target)
| ScratchBird type | Python type | Notes |
| --- | --- | --- |
| NULL | None | for metadata only |
| BOOLEAN | bool | |
| INT8/16/32/64 | int | |
| UINT8/16/32/64 | int | range checks on bind |
| INT128/UINT128 | int | arbitrary precision |
| DECIMAL | decimal.Decimal | preserve scale |
| MONEY | decimal.Decimal | scale 4 by default |
| FLOAT32/FLOAT64 | float | |
| CHAR/VARCHAR/TEXT | str | UTF-8 |
| JSON | dict/list/str | configurable json decoder |
| JSONB | scratchbird.types.Jsonb | wrapper type |
| XML | str | optional ElementTree parsing |
| BINARY/VARBINARY | bytes/memoryview | |
| BLOB/BYTEA | bytes/memoryview | |
| DATE | datetime.date | |
| TIME | datetime.time | tz-aware where possible |
| TIMESTAMP | datetime.datetime | tz-aware |
| INTERVAL | datetime.timedelta | or dateutil.relativedelta |
| UUID | uuid.UUID | |
| ARRAY | list/tuple | element type preserved |
| RANGE | scratchbird.types.Range | wrapper type |
| COMPOSITE/ROW | tuple | or dataclass mapping |
| VARIANT | object | includes type tag |
| VECTOR | list[float]/numpy.ndarray | optional |
| TSVECTOR/TSQUERY | str | |
| INET | ipaddress.IPv4Address/IPv6Address | |
| CIDR | ipaddress.IPv4Network/IPv6Network | |
| MACADDR/MACADDR8 | bytes/str | optional netaddr.EUI |
| GEOMETRY/POINT/LINESTRING/POLYGON/MULTI*/GEOMETRYCOLLECTION | scratchbird.types.Geometry | WKB/WKT recommended |

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
| SQLSTATE class | Meaning | DB-API exception |
| --- | --- | --- |
| 01 | Warning | Warning |
| 02 | No data | Warning |
| 08 | Connection exception | OperationalError |
| 0A | Feature not supported | NotSupportedError |
| 22 | Data exception | DataError |
| 23 | Integrity constraint violation | IntegrityError |
| 28 | Invalid authorization | OperationalError |
| 40 | Transaction rollback | OperationalError |
| 42 | Syntax/access violation | ProgrammingError |
| 53 | Insufficient resources | OperationalError |
| 54 | Program limit exceeded | OperationalError |
| 57 | Operator intervention | OperationalError |
| 58 | System error | InternalError |
| XX | Internal error | InternalError |

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
- Pooling: external pool (SQLAlchemy/pool); driver does not pool by default.
- Logging: disabled by default; integrate with Python logging.

