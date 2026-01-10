# ScratchBird JDBC Driver Specification

## 1. Overview

### 1.1 Purpose

The ScratchBird JDBC driver provides standard JDBC connectivity for:
1. **Java Applications** connecting TO ScratchBird databases
2. **ScratchBird Foreign Tables** connecting FROM ScratchBird to external databases via JDBC

**Scope Note:** MSSQL external connectivity is post-gold; MSSQL entries are forward-looking.

### 1.2 JDBC Version

- **JDBC 4.3** compliance (Java 9+)
- **JDBC 4.2** compatibility mode (Java 8)
- Service Provider Interface (SPI) auto-loading
- Connection pooling compatible (HikariCP, C3P0, Apache DBCP)

### 1.3 Artifacts

```xml
<!-- Maven dependency -->
<dependency>
    <groupId>com.scratchbird</groupId>
    <artifactId>scratchbird-jdbc</artifactId>
    <version>1.0.0</version>
</dependency>

<!-- Gradle -->
implementation 'com.scratchbird:scratchbird-jdbc:1.0.0'
```

**JAR Files:**
- `scratchbird-jdbc-1.0.0.jar` - Main driver
- `scratchbird-jdbc-1.0.0-all.jar` - Fat JAR with all dependencies

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  Java Application (Spring, Hibernate, etc.)                      │
└─────────────────────┬───────────────────────────────────────────┘
                      │ JDBC API (java.sql.*)
┌─────────────────────▼───────────────────────────────────────────┐
│  ScratchBird JDBC Driver (Type 4 - Pure Java)                    │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  SBDriver                                                 │  │
│  │  - URL parsing                                            │  │
│  │  - Connection factory                                     │  │
│  │  - SPI registration                                       │  │
│  └───────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  SBConnection                                             │  │
│  │  - Session management                                     │  │
│  │  - Transaction control                                    │  │
│  │  - Statement factory                                      │  │
│  └───────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  Protocol Layer                                           │  │
│  │  - ScratchBird Native Protocol (default)                  │  │
│  │  - SSL/TLS support                                        │  │
│  │  - Connection multiplexing                                │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────┬───────────────────────────────────────────┘
                      │ TCP/IP
┌─────────────────────▼───────────────────────────────────────────┐
│  ScratchBird Server (port 3092)                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Connection URL

### 3.1 URL Format

```
jdbc:scratchbird://host[:port]/database[?param1=value1&param2=value2]
```

### 3.2 URL Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `user` | - | Username |
| `password` | - | Password |
| `ssl` | prefer | disable, allow, prefer, require, verify-ca, verify-full |
| `sslcert` | - | Client certificate path |
| `sslkey` | - | Client private key path |
| `sslrootcert` | - | CA certificate path |
| `sslmode` | - | Alias for ssl |
| `connectTimeout` | 30 | Connection timeout (seconds) |
| `socketTimeout` | 0 | Socket timeout (seconds, 0=unlimited) |
| `loginTimeout` | 30 | Login timeout (seconds) |
| `tcpKeepAlive` | true | Enable TCP keepalive |
| `currentSchema` | public | Default schema |
| `ApplicationName` | - | Application identifier |
| `readOnly` | false | Read-only connection |
| `autoCommit` | true | Auto-commit mode |
| `defaultRowFetchSize` | 0 | Fetch size (0=all) |
| `prepareThreshold` | 5 | Prepare after N executions |
| `binaryTransfer` | true | Use binary protocol |
| `reWriteBatchedInserts` | false | Optimize batch inserts |
| `loggerLevel` | OFF | OFF, DEBUG, TRACE |
| `loggerFile` | - | Log file path |

### 3.3 Example URLs

