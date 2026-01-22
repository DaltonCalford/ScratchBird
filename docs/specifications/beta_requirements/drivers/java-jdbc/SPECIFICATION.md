# Java JDBC Driver Specification

Status: Draft (Target). This specification defines the Java JDBC Driver Specification for ScratchBird V2 using the native wire protocol only.

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
- API: JDBC 4.2+ Type 4 driver.
- Package/module: org.scratchbird:scratchbird-jdbc.
- Parameter style: $1 and :name (rewrite ? to $1..N).
- Transport: TCP with TLS 1.3; optional Unix socket where supported.

## JDBC API surface (required signatures)
These are the minimum JDBC 4.2 entry points the driver must implement. Unsupported
features must throw SQLFeatureNotSupportedException.

### Driver and DataSource
- java.sql.Driver:
  - Connection connect(String url, Properties info)
  - boolean acceptsURL(String url)
  - DriverPropertyInfo[] getPropertyInfo(String url, Properties info)
  - int getMajorVersion()
  - int getMinorVersion()
  - boolean jdbcCompliant()
  - Logger getParentLogger()
- javax.sql.DataSource:
  - Connection getConnection()
  - Connection getConnection(String user, String password)
  - void setLoginTimeout(int seconds) / int getLoginTimeout()
  - void setLogWriter(PrintWriter out) / PrintWriter getLogWriter()
  - Logger getParentLogger()

### Connection
- Statement createStatement()
- PreparedStatement prepareStatement(String sql)
- CallableStatement prepareCall(String sql) (optional)
- void setAutoCommit(boolean autoCommit) / boolean getAutoCommit()
- void commit() / void rollback()
- Savepoint setSavepoint() / Savepoint setSavepoint(String name)
- void setTransactionIsolation(int level) / int getTransactionIsolation()
- DatabaseMetaData getMetaData()
- void close() / boolean isClosed()

### Statement / PreparedStatement
- ResultSet executeQuery(String sql)
- int executeUpdate(String sql)
- boolean execute(String sql)
- void addBatch(String sql) / int[] executeBatch()
- ResultSet getResultSet()
- int getUpdateCount()
- void setQueryTimeout(int seconds)
- PreparedStatement parameter binding: setNull/setBoolean/setInt/setLong/setBigDecimal/setString/setBytes/setObject

### ResultSet
- boolean next()
- Object getObject(int columnIndex) / Object getObject(String columnLabel)
- getString/getInt/getLong/getBigDecimal/getBytes/getTimestamp/getDate/getTime/getArray/getBlob/getClob
- ResultSetMetaData getMetaData()
- void close()

### DatabaseMetaData (subset)
- ResultSet getTables(...)
- ResultSet getColumns(...)
- ResultSet getSchemas(...)
- ResultSet getCatalogs()
- ResultSet getPrimaryKeys(...)
- ResultSet getIndexInfo(...)
- ResultSet getProcedures(...)
- ResultSet getFunctions(...)
- boolean supportsTransactions()
- int getDefaultTransactionIsolation()

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
| ScratchBird type | Java type | Notes |
| --- | --- | --- |
| BOOLEAN | boolean | |
| INT8/16/32/64 | byte/short/int/long | |
| UINT* | use larger signed or BigInteger | |
| INT128/UINT128 | BigInteger | |
| DECIMAL/MONEY | BigDecimal | |
| FLOAT* | float/double | |
| TEXT | String | UTF-8 |
| BINARY | byte[] | |
| DATE | LocalDate | |
| TIME | LocalTime | |
| TIMESTAMP | OffsetDateTime/Instant | |
| UUID | java.util.UUID | |
| JSON | String | optional JSON lib |
| JSONB | SBJsonb | |
| XML | String | |
| ARRAY | List/Object[] | |
| RANGE | SBRange<T> | custom wrapper |
| VECTOR | float[] | |
| INET/CIDR | InetAddress/IpPrefix | custom |
| GEOMETRY | SBGeometry | WKB/WKT recommended |

