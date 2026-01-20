[Back to Drivers](Driver-Comparison.md) | [Back to Home](../Home.md)

# Java JDBC Driver Guide

**Status:** Alpha documentation
**Last Updated:** 2026-01-20

---

## Overview

Java applications can connect to ScratchBird through JDBC using several drivers:

| Protocol | Port | Driver | Best For |
|----------|------|--------|----------|
| PostgreSQL | 5432 | PostgreSQL JDBC | Most Java apps |
| MySQL | 3306 | MySQL Connector/J | MySQL migrations |
| Firebird | 3050 | Jaybird | Firebird migrations |
| Native | 3092 | (future) ScratchBird JDBC | Full feature access |

**Recommendation:** Use **PostgreSQL JDBC** driver via port 5432 for the best ecosystem compatibility.

---

## Quick Start

### Maven Dependencies

```xml
<!-- PostgreSQL JDBC (Recommended) -->
<dependency>
    <groupId>org.postgresql</groupId>
    <artifactId>postgresql</artifactId>
    <version>42.7.1</version>
</dependency>

<!-- MySQL Connector/J -->
<dependency>
    <groupId>com.mysql</groupId>
    <artifactId>mysql-connector-j</artifactId>
    <version>8.2.0</version>
</dependency>

<!-- Jaybird (Firebird) -->
<dependency>
    <groupId>org.firebirdsql.jdbc</groupId>
    <artifactId>jaybird</artifactId>
    <version>5.0.3.java11</version>
</dependency>

<!-- HikariCP Connection Pool -->
<dependency>
    <groupId>com.zaxxer</groupId>
    <artifactId>HikariCP</artifactId>
    <version>5.1.0</version>
</dependency>
```

### Gradle Dependencies

```groovy
// PostgreSQL JDBC
implementation 'org.postgresql:postgresql:42.7.1'

// MySQL Connector/J
implementation 'com.mysql:mysql-connector-j:8.2.0'

// Jaybird (Firebird)
implementation 'org.firebirdsql.jdbc:jaybird:5.0.3.java11'

// HikariCP Connection Pool
implementation 'com.zaxxer:HikariCP:5.1.0'
```

### Basic Connection

```java
import java.sql.*;

public class QuickStart {
    public static void main(String[] args) {
        String url = "jdbc:postgresql://localhost:5432/scratchbird";
        String user = "app_user";
        String password = "secret";

        try (Connection conn = DriverManager.getConnection(url, user, password)) {
            Statement stmt = conn.createStatement();
            ResultSet rs = stmt.executeQuery("SELECT version()");

            if (rs.next()) {
                System.out.println("Version: " + rs.getString(1));
            }
        } catch (SQLException e) {
            e.printStackTrace();
        }
    }
}
```

---

## Connection Methods

### PostgreSQL JDBC

```java
import java.sql.*;
import java.util.Properties;

// Basic connection
Connection conn = DriverManager.getConnection(
    "jdbc:postgresql://localhost:5432/scratchbird",
    "app_user",
    "secret"
);

// With Properties
Properties props = new Properties();
props.setProperty("user", "app_user");
props.setProperty("password", "secret");
props.setProperty("ssl", "true");
props.setProperty("sslmode", "require");
props.setProperty("connectTimeout", "10");
props.setProperty("socketTimeout", "30");

Connection conn2 = DriverManager.getConnection(
    "jdbc:postgresql://localhost:5432/scratchbird",
    props
);

// Connection string with parameters
String url = "jdbc:postgresql://localhost:5432/scratchbird" +
    "?user=app_user" +
    "&password=secret" +
    "&ssl=true" +
    "&sslmode=require";
Connection conn3 = DriverManager.getConnection(url);
```

### MySQL Connector/J

```java
import java.sql.*;

// Basic connection
Connection conn = DriverManager.getConnection(
    "jdbc:mysql://localhost:3306/scratchbird",
    "app_user",
    "secret"
);

// With options
String url = "jdbc:mysql://localhost:3306/scratchbird" +
    "?useSSL=true" +
    "&serverTimezone=UTC" +
    "&allowPublicKeyRetrieval=true";
Connection conn2 = DriverManager.getConnection(url, "app_user", "secret");
```