```java
// Basic connection
jdbc:scratchbird://localhost:3092/mydb

// With credentials in URL (not recommended)
jdbc:scratchbird://localhost:3092/mydb?user=admin&password=secret

// SSL connection
jdbc:scratchbird://db.example.com:3092/production?ssl=verify-full&sslrootcert=/path/to/ca.crt

// Full configuration
jdbc:scratchbird://localhost:3092/mydb?currentSchema=app&connectTimeout=10&socketTimeout=300&ApplicationName=MyApp&binaryTransfer=true&reWriteBatchedInserts=true
```

---

## 4. Driver Class

### 4.1 Automatic Registration (JDBC 4.0+)

The driver auto-registers via SPI. No explicit `Class.forName()` needed.

```java
// Just get connection - driver loads automatically
Connection conn = DriverManager.getConnection(
    "jdbc:scratchbird://localhost:3092/mydb",
    "user", "password"
);
```

### 4.2 Manual Registration (Legacy)

```java
// For older environments or explicit control
Class.forName("com.scratchbird.jdbc.SBDriver");

// Or
DriverManager.registerDriver(new com.scratchbird.jdbc.SBDriver());
```

### 4.3 Driver Properties

```java
Driver driver = DriverManager.getDriver("jdbc:scratchbird://localhost/db");

// Get driver version
int majorVersion = driver.getMajorVersion();  // 1
int minorVersion = driver.getMinorVersion();  // 0

// Check if URL is valid
boolean valid = driver.acceptsURL("jdbc:scratchbird://localhost/db");

// Get connection properties
DriverPropertyInfo[] props = driver.getPropertyInfo(url, null);
```

---

## 5. JDBC API Implementation

### 5.1 Connection Interface

```java
public interface SBConnection extends Connection {
    // Standard Connection methods fully implemented

    // ScratchBird extensions
    void cancelQuery() throws SQLException;
    void setSchema(String schema) throws SQLException;
    String getSchema() throws SQLException;
    PGNotification[] getNotifications() throws SQLException;
    void addNotificationListener(String channel, NotificationListener listener);
    void removeNotificationListener(String channel);

    // Copy operations
    CopyManager getCopyAPI();

    // Large object support
    LargeObjectManager getLargeObjectAPI();
}
```

### 5.2 Statement Interface

```java
public interface SBStatement extends Statement {
    // Standard Statement methods

    // Extensions
    long getLargeUpdateCount() throws SQLException;  // > Integer.MAX_VALUE
    void setFetchDirection(int direction) throws SQLException;
    void setQueryTimeout(int seconds) throws SQLException;
    void cancel() throws SQLException;

    // Warnings
    SQLWarning getWarnings() throws SQLException;
    void clearWarnings() throws SQLException;
}
```

### 5.3 PreparedStatement Interface

```java
public interface SBPreparedStatement extends PreparedStatement {
    // Standard PreparedStatement methods

    // Array support
    void setArray(int i, Array x) throws SQLException;
    void setObject(int i, Object x, SQLType targetType) throws SQLException;

    // JSON support
    void setJson(int i, String json) throws SQLException;
    void setJsonb(int i, byte[] jsonb) throws SQLException;

    // UUID support
    void setUUID(int i, UUID uuid) throws SQLException;

    // Network types
    void setInet(int i, InetAddress addr) throws SQLException;

    // Batch optimization
    void addBatch() throws SQLException;
    long[] executeLargeBatch() throws SQLException;
}
```

### 5.4 ResultSet Interface

```java
public interface SBResultSet extends ResultSet {
    // Standard ResultSet methods

    // Array retrieval
    Array getArray(int columnIndex) throws SQLException;
    Array getArray(String columnLabel) throws SQLException;

    // JSON retrieval
    String getJson(int columnIndex) throws SQLException;
    String getJson(String columnLabel) throws SQLException;

    // UUID retrieval
    UUID getUUID(int columnIndex) throws SQLException;
    UUID getUUID(String columnLabel) throws SQLException;

    // Network types
    InetAddress getInet(int columnIndex) throws SQLException;

    // Interval type
    PGInterval getInterval(int columnIndex) throws SQLException;
}
```