## Type mapping (full target)
| ScratchBird type | JDBC Types | Java type | Notes |
| --- | --- | --- | --- |
| NULL | NULL | null | for metadata only |
| BOOLEAN | BOOLEAN | boolean/Boolean | |
| INT8 | TINYINT | byte/Byte | |
| UINT8 | SMALLINT | short/Short | range 0..255 |
| INT16 | SMALLINT | short/Short | |
| UINT16 | INTEGER | int/Integer | range 0..65535 |
| INT32 | INTEGER | int/Integer | |
| UINT32 | BIGINT | long/Long | range 0..4294967295 |
| INT64 | BIGINT | long/Long | |
| UINT64 | NUMERIC/DECIMAL | BigInteger | allow Long if <= Long.MAX_VALUE |
| INT128 | NUMERIC/DECIMAL | BigInteger | |
| UINT128 | NUMERIC/DECIMAL | BigInteger | |
| DECIMAL | DECIMAL | BigDecimal | precision/scale from metadata |
| MONEY | DECIMAL | BigDecimal | scale 4 by default |
| FLOAT32 | REAL | float/Float | |
| FLOAT64 | DOUBLE | double/Double | |
| CHAR | CHAR | String | space padded |
| VARCHAR | VARCHAR | String | |
| TEXT | LONGVARCHAR | String | |
| JSON | LONGVARCHAR | String | optional JSON library |
| JSONB | OTHER | SBJsonb | wrapper type |
| XML | SQLXML/LONGVARCHAR | SQLXML/String | |
| BINARY | BINARY | byte[] | zero padded |
| VARBINARY | VARBINARY | byte[] | |
| BLOB/BYTEA | BLOB | byte[]/Blob | |
| DATE | DATE | LocalDate | |
| TIME | TIME | LocalTime | timezone offset preserved in metadata |
| TIMESTAMP | TIMESTAMP_WITH_TIMEZONE | OffsetDateTime/Instant | fallback to TIMESTAMP |
| INTERVAL | OTHER | Duration/Period | custom wrapper if needed |
| UUID | OTHER | java.util.UUID | |
| ARRAY | ARRAY | java.sql.Array | element type required |
| RANGE | OTHER | SBRange<T> | custom type |
| COMPOSITE/ROW | STRUCT | java.sql.Struct | |
| VARIANT | OTHER | Object | includes type tag |
| VECTOR | OTHER | float[] | optional custom vector |
| TSVECTOR/TSQUERY | OTHER | String | |
| INET | OTHER | InetAddress | |
| CIDR | OTHER | IpPrefix | custom |
| MACADDR/MACADDR8 | OTHER | byte[]/String | |
| GEOMETRY/POINT/LINESTRING/POLYGON/MULTI*/GEOMETRYCOLLECTION | OTHER | SBGeometry | WKB/WKT recommended |

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
| SQLSTATE class | Meaning | JDBC exception |
| --- | --- | --- |
| 01 | Warning | SQLWarning |
| 02 | No data | SQLNoDataException |
| 08 | Connection exception | SQLNonTransientConnectionException / SQLTransientConnectionException |
| 0A | Feature not supported | SQLFeatureNotSupportedException |
| 22 | Data exception | SQLDataException |
| 23 | Integrity constraint violation | SQLIntegrityConstraintViolationException |
| 28 | Invalid authorization | SQLInvalidAuthorizationSpecException |
| 40 | Transaction rollback | SQLTransactionRollbackException |
| 42 | Syntax/access violation | SQLSyntaxErrorException |
| 53 | Insufficient resources | SQLTransientException |
| 54 | Program limit exceeded | SQLNonTransientException |
| 57 | Operator intervention | SQLTransientException |
| 58 | System error | SQLNonTransientException |
| XX | Internal error | SQLNonTransientException |

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
- Pooling: DataSource + external pool (HikariCP or app server pools).
- Logging: disabled by default; integrate with java.util.logging or SLF4J.