### Jaybird (Firebird)

```java
import java.sql.*;

// Basic connection
Connection conn = DriverManager.getConnection(
    "jdbc:firebird://localhost:3050/scratchbird",
    "SYSDBA",
    "masterkey"
);

// With encoding
String url = "jdbc:firebird://localhost:3050/scratchbird" +
    "?encoding=UTF8";
Connection conn2 = DriverManager.getConnection(url, "SYSDBA", "masterkey");
```

---

## CRUD Operations

### Create (INSERT)

```java
import java.sql.*;

public class InsertExample {
    public static void main(String[] args) throws SQLException {
        String url = "jdbc:postgresql://localhost:5432/scratchbird";

        try (Connection conn = DriverManager.getConnection(url, "app_user", "secret")) {

            // Simple INSERT with PreparedStatement
            String sql = "INSERT INTO users (username, email) VALUES (?, ?)";
            try (PreparedStatement pstmt = conn.prepareStatement(sql)) {
                pstmt.setString(1, "john_doe");
                pstmt.setString(2, "john@example.com");
                int rowsInserted = pstmt.executeUpdate();
                System.out.println("Rows inserted: " + rowsInserted);
            }

            // INSERT with RETURNING (get generated ID)
            String sqlReturning = "INSERT INTO users (username, email) VALUES (?, ?) RETURNING id";
            try (PreparedStatement pstmt = conn.prepareStatement(sqlReturning)) {
                pstmt.setString(1, "jane_doe");
                pstmt.setString(2, "jane@example.com");
                ResultSet rs = pstmt.executeQuery();
                if (rs.next()) {
                    System.out.println("Inserted user ID: " + rs.getLong("id"));
                }
            }

            // Using RETURN_GENERATED_KEYS
            String sqlGenKeys = "INSERT INTO users (username, email) VALUES (?, ?)";
            try (PreparedStatement pstmt = conn.prepareStatement(sqlGenKeys,
                    Statement.RETURN_GENERATED_KEYS)) {
                pstmt.setString(1, "bob_smith");
                pstmt.setString(2, "bob@example.com");
                pstmt.executeUpdate();

                ResultSet rs = pstmt.getGeneratedKeys();
                if (rs.next()) {
                    System.out.println("Generated ID: " + rs.getLong(1));
                }
            }

            // Batch INSERT
            String batchSql = "INSERT INTO users (username, email) VALUES (?, ?)";
            try (PreparedStatement pstmt = conn.prepareStatement(batchSql)) {
                String[][] users = {
                    {"user1", "user1@example.com"},
                    {"user2", "user2@example.com"},
                    {"user3", "user3@example.com"}
                };

                for (String[] user : users) {
                    pstmt.setString(1, user[0]);
                    pstmt.setString(2, user[1]);
                    pstmt.addBatch();
                }

                int[] results = pstmt.executeBatch();
                System.out.println("Batch inserted: " + results.length + " rows");
            }
        }
    }
}
```

### Read (SELECT)