### 5.5 DatabaseMetaData Interface

All standard `DatabaseMetaData` methods implemented:

```java
DatabaseMetaData meta = conn.getMetaData();

// Database info
meta.getDatabaseProductName();      // "ScratchBird"
meta.getDatabaseProductVersion();   // "1.0.0"
meta.getDriverName();               // "ScratchBird JDBC Driver"
meta.getDriverVersion();            // "1.0.0"

// Catalog methods
meta.getCatalogs();
meta.getSchemas();
meta.getTables(catalog, schema, table, types);
meta.getColumns(catalog, schema, table, column);
meta.getPrimaryKeys(catalog, schema, table);
meta.getImportedKeys(catalog, schema, table);
meta.getExportedKeys(catalog, schema, table);
meta.getIndexInfo(catalog, schema, table, unique, approximate);
meta.getProcedures(catalog, schema, procedure);
meta.getProcedureColumns(catalog, schema, procedure, column);
meta.getFunctions(catalog, schema, function);
meta.getFunctionColumns(catalog, schema, function, column);

// Feature support
meta.supportsTransactions();        // true
meta.supportsBatchUpdates();        // true
meta.supportsSavepoints();          // true
meta.supportsStoredProcedures();    // true
meta.supportsFullOuterJoins();      // true
meta.supportsGroupBy();             // true
meta.supportsUnion();               // true
meta.supportsSubqueriesInExists();  // true
```

---

## 6. Data Type Mapping

### 6.1 ScratchBird to Java Type Mapping

| ScratchBird Type | JDBC Type | Java Type | ResultSet Method |
|------------------|-----------|-----------|------------------|
| BOOLEAN | BOOLEAN | boolean | `getBoolean()` |
| SMALLINT | SMALLINT | short | `getShort()` |
| INTEGER | INTEGER | int | `getInt()` |
| BIGINT | BIGINT | long | `getLong()` |
| REAL | REAL | float | `getFloat()` |
| DOUBLE PRECISION | DOUBLE | double | `getDouble()` |
| NUMERIC/DECIMAL | NUMERIC | BigDecimal | `getBigDecimal()` |
| CHAR(n) | CHAR | String | `getString()` |
| VARCHAR(n) | VARCHAR | String | `getString()` |
| TEXT | LONGVARCHAR | String | `getString()` |
| BYTEA | VARBINARY | byte[] | `getBytes()` |
| DATE | DATE | java.sql.Date | `getDate()` |
| TIME | TIME | java.sql.Time | `getTime()` |
| TIMESTAMP | TIMESTAMP | java.sql.Timestamp | `getTimestamp()` |
| TIMESTAMPTZ | TIMESTAMP_WITH_TIMEZONE | OffsetDateTime | `getObject()` |
| INTERVAL | OTHER | PGInterval | `getObject()` |
| UUID | OTHER | java.util.UUID | `getObject()` |
| JSON | OTHER | String | `getString()` |
| JSONB | OTHER | String | `getString()` |
| ARRAY | ARRAY | java.sql.Array | `getArray()` |
| INET | OTHER | InetAddress | `getObject()` |
| CIDR | OTHER | String | `getString()` |
| MACADDR | OTHER | String | `getString()` |

### 6.2 Java to ScratchBird Type Mapping

| Java Type | ScratchBird Type |
|-----------|------------------|
| boolean, Boolean | BOOLEAN |
| byte, short, Short | SMALLINT |
| int, Integer | INTEGER |
| long, Long | BIGINT |
| float, Float | REAL |
| double, Double | DOUBLE PRECISION |
| BigDecimal | NUMERIC |
| String | VARCHAR |
| byte[] | BYTEA |
| java.sql.Date | DATE |
| java.sql.Time | TIME |
| java.sql.Timestamp | TIMESTAMP |
| java.time.LocalDate | DATE |
| java.time.LocalTime | TIME |
| java.time.LocalDateTime | TIMESTAMP |
| java.time.OffsetDateTime | TIMESTAMPTZ |
| java.util.UUID | UUID |
| java.sql.Array | ARRAY |
| java.net.InetAddress | INET |

