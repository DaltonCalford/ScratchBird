# API Reference (JDBC)

## Wrapper Types
- JSONB: `SBJsonb`
- RANGE: `SBRange<T>`
- GEOMETRY: `SBGeometry`


## Driver and DataSource
- Driver.connect(url, props)
- Driver.acceptsURL(url)
- Driver.getPropertyInfo(url, props)
- Driver.getMajorVersion() / getMinorVersion()
- Driver.jdbcCompliant()
- Driver.getParentLogger()
- DataSource.getConnection()
- DataSource.getConnection(user, password)
- DataSource.setLoginTimeout() / getLoginTimeout()
- DataSource.setLogWriter() / getLogWriter()
- DataSource.getParentLogger()

## Connection
- createStatement()
- prepareStatement(sql)
- prepareCall(sql) (optional)
- setAutoCommit() / getAutoCommit()
- commit() / rollback()
- setSavepoint() / setSavepoint(name)
- setTransactionIsolation() / getTransactionIsolation()
- getMetaData()
- close() / isClosed()

## Statement / PreparedStatement
- executeQuery(sql) / executeUpdate(sql) / execute(sql)
- addBatch(sql) / executeBatch()
- getResultSet() / getUpdateCount()
- setQueryTimeout(seconds)
- PreparedStatement bindings: setNull/setBoolean/setInt/setLong/setBigDecimal/setString/setBytes/setObject

## ResultSet
- next()
- getObject(index/label)
- getString/getInt/getLong/getBigDecimal/getBytes/getTimestamp/getDate/getTime/getArray/getBlob/getClob
- getMetaData()
- close()

## DatabaseMetaData (subset)
- getTables/getColumns/getSchemas/getCatalogs
- getPrimaryKeys/getIndexInfo
- getProcedures/getFunctions
- supportsTransactions
- getDefaultTransactionIsolation

## Usage examples
```java
Connection conn = DriverManager.getConnection(
    "scratchbird://user:pass@localhost:3092/db");
Statement st = conn.createStatement();
ResultSet rs = st.executeQuery("select 1 as one");

PreparedStatement ps = conn.prepareStatement(
    "select * from widgets where id = $1");
ps.setInt(1, 42);
ResultSet rows = ps.executeQuery();
```