```java
import java.sql.*;
import java.util.ArrayList;
import java.util.List;

public class SelectExample {
    public static void main(String[] args) throws SQLException {
        String url = "jdbc:postgresql://localhost:5432/scratchbird";

        try (Connection conn = DriverManager.getConnection(url, "app_user", "secret")) {

            // Simple SELECT
            String sql = "SELECT id, username, email, created_at FROM users";
            try (Statement stmt = conn.createStatement();
                 ResultSet rs = stmt.executeQuery(sql)) {

                while (rs.next()) {
                    long id = rs.getLong("id");
                    String username = rs.getString("username");
                    String email = rs.getString("email");
                    Timestamp createdAt = rs.getTimestamp("created_at");

                    System.out.printf("ID: %d, User: %s, Email: %s, Created: %s%n",
                        id, username, email, createdAt);
                }
            }

            // SELECT with parameters
            String sqlParam = "SELECT * FROM users WHERE status = ? AND role = ?";
            try (PreparedStatement pstmt = conn.prepareStatement(sqlParam)) {
                pstmt.setString(1, "active");
                pstmt.setString(2, "admin");

                ResultSet rs = pstmt.executeQuery();
                while (rs.next()) {
                    System.out.println(rs.getString("username"));
                }
            }

            // SELECT single row
            String sqlSingle = "SELECT * FROM users WHERE id = ?";
            try (PreparedStatement pstmt = conn.prepareStatement(sqlSingle)) {
                pstmt.setLong(1, 1L);
                ResultSet rs = pstmt.executeQuery();

                if (rs.next()) {
                    System.out.println("Found: " + rs.getString("username"));
                } else {
                    System.out.println("User not found");
                }
            }

            // SELECT with LIMIT and OFFSET
            String sqlPaged = "SELECT * FROM users ORDER BY id LIMIT ? OFFSET ?";
            try (PreparedStatement pstmt = conn.prepareStatement(sqlPaged)) {
                pstmt.setInt(1, 10);  // limit
                pstmt.setInt(2, 0);   // offset
                ResultSet rs = pstmt.executeQuery();
                // Process results...
            }
        }
    }
}
```

### Update

```java
import java.sql.*;

public class UpdateExample {
    public static void main(String[] args) throws SQLException {
        String url = "jdbc:postgresql://localhost:5432/scratchbird";

        try (Connection conn = DriverManager.getConnection(url, "app_user", "secret")) {

            // Simple UPDATE
            String sql = "UPDATE users SET email = ? WHERE id = ?";
            try (PreparedStatement pstmt = conn.prepareStatement(sql)) {
                pstmt.setString(1, "newemail@example.com");
                pstmt.setLong(2, 1L);
                int rowsUpdated = pstmt.executeUpdate();
                System.out.println("Rows updated: " + rowsUpdated);
            }

            // UPDATE with RETURNING
            String sqlReturning = """
                UPDATE users
                SET last_login = NOW(), login_count = login_count + 1
                WHERE id = ?
                RETURNING id, last_login, login_count
                """;
            try (PreparedStatement pstmt = conn.prepareStatement(sqlReturning)) {
                pstmt.setLong(1, 1L);
                ResultSet rs = pstmt.executeQuery();
                if (rs.next()) {
                    System.out.printf("Updated: ID=%d, LastLogin=%s, Count=%d%n",
                        rs.getLong("id"),
                        rs.getTimestamp("last_login"),
                        rs.getInt("login_count"));
                }
            }

            // Conditional UPDATE
            String sqlConditional = """
                UPDATE products
                SET price = price * 1.10
                WHERE category = ? AND updated_at < ?
                """;
            try (PreparedStatement pstmt = conn.prepareStatement(sqlConditional)) {
                pstmt.setString(1, "electronics");
                pstmt.setDate(2, java.sql.Date.valueOf("2024-01-01"));
                int rows = pstmt.executeUpdate();
                System.out.println("Products updated: " + rows);
            }
        }
    }
}
```

### Delete

```java
import java.sql.*;

public class DeleteExample {
    public static void main(String[] args) throws SQLException {
        String url = "jdbc:postgresql://localhost:5432/scratchbird";

        try (Connection conn = DriverManager.getConnection(url, "app_user", "secret")) {

            // Simple DELETE
            String sql = "DELETE FROM sessions WHERE user_id = ?";
            try (PreparedStatement pstmt = conn.prepareStatement(sql)) {
                pstmt.setLong(1, 1L);
                int rowsDeleted = pstmt.executeUpdate();
                System.out.println("Sessions deleted: " + rowsDeleted);
            }

            // DELETE with RETURNING
            String sqlReturning = """
                DELETE FROM audit_logs
                WHERE created_at < NOW() - INTERVAL '1 year'
                RETURNING id, action
                """;
            try (Statement stmt = conn.createStatement();
                 ResultSet rs = stmt.executeQuery(sqlReturning)) {

                int count = 0;
                while (rs.next()) {
                    count++;
                }
                System.out.println("Deleted " + count + " old audit logs");
            }

            // DELETE with subquery
            String sqlSubquery = """
                DELETE FROM order_items
                WHERE order_id IN (
                    SELECT id FROM orders WHERE status = 'cancelled'
                )
                """;
            try (Statement stmt = conn.createStatement()) {
                int rows = stmt.executeUpdate(sqlSubquery);
                System.out.println("Order items deleted: " + rows);
            }
        }
    }
}
```