---

## 7. Transaction Support

### 7.1 Auto-Commit Mode

```java
// Disable auto-commit for explicit transactions
conn.setAutoCommit(false);

try {
    stmt.executeUpdate("INSERT INTO accounts ...");
    stmt.executeUpdate("UPDATE balances ...");
    conn.commit();
} catch (SQLException e) {
    conn.rollback();
    throw e;
}
```

### 7.2 Isolation Levels

```java
// Supported isolation levels
conn.setTransactionIsolation(Connection.TRANSACTION_READ_UNCOMMITTED);
conn.setTransactionIsolation(Connection.TRANSACTION_READ_COMMITTED);     // Default
conn.setTransactionIsolation(Connection.TRANSACTION_REPEATABLE_READ);
conn.setTransactionIsolation(Connection.TRANSACTION_SERIALIZABLE);

// ScratchBird extension: Snapshot isolation
conn.setTransactionIsolation(SBConnection.TRANSACTION_SNAPSHOT);
```

### 7.3 Savepoints

```java
conn.setAutoCommit(false);

Savepoint sp1 = conn.setSavepoint("checkpoint1");
stmt.executeUpdate("UPDATE ...");

Savepoint sp2 = conn.setSavepoint("checkpoint2");
stmt.executeUpdate("INSERT ...");

// Rollback to savepoint
conn.rollback(sp2);

// Release savepoint
conn.releaseSavepoint(sp1);

conn.commit();
```

---

## 8. Batch Operations

### 8.1 Statement Batching

```java
Statement stmt = conn.createStatement();
stmt.addBatch("INSERT INTO logs VALUES (1, 'msg1')");
stmt.addBatch("INSERT INTO logs VALUES (2, 'msg2')");
stmt.addBatch("INSERT INTO logs VALUES (3, 'msg3')");

int[] results = stmt.executeBatch();
// Or for large counts:
long[] results = stmt.executeLargeBatch();
```

### 8.2 PreparedStatement Batching

```java
PreparedStatement pstmt = conn.prepareStatement(
    "INSERT INTO users (id, name, email) VALUES (?, ?, ?)"
);

for (User user : users) {
    pstmt.setInt(1, user.getId());
    pstmt.setString(2, user.getName());
    pstmt.setString(3, user.getEmail());
    pstmt.addBatch();
}

int[] results = pstmt.executeBatch();
```

### 8.3 Batch Optimization

Enable `reWriteBatchedInserts` for multi-value INSERT:

```java
// URL parameter
jdbc:scratchbird://localhost/db?reWriteBatchedInserts=true

// Multiple INSERTs rewritten to:
// INSERT INTO users VALUES (1,'a','a@x'),(2,'b','b@x'),(3,'c','c@x')
```

---

## 9. Connection Pooling

### 9.1 HikariCP Configuration

```java
HikariConfig config = new HikariConfig();
config.setJdbcUrl("jdbc:scratchbird://localhost:3092/mydb");
config.setUsername("user");
config.setPassword("password");
config.setMaximumPoolSize(20);
config.setMinimumIdle(5);
config.setIdleTimeout(300000);
config.setConnectionTimeout(30000);
config.setMaxLifetime(1800000);

// ScratchBird-specific settings
config.addDataSourceProperty("prepareThreshold", "5");
config.addDataSourceProperty("binaryTransfer", "true");
config.addDataSourceProperty("reWriteBatchedInserts", "true");

HikariDataSource ds = new HikariDataSource(config);
```

### 9.2 Apache DBCP2 Configuration

