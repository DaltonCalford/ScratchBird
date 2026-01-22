# Java JDBC Driver Specification
**Priority:** P0 (Critical - Beta Required)
**Status:** Draft
**Target Market:** ~30% of developers worldwide, enterprise applications
**Use Cases:** Enterprise applications, Android development, big data (Spark, Hadoop), BI tools

---


## ScratchBird V2 Scope (Native Protocol Only)

- Protocol: ScratchBird Native Wire Protocol (SBWP) v1.1 only.
- Default port: 3092.
- TLS 1.3 required.
- Emulated protocol drivers (PostgreSQL/MySQL/Firebird/TDS) are out of scope.


## Wrapper Types
- JSONB: `SBJsonb`
- RANGE: `SBRange<T>`
- GEOMETRY: `SBGeometry`

## Overview

Java remains dominant in enterprise software, with JDBC (Java Database Connectivity) being the standard database access API. A JDBC 4.2+ compliant driver is essential for ScratchBird enterprise adoption and integration with the Java ecosystem.

**Key Requirements:**
- JDBC 4.2+ specification compliance
- Type 4 driver (pure Java, no native code)
- Connection pooling support (HikariCP, Apache DBCP2, c3p0)
- Prepared statement support with parameter binding
- Batch execution support
- Transaction management (ACID compliance)
- ResultSet streaming for large datasets
- Metadata introspection (DatabaseMetaData, ResultSetMetaData)
- SSL/TLS support

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

---

## Specification Documents

### Required Documents

- [x] [SPECIFICATION.md](SPECIFICATION.md) - Detailed technical specification
  - JDBC 4.2 compliance matrix
  - Driver architecture (Type 4 pure Java)
  - Connection lifecycle management
  - Statement types (Statement, PreparedStatement, CallableStatement)
  - ResultSet types and concurrency modes
  - Transaction isolation levels
  - Type mappings (Java ↔ SQL ↔ ScratchBird)
  - Exception hierarchy

- [x] [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) - Development roadmap
  - Milestone 1: Basic JDBC connectivity (Connection, Statement, ResultSet)
  - Milestone 2: PreparedStatement and batch execution
  - Milestone 3: Transaction management and savepoints
  - Milestone 4: Connection pooling integration
  - Milestone 5: Hibernate dialect
  - Milestone 6: Spring Boot starter
  - Resource requirements
  - Timeline estimates

- [x] [TESTING_CRITERIA.md](TESTING_CRITERIA.md) - Test requirements
  - JDBC TCK (Technology Compatibility Kit) compliance
  - Hibernate test suite compatibility
  - Performance benchmarks vs PostgreSQL JDBC driver
  - Connection pool stress tests (HikariCP, DBCP2)
  - Concurrency and thread safety tests
  - Memory leak tests (long-running applications)

- [x] [COMPATIBILITY_MATRIX.md](COMPATIBILITY_MATRIX.md) - Version support
  - Java versions (8, 11, 17 LTS, 21 LTS)
  - JDBC versions (4.2, 4.3)
  - Application servers (Tomcat, Jetty, WildFly, WebSphere)
  - Connection pools (HikariCP, DBCP2, c3p0, Tomcat JDBC)
  - ORMs (Hibernate 5.x, 6.x, MyBatis, jOOQ)
  - Build tools (Maven, Gradle)

- [x] [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md) - Migration from other drivers
  - From PostgreSQL JDBC driver (org.postgresql.Driver)
  - From MySQL Connector/J (com.mysql.cj.jdbc.Driver)
  - From Oracle JDBC driver
  - From Firebird Jaybird driver
  - Connection string format changes
  - Configuration differences

- [x] [API_REFERENCE.md](API_REFERENCE.md) - Complete API documentation (JavaDoc)
  - Driver class
  - Connection class
  - Statement classes
  - ResultSet class
  - DataSource implementation
  - Exception classes

### Shared References

- [../DRIVER_BASELINE_SPEC.md](../DRIVER_BASELINE_SPEC.md) - Shared V2 driver requirements.

### Examples Directory