---

## Connection Pooling

### HikariCP (Recommended)

```java
import com.zaxxer.hikari.HikariConfig;
import com.zaxxer.hikari.HikariDataSource;
import java.sql.*;

public class HikariExample {
    private static HikariDataSource dataSource;

    static {
        HikariConfig config = new HikariConfig();
        config.setJdbcUrl("jdbc:postgresql://localhost:5432/scratchbird");
        config.setUsername("app_user");
        config.setPassword("secret");

        // Pool sizing
        config.setMinimumIdle(5);
        config.setMaximumPoolSize(20);

        // Connection settings
        config.setConnectionTimeout(30000);     // 30 seconds
        config.setIdleTimeout(600000);          // 10 minutes
        config.setMaxLifetime(1800000);         // 30 minutes
        config.setValidationTimeout(5000);      // 5 seconds

        // Performance
        config.addDataSourceProperty("cachePrepStmts", "true");
        config.addDataSourceProperty("prepStmtCacheSize", "250");
        config.addDataSourceProperty("prepStmtCacheSqlLimit", "2048");

        dataSource = new HikariDataSource(config);
    }

    public static Connection getConnection() throws SQLException {
        return dataSource.getConnection();
    }

    public static void close() {
        if (dataSource != null) {
            dataSource.close();
        }
    }

    public static void main(String[] args) {
        try (Connection conn = getConnection()) {
            Statement stmt = conn.createStatement();
            ResultSet rs = stmt.executeQuery("SELECT COUNT(*) FROM users");
            if (rs.next()) {
                System.out.println("User count: " + rs.getInt(1));
            }
        } catch (SQLException e) {
            e.printStackTrace();
        } finally {
            close();
        }
    }
}
```

### Apache DBCP2

```java
import org.apache.commons.dbcp2.BasicDataSource;
import java.sql.*;

public class DBCP2Example {
    private static BasicDataSource dataSource;

    static {
        dataSource = new BasicDataSource();
        dataSource.setUrl("jdbc:postgresql://localhost:5432/scratchbird");
        dataSource.setUsername("app_user");
        dataSource.setPassword("secret");

        // Pool configuration
        dataSource.setInitialSize(5);
        dataSource.setMinIdle(5);
        dataSource.setMaxIdle(10);
        dataSource.setMaxTotal(20);

        // Validation
        dataSource.setValidationQuery("SELECT 1");
        dataSource.setTestOnBorrow(true);
    }

    public static Connection getConnection() throws SQLException {
        return dataSource.getConnection();
    }
}
```

---

## Transactions

### Manual Transaction Control

```java
import java.sql.*;

public class TransactionExample {
    public static void main(String[] args) {
        String url = "jdbc:postgresql://localhost:5432/scratchbird";

        try (Connection conn = DriverManager.getConnection(url, "app_user", "secret")) {
            // Disable auto-commit
            conn.setAutoCommit(false);

            try {
                // Transfer funds
                PreparedStatement debit = conn.prepareStatement(
                    "UPDATE accounts SET balance = balance - ? WHERE id = ?"
                );
                debit.setBigDecimal(1, new java.math.BigDecimal("100.00"));
                debit.setLong(2, 1L);
                debit.executeUpdate();

                PreparedStatement credit = conn.prepareStatement(
                    "UPDATE accounts SET balance = balance + ? WHERE id = ?"
                );
                credit.setBigDecimal(1, new java.math.BigDecimal("100.00"));
                credit.setLong(2, 2L);
                credit.executeUpdate();

                // Record transfer
                PreparedStatement record = conn.prepareStatement(
                    "INSERT INTO transfers (from_id, to_id, amount) VALUES (?, ?, ?)"
                );
                record.setLong(1, 1L);
                record.setLong(2, 2L);
                record.setBigDecimal(3, new java.math.BigDecimal("100.00"));
                record.executeUpdate();

                // Commit transaction
                conn.commit();
                System.out.println("Transfer successful");

            } catch (SQLException e) {
                // Rollback on error
                conn.rollback();
                System.err.println("Transfer failed: " + e.getMessage());
                throw e;
            }
        } catch (SQLException e) {
            e.printStackTrace();
        }
    }
}
```