```java
BasicDataSource ds = new BasicDataSource();
ds.setDriverClassName("com.scratchbird.jdbc.SBDriver");
ds.setUrl("jdbc:scratchbird://localhost:3092/mydb");
ds.setUsername("user");
ds.setPassword("password");
ds.setInitialSize(5);
ds.setMaxTotal(20);
ds.setMaxIdle(10);
ds.setMinIdle(5);
ds.setMaxWaitMillis(30000);
ds.setValidationQuery("SELECT 1");
ds.setTestOnBorrow(true);
```

### 9.3 Spring Boot Configuration

```yaml
# application.yml
spring:
  datasource:
    url: jdbc:scratchbird://localhost:3092/mydb
    username: user
    password: password
    driver-class-name: com.scratchbird.jdbc.SBDriver
    hikari:
      maximum-pool-size: 20
      minimum-idle: 5
      connection-timeout: 30000
      idle-timeout: 300000
      max-lifetime: 1800000
      connection-test-query: SELECT 1
```

---

## 10. ORM Integration

### 10.1 Hibernate Configuration

```xml
<!-- hibernate.cfg.xml -->
<hibernate-configuration>
    <session-factory>
        <property name="hibernate.dialect">
            com.scratchbird.hibernate.SBDialect
        </property>
        <property name="hibernate.connection.driver_class">
            com.scratchbird.jdbc.SBDriver
        </property>
        <property name="hibernate.connection.url">
            jdbc:scratchbird://localhost:3092/mydb
        </property>
    </session-factory>
</hibernate-configuration>
```

### 10.2 JPA/Spring Data JPA

```yaml
# application.yml
spring:
  jpa:
    database-platform: com.scratchbird.hibernate.SBDialect
    properties:
      hibernate:
        jdbc:
          batch_size: 50
          batch_versioned_data: true
        order_inserts: true
        order_updates: true
```

### 10.3 MyBatis Configuration

```xml
<!-- mybatis-config.xml -->
<configuration>
    <environments default="development">
        <environment id="development">
            <transactionManager type="JDBC"/>
            <dataSource type="POOLED">
                <property name="driver" value="com.scratchbird.jdbc.SBDriver"/>
                <property name="url" value="jdbc:scratchbird://localhost:3092/mydb"/>
                <property name="username" value="user"/>
                <property name="password" value="password"/>
            </dataSource>
        </environment>
    </environments>
</configuration>
```

---

## 11. JDBC Foreign Data Wrapper

### 11.1 jdbc_fdw for External Database Access

ScratchBird includes `jdbc_fdw` for connecting to any JDBC-accessible database:

```sql
-- Create foreign server using JDBC
CREATE SERVER oracle_server
    FOREIGN DATA WRAPPER jdbc_fdw
    OPTIONS (
        driver_class 'oracle.jdbc.OracleDriver',
        url 'jdbc:oracle:thin:@//oracle.example.com:1521/ORCL',
        jar_path '/opt/oracle/ojdbc8.jar'
    );

-- Create user mapping
CREATE USER MAPPING FOR CURRENT_USER
    SERVER oracle_server
    OPTIONS (
        username 'scott',
        password 'tiger'
    );

-- Import foreign schema
IMPORT FOREIGN SCHEMA SCOTT
    FROM SERVER oracle_server
    INTO oracle_schema;

-- Or create individual foreign table
CREATE FOREIGN TABLE oracle_employees (
    employee_id INTEGER,
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    salary NUMERIC(10,2)
)
SERVER oracle_server
OPTIONS (
    schema 'SCOTT',
    table 'EMP'
);

-- Query external data
SELECT * FROM oracle_schema.EMP WHERE deptno = 10;
```

### 11.2 Supported External Databases via JDBC

