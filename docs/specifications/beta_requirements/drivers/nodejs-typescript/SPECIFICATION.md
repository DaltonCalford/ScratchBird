# Node.js / TypeScript Driver Specification

Status: Draft (Target). This specification defines the Node.js / TypeScript Driver Specification for ScratchBird V2 using the native wire protocol only.

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
- API: Async client and pool with TypeScript types.
- Package/module: scratchbird.
- Parameter style: $1 and :name (rewrite ? to $1..N).
- Transport: TCP with TLS 1.3; optional Unix socket where supported.

## API surface (required signatures)
The driver must provide Promise-based APIs for client and pool usage.

```ts
export interface ClientConfig {
  host?: string;
  port?: number;
  user?: string;
  password?: string;
  database?: string;
  ssl?: boolean | object;
  connectTimeoutMs?: number;
  applicationName?: string;
  binaryTransfer?: boolean;
  compression?: "zstd" | "off";
}

export interface FieldDef {
  name: string;
  dataType: string;
  format: "text" | "binary";
  nullable: boolean;
}

export interface QueryResult<T = any> {
  rows: T[];
  rowCount: number;
  fields: FieldDef[];
  command: string;
}

export class Client {
  constructor(config?: ClientConfig | string);
  connect(): Promise<void>;
  query<T = any>(text: string, params?: any[] | Record<string, any>): Promise<QueryResult<T>>;
  prepare(name: string, text: string, paramTypes?: string[]): Promise<void>;
  execute<T = any>(name: string, params?: any[] | Record<string, any>): Promise<QueryResult<T>>;
  begin(): Promise<void>;
  commit(): Promise<void>;
  rollback(): Promise<void>;
  end(): Promise<void>;
}

export class Pool {
  constructor(config?: ClientConfig | string);
  connect(): Promise<Client>;
  query<T = any>(text: string, params?: any[] | Record<string, any>): Promise<QueryResult<T>>;
  end(): Promise<void>;
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
| ScratchBird type | JS/TS type | Notes |
| --- | --- | --- |
| BOOLEAN | boolean | |
| INT32 | number | |
| INT64/UINT64 | bigint | avoid number overflow |
| INT128/UINT128 | bigint or string | |
| DECIMAL/MONEY | string or bigint+scale | |
| FLOAT* | number | |
| TEXT | string | UTF-8 |
| BINARY | Buffer/Uint8Array | |
| DATE/TIME/TIMESTAMP | Date or string | prefer string for DATE/TIME |
| UUID | string | canonical form |
| JSON | object/string | |
| JSONB | ScratchbirdJsonb | |
| ARRAY | any[] | |
| RANGE | ScratchbirdRange<T> | |
| VECTOR | Float32Array | |
| INET/CIDR/MACADDR | string | |
| GEOMETRY | ScratchbirdGeometry | GeoJSON or WKB/WKT |

## Type mapping (full target)
| ScratchBird type | JS/TS type | Notes |
| --- | --- | --- |
| NULL | null | for metadata only |
| BOOLEAN | boolean | |
| INT8 | number | |
| UINT8 | number | |
| INT16 | number | |
| UINT16 | number | |
| INT32 | number | |
| UINT32 | number | |
| INT64 | bigint | use number only if safe |
| UINT64 | bigint | or string if JSON serialization needed |
| INT128 | bigint | or string for JSON |
| UINT128 | bigint | or string for JSON |
| DECIMAL | string | or Decimal.js/Big.js |
| MONEY | string | scale 4 by default |
| FLOAT32 | number | |
| FLOAT64 | number | |
| CHAR | string | space padded |
| VARCHAR | string | |
| TEXT | string | |
| JSON | object/string | configurable decoder |
| JSONB | ScratchbirdJsonb | wrapper type |
| XML | string | |
| BINARY | Buffer/Uint8Array | fixed length |
| VARBINARY | Buffer/Uint8Array | |
| BLOB/BYTEA | Buffer/Uint8Array | |
| DATE | string | YYYY-MM-DD |
| TIME | string | HH:MM:SS[.ffffff] |
| TIMESTAMP | string/Date | ISO 8601 |
| INTERVAL | string/object | custom parser |
| UUID | string | canonical |
| ARRAY | any[] | element type preserved |
| RANGE | ScratchbirdRange<T> | wrapper type |
| COMPOSITE/ROW | object/any[] | |
| VARIANT | any | includes type tag |
| VECTOR | Float32Array/number[] | |
| TSVECTOR/TSQUERY | string | |
| INET | string | |
| CIDR | string | |
| MACADDR/MACADDR8 | string | |
| GEOMETRY/POINT/LINESTRING/POLYGON/MULTI*/GEOMETRYCOLLECTION | ScratchbirdGeometry | GeoJSON or WKB/WKT |

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
| SQLSTATE class | Meaning | JS/TS error class |
| --- | --- | --- |
| 01 | Warning | ScratchbirdWarning |
| 02 | No data | ScratchbirdNoDataError |
| 08 | Connection exception | ScratchbirdConnectionError |
| 0A | Feature not supported | ScratchbirdNotSupportedError |
| 22 | Data exception | ScratchbirdDataError |
| 23 | Integrity constraint violation | ScratchbirdIntegrityError |
| 28 | Invalid authorization | ScratchbirdAuthError |
| 40 | Transaction rollback | ScratchbirdTransactionError |
| 42 | Syntax/access violation | ScratchbirdSyntaxError |
| 53 | Insufficient resources | ScratchbirdResourceError |
| 54 | Program limit exceeded | ScratchbirdLimitError |
| 57 | Operator intervention | ScratchbirdOperatorInterventionError |
| 58 | System error | ScratchbirdSystemError |
| XX | Internal error | ScratchbirdInternalError |

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
- Pooling: optional Pool class; off unless explicitly used.
- Logging: disabled by default; integrate with standard Node.js loggers.