### Savepoints

```java
import java.sql.*;

public class SavepointExample {
    public static void main(String[] args) throws SQLException {
        String url = "jdbc:postgresql://localhost:5432/scratchbird";

        try (Connection conn = DriverManager.getConnection(url, "app_user", "secret")) {
            conn.setAutoCommit(false);

            try {
                // Create order
                PreparedStatement orderStmt = conn.prepareStatement(
                    "INSERT INTO orders (customer_id) VALUES (?)"
                );
                orderStmt.setLong(1, 1L);
                orderStmt.executeUpdate();

                // Create savepoint before items
                Savepoint beforeItems = conn.setSavepoint("before_items");

                try {
                    // Try to add items (may fail)
                    PreparedStatement itemStmt = conn.prepareStatement(
                        "INSERT INTO order_items (order_id, product_id) VALUES (?, ?)"
                    );
                    itemStmt.setLong(1, 1L);
                    itemStmt.setLong(2, 999L);  // May not exist
                    itemStmt.executeUpdate();

                } catch (SQLException e) {
                    // Rollback just the items, keep the order
                    conn.rollback(beforeItems);
                    System.out.println("Item insertion failed, order preserved");
                }

                conn.commit();

            } catch (SQLException e) {
                conn.rollback();
                throw e;
            }
        }
    }
}
```

### Transaction Isolation Levels

```java
import java.sql.*;

public class IsolationExample {
    public static void main(String[] args) throws SQLException {
        String url = "jdbc:postgresql://localhost:5432/scratchbird";

        try (Connection conn = DriverManager.getConnection(url, "app_user", "secret")) {
            // Set isolation level
            conn.setTransactionIsolation(Connection.TRANSACTION_SERIALIZABLE);
            // Other options:
            // Connection.TRANSACTION_READ_UNCOMMITTED
            // Connection.TRANSACTION_READ_COMMITTED
            // Connection.TRANSACTION_REPEATABLE_READ
            // Connection.TRANSACTION_SERIALIZABLE

            conn.setAutoCommit(false);

            try {
                // Perform operations...
                conn.commit();
            } catch (SQLException e) {
                conn.rollback();
                throw e;
            }
        }
    }
}
```

---

## Prepared Statements

```java
import java.sql.*;

public class PreparedStatementExample {
    public static void main(String[] args) throws SQLException {
        String url = "jdbc:postgresql://localhost:5432/scratchbird";

        try (Connection conn = DriverManager.getConnection(url, "app_user", "secret")) {

            // PreparedStatement with various types
            String sql = """
                INSERT INTO products (name, price, quantity, active, created_at)
                VALUES (?, ?, ?, ?, ?)
                """;

            try (PreparedStatement pstmt = conn.prepareStatement(sql)) {
                pstmt.setString(1, "Widget");
                pstmt.setBigDecimal(2, new java.math.BigDecimal("19.99"));
                pstmt.setInt(3, 100);
                pstmt.setBoolean(4, true);
                pstmt.setTimestamp(5, new Timestamp(System.currentTimeMillis()));
                pstmt.executeUpdate();
            }

            // Handling NULL values
            String sqlNull = "UPDATE users SET phone = ? WHERE id = ?";
            try (PreparedStatement pstmt = conn.prepareStatement(sqlNull)) {
                pstmt.setNull(1, Types.VARCHAR);  // Set NULL
                pstmt.setLong(2, 1L);
                pstmt.executeUpdate();
            }

            // Reusing PreparedStatement
            String sqlReuse = "SELECT * FROM users WHERE id = ?";
            try (PreparedStatement pstmt = conn.prepareStatement(sqlReuse)) {
                for (long id = 1; id <= 10; id++) {
                    pstmt.setLong(1, id);
                    ResultSet rs = pstmt.executeQuery();
                    if (rs.next()) {
                        System.out.println(rs.getString("username"));
                    }
                    rs.close();
                }
            }
        }
    }
}
```