| Database | JDBC Driver | Notes |
|----------|-------------|-------|
| Oracle | ojdbc8.jar | Full support |
| SQL Server | mssql-jdbc.jar | Planned (post-gold) |
| DB2 | db2jcc4.jar | Full support |
| MySQL | mysql-connector-java.jar | Full support |
| PostgreSQL | postgresql.jar | Full support |
| SAP HANA | ngdbc.jar | Full support |
| Snowflake | snowflake-jdbc.jar | Full support |
| Teradata | terajdbc4.jar | Full support |
| SQLite | sqlite-jdbc.jar | Full support |
| H2 | h2.jar | Full support |
| MariaDB | mariadb-java-client.jar | Full support |

### 11.3 Query Pushdown

```sql
-- Pushdown optimization for Oracle
EXPLAIN VERBOSE
SELECT department_id, COUNT(*), AVG(salary)
FROM oracle_schema.employees
WHERE hire_date > '2020-01-01'
GROUP BY department_id
HAVING COUNT(*) > 5;

-- Shows: Remote SQL sent to Oracle
```

---

## 12. Advanced Features

### 12.1 LISTEN/NOTIFY

```java
SBConnection sbConn = conn.unwrap(SBConnection.class);

// Add listener
sbConn.addNotificationListener("my_channel", notification -> {
    System.out.println("Received: " + notification.getParameter());
});

// In another thread/connection
Statement stmt = conn.createStatement();
stmt.execute("NOTIFY my_channel, 'Hello World'");

// Check for notifications (polling mode)
PGNotification[] notifications = sbConn.getNotifications();
```

### 12.2 COPY Operations

```java
SBConnection sbConn = conn.unwrap(SBConnection.class);
CopyManager copyManager = sbConn.getCopyAPI();

// COPY TO (export)
FileWriter writer = new FileWriter("/tmp/export.csv");
copyManager.copyOut("COPY users TO STDOUT WITH CSV HEADER", writer);

// COPY FROM (import)
FileReader reader = new FileReader("/tmp/import.csv");
long rows = copyManager.copyIn("COPY users FROM STDIN WITH CSV HEADER", reader);
```

### 12.3 Large Objects (LOB)

```java
SBConnection sbConn = conn.unwrap(SBConnection.class);
LargeObjectManager lom = sbConn.getLargeObjectAPI();

conn.setAutoCommit(false);

// Create large object
long oid = lom.createLO(LargeObjectManager.READ | LargeObjectManager.WRITE);
LargeObject lo = lom.open(oid, LargeObjectManager.WRITE);
lo.write(data);
lo.close();

// Read large object
lo = lom.open(oid, LargeObjectManager.READ);
byte[] data = lo.read(lo.size());
lo.close();

conn.commit();
```

### 12.4 Array Support

```java
// Create array
Integer[] values = {1, 2, 3, 4, 5};
Array array = conn.createArrayOf("INTEGER", values);

PreparedStatement pstmt = conn.prepareStatement(
    "INSERT INTO data (tags) VALUES (?)"
);
pstmt.setArray(1, array);
pstmt.executeUpdate();

// Read array
ResultSet rs = stmt.executeQuery("SELECT tags FROM data");
rs.next();
Array arr = rs.getArray("tags");
Integer[] result = (Integer[]) arr.getArray();
```

---

## 13. Error Handling

### 13.1 SQLException Hierarchy

```
SQLException
├── SQLNonTransientException
│   ├── SQLDataException           (data errors)
│   ├── SQLIntegrityConstraintViolationException
│   ├── SQLInvalidAuthorizationSpecException
│   └── SQLSyntaxErrorException
├── SQLTransientException
│   ├── SQLTimeoutException
│   ├── SQLTransactionRollbackException
│   └── SQLTransientConnectionException
└── SQLRecoverableException
```

### 13.2 SQLSTATE Mapping

```java
try {
    stmt.executeUpdate("INSERT INTO users ...");
} catch (SQLException e) {
    String sqlState = e.getSQLState();
    int errorCode = e.getErrorCode();

    switch (sqlState) {
        case "23505": // unique_violation
            throw new DuplicateKeyException(e);
        case "23503": // foreign_key_violation
            throw new ReferenceConstraintException(e);
        case "40001": // serialization_failure
            // Retry transaction
            break;
        default:
            throw e;
    }
}
```

