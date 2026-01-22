# JDBC Connectivity

Connect to ScratchBird from Java applications.

[Back to Connectivity Index](index.md) | [Back to Documentation Index](../index.md)

---

## Overview

ScratchBird ships a native JDBC driver (Type 4) that speaks the ScratchBird wire protocol on port 3092. PostgreSQL/MySQL JDBC drivers remain supported for compatibility testing and migrations.

---

## ScratchBird JDBC Driver (Native)

### Build

```bash
cd jdbc
gradle jar
```

Jar output: `jdbc/build/libs/scratchbird-jdbc.jar`

### Connection URL

```
jdbc:scratchbird://host[:port]/database
```

### SSL/TLS Options

Use standard JDBC properties:

```
sslmode=disable|allow|prefer|require|verify-ca|verify-full
sslrootcert=/path/to/ca.pem
sslcert=/path/to/client-keystore.p12
sslpassword=keystore_password
```

### Example

```java
String url = "jdbc:scratchbird://localhost:3092/mydb";
Connection conn = DriverManager.getConnection(url, "admin", "secret");
```

### Example with Properties

```java
Properties props = new Properties();
props.setProperty("user", "admin");
props.setProperty("password", "secret");
props.setProperty("sslmode", "require");
props.setProperty("sslrootcert", "/etc/ssl/certs/ca.pem");

Connection conn = DriverManager.getConnection(
    "jdbc:scratchbird://localhost:3092/mydb", props);
```

---

## Driver Selection

| Protocol | Driver | Best For |
|----------|--------|----------|
| ScratchBird (native) | ScratchBird JDBC | Direct native protocol |
| PostgreSQL | PostgreSQL JDBC | Compatibility testing |
| MySQL | MySQL Connector/J | MySQL-specific apps |
| Firebird | Jaybird | Firebird-specific apps |

---

## PostgreSQL JDBC Driver

### Maven Dependency

```xml
<dependency>
    <groupId>org.postgresql</groupId>
    <artifactId>postgresql</artifactId>
    <version>42.7.0</version>
</dependency>
```

### Gradle

```groovy
implementation 'org.postgresql:postgresql:42.7.0'
```

### Basic Connection

```java
import java.sql.*;

public class Example {
    public static void main(String[] args) throws SQLException {
        String url = "jdbc:postgresql://localhost:5432/mydb";
        String user = "admin";
        String password = "secret";

        try (Connection conn = DriverManager.getConnection(url, user, password)) {
            System.out.println("Connected!");

            try (Statement stmt = conn.createStatement()) {
                ResultSet rs = stmt.executeQuery("SELECT * FROM users");
                while (rs.next()) {
                    System.out.println(rs.getString("name"));
                }
            }
        }
    }
}
```

### Connection with Properties

```java
Properties props = new Properties();
props.setProperty("user", "admin");
props.setProperty("password", "secret");
props.setProperty("ssl", "true");
props.setProperty("sslfactory", "org.postgresql.ssl.NonValidatingFactory");

Connection conn = DriverManager.getConnection(
    "jdbc:postgresql://localhost:5432/mydb", props);
```

---

## Prepared Statements

```java
String sql = "SELECT * FROM users WHERE id = ? AND active = ?";

try (PreparedStatement pstmt = conn.prepareStatement(sql)) {
    pstmt.setInt(1, 123);
    pstmt.setBoolean(2, true);

    try (ResultSet rs = pstmt.executeQuery()) {
        while (rs.next()) {
            System.out.println(rs.getString("name"));
        }
    }
}
```

---

## Batch Operations

```java
String sql = "INSERT INTO users (name, email) VALUES (?, ?)";

try (PreparedStatement pstmt = conn.prepareStatement(sql)) {
    conn.setAutoCommit(false);

    for (User user : users) {
        pstmt.setString(1, user.getName());
        pstmt.setString(2, user.getEmail());
        pstmt.addBatch();
    }

    int[] results = pstmt.executeBatch();
    conn.commit();
}
```

---

## Connection Pooling

### HikariCP

```xml
<dependency>
    <groupId>com.zaxxer</groupId>
    <artifactId>HikariCP</artifactId>
    <version>5.1.0</version>
</dependency>
```

```java
import com.zaxxer.hikari.HikariConfig;
import com.zaxxer.hikari.HikariDataSource;

HikariConfig config = new HikariConfig();
config.setJdbcUrl("jdbc:postgresql://localhost:5432/mydb");
config.setUsername("admin");
config.setPassword("secret");
config.setMaximumPoolSize(10);
config.setMinimumIdle(5);

HikariDataSource ds = new HikariDataSource(config);

try (Connection conn = ds.getConnection()) {
    // Use connection
}
```

### Apache DBCP2

```xml
<dependency>
    <groupId>org.apache.commons</groupId>
    <artifactId>commons-dbcp2</artifactId>
    <version>2.11.0</version>
</dependency>
```

```java
import org.apache.commons.dbcp2.BasicDataSource;

BasicDataSource ds = new BasicDataSource();
ds.setUrl("jdbc:postgresql://localhost:5432/mydb");
ds.setUsername("admin");
ds.setPassword("secret");
ds.setMinIdle(5);
ds.setMaxIdle(10);
ds.setMaxTotal(25);
```

---

## Transactions