---

## Error Handling

```java
import java.sql.*;

public class ErrorHandlingExample {
    public static void main(String[] args) {
        String url = "jdbc:postgresql://localhost:5432/scratchbird";

        try (Connection conn = DriverManager.getConnection(url, "app_user", "secret")) {

            PreparedStatement pstmt = conn.prepareStatement(
                "INSERT INTO users (username, email) VALUES (?, ?)"
            );
            pstmt.setString(1, "duplicate_user");
            pstmt.setString(2, "dup@example.com");
            pstmt.executeUpdate();

        } catch (SQLException e) {
            // Get SQL state for specific error handling
            String sqlState = e.getSQLState();

            switch (sqlState) {
                case "23505":  // unique_violation
                    System.err.println("Duplicate entry: " + e.getMessage());
                    break;
                case "23503":  // foreign_key_violation
                    System.err.println("Foreign key violation: " + e.getMessage());
                    break;
                case "23502":  // not_null_violation
                    System.err.println("NULL value not allowed: " + e.getMessage());
                    break;
                case "23514":  // check_violation
                    System.err.println("Check constraint failed: " + e.getMessage());
                    break;
                case "42P01":  // undefined_table
                    System.err.println("Table does not exist: " + e.getMessage());
                    break;
                case "08001":  // connection error
                case "08006":  // connection failure
                    System.err.println("Connection error: " + e.getMessage());
                    break;
                default:
                    System.err.println("SQL Error [" + sqlState + "]: " + e.getMessage());
            }

            // Print full error chain
            SQLException next = e.getNextException();
            while (next != null) {
                System.err.println("Caused by: " + next.getMessage());
                next = next.getNextException();
            }
        }
    }
}
```

---

## DAO Pattern

```java
import java.sql.*;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

// User entity
public class User {
    private Long id;
    private String username;
    private String email;
    private Timestamp createdAt;

    // Getters and setters...
    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }
    public String getUsername() { return username; }
    public void setUsername(String username) { this.username = username; }
    public String getEmail() { return email; }
    public void setEmail(String email) { this.email = email; }
    public Timestamp getCreatedAt() { return createdAt; }
    public void setCreatedAt(Timestamp createdAt) { this.createdAt = createdAt; }
}

// User DAO
public class UserDao {
    private final DataSource dataSource;

    public UserDao(DataSource dataSource) {
        this.dataSource = dataSource;
    }

    public Optional<User> findById(Long id) throws SQLException {
        String sql = "SELECT * FROM users WHERE id = ?";
        try (Connection conn = dataSource.getConnection();
             PreparedStatement pstmt = conn.prepareStatement(sql)) {

            pstmt.setLong(1, id);
            ResultSet rs = pstmt.executeQuery();

            if (rs.next()) {
                return Optional.of(mapRow(rs));
            }
            return Optional.empty();
        }
    }

    public List<User> findAll() throws SQLException {
        String sql = "SELECT * FROM users ORDER BY id";
        List<User> users = new ArrayList<>();

        try (Connection conn = dataSource.getConnection();
             Statement stmt = conn.createStatement();
             ResultSet rs = stmt.executeQuery(sql)) {

            while (rs.next()) {
                users.add(mapRow(rs));
            }
        }
        return users;
    }

    public User create(User user) throws SQLException {
        String sql = "INSERT INTO users (username, email) VALUES (?, ?) RETURNING *";
        try (Connection conn = dataSource.getConnection();
             PreparedStatement pstmt = conn.prepareStatement(sql)) {

            pstmt.setString(1, user.getUsername());
            pstmt.setString(2, user.getEmail());
            ResultSet rs = pstmt.executeQuery();

            if (rs.next()) {
                return mapRow(rs);
            }
            throw new SQLException("Failed to create user");
        }
    }

    public boolean update(User user) throws SQLException {
        String sql = "UPDATE users SET username = ?, email = ? WHERE id = ?";
        try (Connection conn = dataSource.getConnection();
             PreparedStatement pstmt = conn.prepareStatement(sql)) {

            pstmt.setString(1, user.getUsername());
            pstmt.setString(2, user.getEmail());
            pstmt.setLong(3, user.getId());
            return pstmt.executeUpdate() > 0;
        }
    }

    public boolean delete(Long id) throws SQLException {
        String sql = "DELETE FROM users WHERE id = ?";
        try (Connection conn = dataSource.getConnection();
             PreparedStatement pstmt = conn.prepareStatement(sql)) {

            pstmt.setLong(1, id);
            return pstmt.executeUpdate() > 0;
        }
    }

    private User mapRow(ResultSet rs) throws SQLException {
        User user = new User();
        user.setId(rs.getLong("id"));
        user.setUsername(rs.getString("username"));
        user.setEmail(rs.getString("email"));
        user.setCreatedAt(rs.getTimestamp("created_at"));
        return user;
    }
}
```