---

## 14. Logging and Debugging

### 14.1 Driver Logging

```java
// Via URL parameter
jdbc:scratchbird://localhost/db?loggerLevel=DEBUG&loggerFile=/tmp/jdbc.log

// Via system property
System.setProperty("com.scratchbird.jdbc.loggerLevel", "DEBUG");
System.setProperty("com.scratchbird.jdbc.loggerFile", "/tmp/jdbc.log");
```

### 14.2 SLF4J Integration

```xml
<!-- Add SLF4J binding -->
<dependency>
    <groupId>org.slf4j</groupId>
    <artifactId>slf4j-simple</artifactId>
    <version>1.7.36</version>
</dependency>
```

```properties
# simplelogger.properties
org.slf4j.simpleLogger.log.com.scratchbird.jdbc=debug
```

---

## 15. Configuration Reference

### 15.1 System Properties

| Property | Description |
|----------|-------------|
| `com.scratchbird.jdbc.loggerLevel` | OFF, DEBUG, TRACE |
| `com.scratchbird.jdbc.loggerFile` | Log file path |
| `com.scratchbird.jdbc.ssl.trustStore` | Trust store path |
| `com.scratchbird.jdbc.ssl.trustStorePassword` | Trust store password |
| `com.scratchbird.jdbc.ssl.keyStore` | Key store path |
| `com.scratchbird.jdbc.ssl.keyStorePassword` | Key store password |

### 15.2 DataSource Properties

```java
SBSimpleDataSource ds = new SBSimpleDataSource();
ds.setServerNames(new String[]{"localhost"});
ds.setPortNumbers(new int[]{3092});
ds.setDatabaseName("mydb");
ds.setUser("user");
ds.setPassword("password");
ds.setSsl(true);
ds.setSslMode("verify-full");
ds.setConnectTimeout(30);
ds.setSocketTimeout(0);
ds.setCurrentSchema("app");
ds.setApplicationName("MyApplication");
ds.setBinaryTransfer(true);
ds.setPrepareThreshold(5);
ds.setDefaultRowFetchSize(100);
ds.setReWriteBatchedInserts(true);
```

---

## 16. Application Examples

### 16.1 Basic JDBC

```java
import java.sql.*;

public class BasicExample {
    public static void main(String[] args) throws SQLException {
        String url = "jdbc:scratchbird://localhost:3092/mydb";

        try (Connection conn = DriverManager.getConnection(url, "user", "pass");
             Statement stmt = conn.createStatement();
             ResultSet rs = stmt.executeQuery("SELECT * FROM users")) {

            while (rs.next()) {
                System.out.printf("%d: %s (%s)%n",
                    rs.getInt("id"),
                    rs.getString("name"),
                    rs.getString("email"));
            }
        }
    }
}
```

### 16.2 Spring Boot Repository

```java
@Repository
public interface UserRepository extends JpaRepository<User, Long> {

    @Query("SELECT u FROM User u WHERE u.status = :status")
    List<User> findByStatus(@Param("status") String status);

    @Modifying
    @Query("UPDATE User u SET u.lastLogin = :time WHERE u.id = :id")
    void updateLastLogin(@Param("id") Long id, @Param("time") Instant time);
}
```

### 16.3 MyBatis Mapper

```java
@Mapper
public interface UserMapper {

    @Select("SELECT * FROM users WHERE id = #{id}")
    User findById(Long id);

    @Insert("INSERT INTO users (name, email) VALUES (#{name}, #{email})")
    @Options(useGeneratedKeys = true, keyProperty = "id")
    int insert(User user);

    @Update("UPDATE users SET name = #{name} WHERE id = #{id}")
    int update(User user);

    @Delete("DELETE FROM users WHERE id = #{id}")
    int delete(Long id);
}
```