- [ ] **examples/BasicConnection.java** - Simple JDBC connection
- [ ] **examples/PreparedStatementExample.java** - Parameterized queries
- [ ] **examples/BatchInsert.java** - Batch execution
- [ ] **examples/TransactionExample.java** - Transaction management
- [ ] **examples/ConnectionPooling.java** - HikariCP configuration
- [ ] **examples/MetadataIntrospection.java** - DatabaseMetaData usage
- [ ] **examples/hibernate/** - Hibernate ORM integration
  - **hibernate.cfg.xml** - Hibernate configuration
  - **persistence.xml** - JPA configuration
  - **Entity.java** - Sample entity class
  - **DAO.java** - Data access object
- [ ] **examples/spring-boot/** - Spring Boot application
  - **application.properties** - Spring configuration
  - **DataSourceConfig.java** - DataSource bean
  - **Repository.java** - Spring Data repository
- [ ] **examples/mybatis/** - MyBatis integration
- [ ] **examples/jooq/** - jOOQ code generation and usage

---

## Key Integration Points

### Hibernate/JPA Support

**Critical**: Hibernate is the most popular Java ORM, used in majority of enterprise Java applications.

Requirements:
- Custom Hibernate dialect (`ScratchBirdDialect extends Dialect`)
- Type mappings for all ScratchBird types
- DDL generation support (schema creation)
- Sequence and identity column support
- Pagination support (LIMIT/OFFSET)
- Locking strategies (pessimistic, optimistic)

Example Hibernate dialect:
```java
package org.scratchbird.hibernate.dialect;

import org.hibernate.dialect.Dialect;
import org.hibernate.dialect.function.StandardSQLFunction;
import org.hibernate.type.StandardBasicTypes;

public class ScratchBirdDialect extends Dialect {

    public ScratchBirdDialect() {
        super();

        // Register column types
        registerColumnType(Types.BIT, "boolean");
        registerColumnType(Types.BIGINT, "bigint");
        registerColumnType(Types.SMALLINT, "smallint");
        registerColumnType(Types.INTEGER, "integer");
        registerColumnType(Types.CHAR, "char($l)");
        registerColumnType(Types.VARCHAR, "varchar($l)");
        registerColumnType(Types.FLOAT, "float");
        registerColumnType(Types.DOUBLE, "double precision");
        registerColumnType(Types.TIMESTAMP, "timestamp");
        registerColumnType(Types.BLOB, "blob");
        registerColumnType(Types.CLOB, "clob");

        // Register functions
        registerFunction("substring", new StandardSQLFunction("substring", StandardBasicTypes.STRING));
        registerFunction("upper", new StandardSQLFunction("upper", StandardBasicTypes.STRING));
        registerFunction("lower", new StandardSQLFunction("lower", StandardBasicTypes.STRING));
    }

    @Override
    public String getAddColumnString() {
        return "add column";
    }

    @Override
    public boolean supportsIdentityColumns() {
        return true;
    }

    @Override
    public String getIdentityColumnString() {
        return "generated by default as identity";
    }

    @Override
    public boolean supportsSequences() {
        return true;
    }

    @Override
    public String getSequenceNextValString(String sequenceName) {
        return "select next value for " + sequenceName;
    }

    @Override
    public boolean supportsLimit() {
        return true;
    }

    @Override
    public String getLimitString(String sql, boolean hasOffset) {
        return sql + (hasOffset ? " limit ? offset ?" : " limit ?");
    }
}
```

### Spring Boot Support

Requirements:
- Auto-configuration for Spring Boot
- Spring Boot starter package (`spring-boot-starter-scratchbird`)
- DataSource auto-configuration
- Transaction manager integration
- Spring Data JPA compatibility

Example Spring Boot starter:
```java
// ScratchBirdAutoConfiguration.java
@Configuration
@ConditionalOnClass(DataSource.class)
@EnableConfigurationProperties(ScratchBirdProperties.class)
public class ScratchBirdAutoConfiguration {

    @Bean
    @ConditionalOnMissingBean
    public DataSource dataSource(ScratchBirdProperties properties) {
        HikariConfig config = new HikariConfig();
        config.setJdbcUrl(properties.getUrl());
        config.setUsername(properties.getUsername());
        config.setPassword(properties.getPassword());
        config.setDriverClassName("org.scratchbird.jdbc.Driver");
        config.setMaximumPoolSize(properties.getPoolSize());
        return new HikariDataSource(config);
    }
}

// application.properties
spring.datasource.url=jdbc:scratchbird://localhost:3092/mydb
spring.datasource.username=myuser
spring.datasource.password=mypassword
spring.datasource.driver-class-name=org.scratchbird.jdbc.Driver
spring.jpa.database-platform=org.scratchbird.hibernate.dialect.ScratchBirdDialect
```

### Basic JDBC API

Example usage:
```java
import java.sql.*;

public class BasicExample {
    public static void main(String[] args) {
        String url = "jdbc:scratchbird://localhost:3092/mydb";
        String user = "myuser";
        String password = "mypassword";

        try (Connection conn = DriverManager.getConnection(url, user, password)) {

            // Simple query
            try (Statement stmt = conn.createStatement();
                 ResultSet rs = stmt.executeQuery("SELECT * FROM users")) {

                while (rs.next()) {
                    int id = rs.getInt("id");
                    String name = rs.getString("name");
                    System.out.println("User: " + id + " - " + name);
                }
            }

            // Parameterized query (PreparedStatement)
            String sql = "SELECT * FROM users WHERE email = ?";
            try (PreparedStatement pstmt = conn.prepareStatement(sql)) {
                pstmt.setString(1, "user@example.com");

                try (ResultSet rs = pstmt.executeQuery()) {
                    if (rs.next()) {
                        System.out.println("Found user: " + rs.getString("name"));
                    }
                }
            }

            // Transaction example
            conn.setAutoCommit(false);
            try (PreparedStatement pstmt = conn.prepareStatement(
                    "INSERT INTO users (name, email) VALUES (?, ?)")) {

                pstmt.setString(1, "Alice");
                pstmt.setString(2, "alice@example.com");
                pstmt.executeUpdate();

                pstmt.setString(1, "Bob");
                pstmt.setString(2, "bob@example.com");
                pstmt.executeUpdate();

                conn.commit();
            } catch (SQLException e) {
                conn.rollback();
                throw e;
            } finally {
                conn.setAutoCommit(true);
            }

        } catch (SQLException e) {
            e.printStackTrace();
        }
    }
}
```

### Connection Pooling (HikariCP)

Requirements:
- Full compatibility with HikariCP (fastest Java connection pool)
- Support for Apache Commons DBCP2
- Support for c3p0
- Proper connection validation
- Statement caching

Example HikariCP configuration:
```java
import com.zaxxer.hikari.HikariConfig;
import com.zaxxer.hikari.HikariDataSource;

public class ConnectionPoolExample {
    public static void main(String[] args) {
        HikariConfig config = new HikariConfig();
        config.setJdbcUrl("jdbc:scratchbird://localhost:3092/mydb");
        config.setUsername("myuser");
        config.setPassword("mypassword");
        config.setDriverClassName("org.scratchbird.jdbc.Driver");

        // Pool configuration
        config.setMaximumPoolSize(20);
        config.setMinimumIdle(5);
        config.setIdleTimeout(300000); // 5 minutes
        config.setConnectionTimeout(30000); // 30 seconds
        config.setMaxLifetime(1800000); // 30 minutes

        // Performance tuning
        config.setAutoCommit(true);
        config.setCachePrepStmts(true);
        config.setPrepStmtCacheSize(250);
        config.setPrepStmtCacheSqlLimit(2048);

        // Connection test
        config.setConnectionTestQuery("SELECT 1");

        HikariDataSource dataSource = new HikariDataSource(config);

        try (Connection conn = dataSource.getConnection()) {
            // Use connection
        } catch (SQLException e) {
            e.printStackTrace();
        }

        dataSource.close();
    }
}
```

---

## JDBC API Implementation

### Driver Class

```java
package org.scratchbird.jdbc;

import java.sql.*;
import java.util.Properties;
import java.util.logging.Logger;

public class Driver implements java.sql.Driver {

    static {
        try {
            DriverManager.registerDriver(new Driver());
        } catch (SQLException e) {
            throw new RuntimeException("Failed to register ScratchBird JDBC driver", e);
        }
    }

    @Override
    public Connection connect(String url, Properties info) throws SQLException {
        if (!acceptsURL(url)) {
            return null;
        }
        return new ScratchBirdConnection(url, info);
    }

    @Override
    public boolean acceptsURL(String url) throws SQLException {
        return url != null && url.startsWith("jdbc:scratchbird:");
    }

    @Override
    public DriverPropertyInfo[] getPropertyInfo(String url, Properties info)
            throws SQLException {
        // Return driver property information
        return new DriverPropertyInfo[0];
    }

    @Override
    public int getMajorVersion() {
        return 1;
    }

    @Override
    public int getMinorVersion() {
        return 0;
    }

    @Override
    public boolean jdbcCompliant() {
        return true; // JDBC 4.2 compliant
    }

    @Override
    public Logger getParentLogger() throws SQLFeatureNotSupportedException {
        return Logger.getLogger("org.scratchbird.jdbc");
    }
}
```

### Connection URL Format

```
jdbc:scratchbird://[host]:[port]/[database][?property1=value1[&property2=value2]...]

Examples:
  jdbc:scratchbird://localhost:3092/mydb
  jdbc:scratchbird://localhost/mydb?user=admin&password=secret
  jdbc:scratchbird://db.example.com:3092/production?ssl=true&connectTimeout=10
```

### Supported Connection Properties

```java
// Connection properties
user              - Database user
password          - Database password
ssl               - Enable SSL (true/false)
sslmode           - SSL mode (disable, allow, prefer, require, verify-ca, verify-full)
connectTimeout    - Connection timeout in seconds
socketTimeout     - Socket read timeout in seconds
loginTimeout      - Login timeout in seconds
prepareThreshold  - Number of executions before switching to server-prepared statement
binaryTransfer    - Use binary format for data transfer (true/false)
loggerLevel       - Logging level (OFF, SEVERE, WARNING, INFO, CONFIG, FINE, FINER, FINEST)
```

---

## Performance Targets

Benchmark against PostgreSQL JDBC driver:

| Operation | Target Performance |
|-----------|-------------------|
| Connection establishment | Within 10% of pgjdbc |
| Simple SELECT (1 row) | Within 10% of pgjdbc |
| Bulk SELECT (10,000 rows) | Within 15% of pgjdbc |
| PreparedStatement execute | Within 10% of pgjdbc |
| Batch INSERT (1,000 rows) | Within 15% of pgjdbc |
| Transaction commit | Within 10% of pgjdbc |
| ResultSet iteration (100k rows) | Within 15% of pgjdbc |
| Connection pool (HikariCP, 100 queries) | Within 10% of pgjdbc |

---

## Dependencies

### Compile Dependencies

Minimal dependencies (preferably none for core driver):
- None (pure Java implementation preferred)

### Optional Dependencies

- SLF4J (for logging integration)

### Test Dependencies

- JUnit 5
- Mockito
- TestContainers (for integration tests)
- HikariCP (for connection pool tests)
- Hibernate (for ORM integration tests)

### Build Requirements

- JDK 11 or later (for development)
- Maven 3.6+ or Gradle 7+
- Target bytecode: Java 8 compatibility

---

## Maven Coordinates

```xml
<!-- Maven dependency (future) -->
<dependency>
    <groupId>org.scratchbird</groupId>
    <artifactId>scratchbird-jdbc</artifactId>
    <version>1.0.0</version>
</dependency>

<!-- Hibernate dialect (separate artifact) -->
<dependency>
    <groupId>org.scratchbird</groupId>
    <artifactId>scratchbird-hibernate</artifactId>
    <version>1.0.0</version>
</dependency>

<!-- Spring Boot starter -->
<dependency>
    <groupId>org.scratchbird</groupId>
    <artifactId>spring-boot-starter-scratchbird</artifactId>
    <version>1.0.0</version>
</dependency>
```

```groovy
// Gradle dependency
implementation 'org.scratchbird:scratchbird-jdbc:1.0.0'
implementation 'org.scratchbird:scratchbird-hibernate:1.0.0'
implementation 'org.scratchbird:spring-boot-starter-scratchbird:1.0.0'
```

---

## Documentation Requirements

### User Documentation

- [ ] Quick start guide (5-minute tutorial)
- [ ] Installation instructions (Maven, Gradle)
- [ ] Connection string format and properties
- [ ] Connection pooling best practices
- [ ] Transaction management guide
- [ ] PreparedStatement usage and SQL injection prevention
- [ ] Batch execution guide
- [ ] Error handling and exception hierarchy
- [ ] Performance tuning tips

### API Documentation

- [ ] Full JavaDoc for all public classes
- [ ] JDBC 4.2 compliance matrix
- [ ] Supported SQL types mapping
- [ ] DatabaseMetaData capabilities

### Integration Guides

- [ ] Hibernate/JPA integration guide
- [ ] Spring Boot integration guide
- [ ] MyBatis integration guide
- [ ] jOOQ integration guide
- [ ] Apache Spark integration
- [ ] Tomcat deployment guide
- [ ] WildFly/JBoss deployment guide

---

## Release Criteria

### Functional Completeness

- [ ] JDBC 4.2 specification compliance
- [ ] Driver registration with DriverManager
- [ ] Connection, Statement, PreparedStatement, CallableStatement
- [ ] ResultSet (forward-only, scrollable, updatable)
- [ ] Transaction management (commit, rollback, savepoints)
- [ ] Batch execution
- [ ] DatabaseMetaData and ResultSetMetaData
- [ ] All standard SQL types supported
- [ ] SSL/TLS support

### Quality Metrics

- [ ] >90% test coverage
- [ ] JDBC TCK compliance tests passing
- [ ] Hibernate test suite passing
- [ ] Performance benchmarks met (within 10-15% of pgjdbc)
- [ ] Thread safety tests passing
- [ ] Memory leak tests passing (24-hour stress test)
- [ ] Connection pool integration tests passing

### Documentation

- [ ] Complete JavaDoc for all public APIs
- [ ] User guide complete
- [ ] 15+ code examples
- [ ] Migration guide from PostgreSQL/MySQL JDBC
- [ ] Troubleshooting guide
- [ ] Hibernate dialect documentation

### Packaging

- [ ] Maven Central deployment
- [ ] JAR signing (GPG)
- [ ] Source JAR
- [ ] JavaDoc JAR
- [ ] OSGi manifest (optional)
- [ ] Automated CI/CD

---

## Community and Support

### Artifact Locations

- Maven Central: https://central.sonatype.com/artifact/org.scratchbird/scratchbird-jdbc
- GitHub: https://github.com/scratchbird/scratchbird-jdbc
- Documentation: https://scratchbird.dev/docs/jdbc/
- JavaDoc: https://javadoc.io/doc/org.scratchbird/scratchbird-jdbc

### Issue Tracking

- GitHub Issues: https://github.com/scratchbird/scratchbird-jdbc/issues
- Response time target: <3 days
- Bug fix SLA: Critical <7 days, High <14 days

### Community Channels

- Discord: ScratchBird #java-jdbc
- Stack Overflow: Tag `scratchbird-jdbc` or `scratchbird`
- Mailing list: scratchbird-jdbc@googlegroups.com

---

## References

1. **JDBC 4.2 Specification**
   https://jcp.org/en/jsr/detail?id=221

2. **PostgreSQL JDBC Driver** (reference implementation)
   https://jdbc.postgresql.org/

3. **Hibernate ORM Documentation**
   https://hibernate.org/orm/documentation/

4. **Spring Framework Data Access**
   https://docs.spring.io/spring-framework/reference/data-access.html

5. **HikariCP Documentation**
   https://github.com/brettwooldridge/HikariCP

---

## Next Steps

1. **Create detailed SPECIFICATION.md** with complete JDBC 4.2 compliance matrix
2. **Develop IMPLEMENTATION_PLAN.md** with milestones and timeline
3. **Write TESTING_CRITERIA.md** with JDBC TCK requirements
4. **Begin prototype** with basic Driver and Connection classes
5. **Set up Maven/Gradle builds** and CI/CD pipeline
6. **Create Hibernate dialect** (critical for enterprise adoption)

---

**Document Version:** 1.0 (Template)
**Last Updated:** 2026-01-03
**Status:** Draft
**Assigned To:** TBD