---

## Spring Framework Integration

### Spring JDBC

```java
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.jdbc.core.RowMapper;
import javax.sql.DataSource;
import java.util.List;

@Repository
public class UserRepository {
    private final JdbcTemplate jdbcTemplate;

    public UserRepository(DataSource dataSource) {
        this.jdbcTemplate = new JdbcTemplate(dataSource);
    }

    private final RowMapper<User> rowMapper = (rs, rowNum) -> {
        User user = new User();
        user.setId(rs.getLong("id"));
        user.setUsername(rs.getString("username"));
        user.setEmail(rs.getString("email"));
        user.setCreatedAt(rs.getTimestamp("created_at"));
        return user;
    };

    public List<User> findAll() {
        return jdbcTemplate.query("SELECT * FROM users", rowMapper);
    }

    public User findById(Long id) {
        return jdbcTemplate.queryForObject(
            "SELECT * FROM users WHERE id = ?",
            rowMapper,
            id
        );
    }

    public int create(User user) {
        return jdbcTemplate.update(
            "INSERT INTO users (username, email) VALUES (?, ?)",
            user.getUsername(),
            user.getEmail()
        );
    }

    public int update(User user) {
        return jdbcTemplate.update(
            "UPDATE users SET username = ?, email = ? WHERE id = ?",
            user.getUsername(),
            user.getEmail(),
            user.getId()
        );
    }

    public int delete(Long id) {
        return jdbcTemplate.update("DELETE FROM users WHERE id = ?", id);
    }
}
```

### Spring Boot Configuration

```yaml
# application.yml
spring:
  datasource:
    url: jdbc:postgresql://localhost:5432/scratchbird
    username: app_user
    password: secret
    driver-class-name: org.postgresql.Driver
    hikari:
      minimum-idle: 5
      maximum-pool-size: 20
      idle-timeout: 600000
      connection-timeout: 30000
      max-lifetime: 1800000
```

---

## Common Issues

### Issue: Connection Timeout

```java
// Solution: Configure connection timeout
Properties props = new Properties();
props.setProperty("connectTimeout", "30");  // 30 seconds
props.setProperty("socketTimeout", "60");   // 60 seconds

Connection conn = DriverManager.getConnection(url, props);

// Or with HikariCP
HikariConfig config = new HikariConfig();
config.setConnectionTimeout(30000);  // 30 seconds
```

### Issue: Too Many Connections

```java
// Solution: Use connection pooling with limits
HikariConfig config = new HikariConfig();
config.setMaximumPoolSize(20);  // Limit max connections

// Always close connections
try (Connection conn = dataSource.getConnection()) {
    // Use connection
}  // Auto-closed here
```

### Issue: SSL Required

```java
// Solution: Configure SSL
Properties props = new Properties();
props.setProperty("ssl", "true");
props.setProperty("sslmode", "require");

// Or in URL
String url = "jdbc:postgresql://localhost:5432/scratchbird?ssl=true&sslmode=require";
```

---

## See Also

- [Driver Comparison](Driver-Comparison.md) - Compare all drivers
- [First Connection](../getting-started/first-connection.md) - Getting started guide
- [Connection Problems](../troubleshooting/Connection-Problems.md) - Troubleshooting