```java
try {
    conn.setAutoCommit(false);

    // Multiple operations
    stmt.executeUpdate("UPDATE accounts SET balance = balance - 100 WHERE id = 1");
    stmt.executeUpdate("UPDATE accounts SET balance = balance + 100 WHERE id = 2");

    conn.commit();
} catch (SQLException e) {
    conn.rollback();
    throw e;
} finally {
    conn.setAutoCommit(true);
}
```

### Savepoints

```java
conn.setAutoCommit(false);

Savepoint sp1 = conn.setSavepoint("point1");

try {
    stmt.executeUpdate("...");
    // If this fails
    stmt.executeUpdate("...");
} catch (SQLException e) {
    conn.rollback(sp1);
}

conn.commit();
```

---

## Spring Framework

### Spring Boot application.properties

```properties
spring.datasource.url=jdbc:postgresql://localhost:5432/mydb
spring.datasource.username=admin
spring.datasource.password=secret
spring.datasource.driver-class-name=org.postgresql.Driver

# HikariCP settings
spring.datasource.hikari.maximum-pool-size=10
spring.datasource.hikari.minimum-idle=5
```

### Spring Data JPA

```java
@Entity
@Table(name = "users")
public class User {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    private String name;
    private String email;
    // getters/setters
}

@Repository
public interface UserRepository extends JpaRepository<User, Long> {
    List<User> findByNameContaining(String name);
}
```

### JdbcTemplate

```java
@Autowired
private JdbcTemplate jdbcTemplate;

public List<User> findAll() {
    return jdbcTemplate.query(
        "SELECT * FROM users",
        (rs, rowNum) -> new User(
            rs.getLong("id"),
            rs.getString("name"),
            rs.getString("email")
        )
    );
}

public void insert(User user) {
    jdbcTemplate.update(
        "INSERT INTO users (name, email) VALUES (?, ?)",
        user.getName(), user.getEmail()
    );
}
```

---

## MySQL JDBC Driver

If your application uses MySQL syntax:

### Maven

```xml
<dependency>
    <groupId>com.mysql</groupId>
    <artifactId>mysql-connector-j</artifactId>
    <version>8.2.0</version>
</dependency>
```

### Connection

```java
String url = "jdbc:mysql://127.0.0.1:3306/mydb";
Connection conn = DriverManager.getConnection(url, "admin", "secret");
```

---

## Firebird JDBC (Jaybird)

### Maven

```xml
<dependency>
    <groupId>org.firebirdsql.jdbc</groupId>
    <artifactId>jaybird</artifactId>
    <version>5.0.3.java11</version>
</dependency>
```

### Connection

```java
String url = "jdbc:firebird://localhost:3050/path/to/database.sbdb";
Connection conn = DriverManager.getConnection(url, "admin", "secret");
```

---

## SSL/TLS Configuration

### PostgreSQL JDBC

```java
String url = "jdbc:postgresql://localhost:5432/mydb?ssl=true&sslmode=verify-full";

Properties props = new Properties();
props.setProperty("user", "admin");
props.setProperty("password", "secret");
props.setProperty("ssl", "true");
props.setProperty("sslmode", "verify-full");
props.setProperty("sslrootcert", "/path/to/ca.crt");

Connection conn = DriverManager.getConnection(url, props);
```

### SSL Modes

| Mode | Description |
|------|-------------|
| `disable` | No SSL |
| `allow` | SSL if server supports |
| `prefer` | Prefer SSL |
| `require` | Require SSL |
| `verify-ca` | Verify CA certificate |
| `verify-full` | Verify CA and hostname |

---

## Performance Tips

### Prepared Statement Caching

```java
// PostgreSQL JDBC
props.setProperty("prepareThreshold", "5");
props.setProperty("preparedStatementCacheQueries", "256");
props.setProperty("preparedStatementCacheSizeMiB", "5");
```

### Fetch Size

```java
// For large result sets
stmt.setFetchSize(1000);
ResultSet rs = stmt.executeQuery("SELECT * FROM large_table");
```

### Batch Size

```java
// Batch inserts
props.setProperty("reWriteBatchedInserts", "true");
```

---

## Error Handling

```java
try {
    // Database operations
} catch (SQLException e) {
    System.err.println("SQL State: " + e.getSQLState());
    System.err.println("Error Code: " + e.getErrorCode());
    System.err.println("Message: " + e.getMessage());

    // Get all chained exceptions
    SQLException next = e.getNextException();
    while (next != null) {
        System.err.println("Next: " + next.getMessage());
        next = next.getNextException();
    }
}
```

---

## Troubleshooting

### "No suitable driver"

```java
// Ensure driver is loaded
Class.forName("org.postgresql.Driver");
```

### Connection Timeout

```java
// Set timeout in URL
String url = "jdbc:postgresql://localhost:5432/mydb?connectTimeout=10&socketTimeout=30";
```

### "Too many connections"

Use connection pooling and ensure connections are closed:

```java
// Always use try-with-resources
try (Connection conn = ds.getConnection();
     PreparedStatement ps = conn.prepareStatement(sql);
     ResultSet rs = ps.executeQuery()) {
    // Use resources
}  // Automatically closed
```

---

## See Also

- [PostgreSQL Clients](postgresql-clients.md)
- [MySQL Clients](mysql-clients.md)
- [ODBC](odbc.md)
