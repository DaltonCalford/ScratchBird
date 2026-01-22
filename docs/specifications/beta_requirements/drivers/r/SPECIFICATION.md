# R Driver Specification

Status: Draft (Target). This specification defines the R Driver Specification for ScratchBird V2 using the native wire protocol only.

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
- API: DBI-compatible driver.
- Package/module: DBI + scratchbird.
- Parameter style: $1 and :name (rewrite ? to $1..N).
- Transport: TCP with TLS 1.3; optional Unix socket where supported.

## DBI API surface (required generics)
The driver must implement core DBI generics and return DBI-compliant classes.

```r
con <- dbConnect(Scratchbird(), host=..., port=..., user=..., password=..., dbname=...)
dbDisconnect(con)

dbExecute(con, statement, params = list())
dbGetQuery(con, statement, params = list())
res <- dbSendQuery(con, statement, params = list())
dbFetch(res, n = -1)
dbClearResult(res)

dbBegin(con)
dbCommit(con)
dbRollback(con)

dbColumnInfo(res)
dbGetInfo(con)
dbListTables(con)        # optional
dbListFields(con, name)  # optional
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
| ScratchBird type | R type | Notes |
| --- | --- | --- |
| BOOLEAN | logical | |
| INT32 | integer | |
| INT64/UINT64 | bit64::integer64 | use bit64 |
| INT128/UINT128 | character | use string |
| DECIMAL/MONEY | numeric or decimal | use string if precision |
| FLOAT* | numeric (double) | |
| TEXT | character | UTF-8 |
| BINARY | raw | |
| DATE | Date | |
| TIME/TIMESTAMP | POSIXct | |
| UUID | character | |
| JSON | list or character | |
| JSONB | sb_jsonb | S3 class |
| XML | character | |
| ARRAY | list | |
| RANGE | sb_range | S3 class |
| VECTOR | numeric vector | |
| INET/CIDR | character | or iptools |
| GEOMETRY | sb_geometry | S3 class |

## Type mapping (full target)
| ScratchBird type | R type | Notes |
| --- | --- | --- |
| NULL | NA/NULL | for metadata only |
| BOOLEAN | logical | |
| INT8 | integer | |
| UINT8 | integer | range checks |
| INT16 | integer | |
| UINT16 | integer | range checks |
| INT32 | integer | |
| UINT32 | numeric/bit64::integer64 | |
| INT64 | bit64::integer64 | |
| UINT64 | bit64::integer64/character | if overflow |
| INT128 | character | |
| UINT128 | character | |
| DECIMAL | numeric/character | use string for precision |
| MONEY | numeric/character | scale 4 by default |
| FLOAT32 | numeric | |
| FLOAT64 | numeric | |
| CHAR | character | fixed length |
| VARCHAR | character | |
| TEXT | character | |
| JSON | list/character | jsonlite optional |
| JSONB | sb_jsonb | S3 class |
| XML | character | or xml2 |
| BINARY | raw | fixed length |
| VARBINARY | raw | |
| BLOB/BYTEA | raw | |
| DATE | Date | |
| TIME | hms::hms | or character |
| TIMESTAMP | POSIXct | tz-aware |
| INTERVAL | difftime | |
| UUID | character | |
| ARRAY | list | element type preserved |
| RANGE | sb_range | S3 class |
| COMPOSITE/ROW | list/data.frame | |
| VARIANT | list | includes type tag |
| VECTOR | numeric vector | |
| TSVECTOR/TSQUERY | character | |
| INET | character | |
| CIDR | character | |
| MACADDR/MACADDR8 | character | |
| GEOMETRY/POINT/LINESTRING/POLYGON/MULTI*/GEOMETRYCOLLECTION | sb_geometry | WKB/WKT recommended |

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
| SQLSTATE class | Meaning | R condition class |
| --- | --- | --- |
| 01 | Warning | scratchbird_warning |
| 02 | No data | scratchbird_no_data |
| 08 | Connection exception | scratchbird_connection_error |
| 0A | Feature not supported | scratchbird_not_supported_error |
| 22 | Data exception | scratchbird_data_error |
| 23 | Integrity constraint violation | scratchbird_integrity_error |
| 28 | Invalid authorization | scratchbird_auth_error |
| 40 | Transaction rollback | scratchbird_transaction_error |
| 42 | Syntax/access violation | scratchbird_syntax_error |
| 53 | Insufficient resources | scratchbird_resource_error |
| 54 | Program limit exceeded | scratchbird_limit_error |
| 57 | Operator intervention | scratchbird_operator_intervention_error |
| 58 | System error | scratchbird_system_error |
| XX | Internal error | scratchbird_internal_error |

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
- Pooling: external pool (DBI/pool); driver does not pool by default.
- Logging: disabled by default; integrate with base R logging facilities.

