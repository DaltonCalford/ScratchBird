# Go (Golang) Driver Specification
**Priority:** P0 (Critical - Beta Required)
**Status:** Draft
**Target Market:** Growing adoption in cloud-native, microservices, DevOps tools, infrastructure
**Use Cases:** Microservices, Cloud-native apps, CLI tools, Kubernetes operators, high-performance services

---


## ScratchBird V2 Scope (Native Protocol Only)

- Protocol: ScratchBird Native Wire Protocol (SBWP) v1.1 only.
- Default port: 3092.
- TLS 1.3 required.
- Emulated protocol drivers (PostgreSQL/MySQL/Firebird/TDS) are out of scope.


## Wrapper Types
- JSONB: `scratchbird.JSONB`
- RANGE: `scratchbird.Range[T]`
- GEOMETRY: `scratchbird.Geometry`

## Overview

Go has rapidly grown in popularity for cloud-native development, microservices, and infrastructure tooling. A database/sql compliant driver is essential for ScratchBird adoption in modern distributed systems and cloud infrastructure.

**Key Requirements:**
- database/sql.Driver interface compliance
- Context support for cancellation and timeouts
- Connection pooling (via database/sql package)
- Prepared statement support
- Transaction management
- Pure Go implementation (no CGO preferred)
- go mod module support
- Comprehensive error handling with error wrapping

## Usage examples

```go
db, _ := sql.Open("scratchbird", "scratchbird://user:pass@localhost:3092/db")
defer db.Close()

_ = db.QueryRow("select 1 as one")

stmt, _ := db.Prepare("select * from widgets where id = $1")
rows, _ := stmt.Query(42)
_ = rows
```

---

## Specification Documents

### Required Documents

- [x] [SPECIFICATION.md](SPECIFICATION.md) - Detailed technical specification
  - database/sql.Driver interface implementation
  - Connection lifecycle and pooling
  - Prepared statements and query execution
  - Type mappings (Go ↔ ScratchBird SQL)
  - Context propagation for timeouts and cancellation
  - Error types and handling
  - Connection string format (DSN)

- [x] [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) - Development roadmap
  - Milestone 1: Basic driver.Driver implementation
  - Milestone 2: Prepared statements and query optimization
  - Milestone 3: Context support and cancellation
  - Milestone 4: Advanced features (bulk copy, notifications)
  - Milestone 5: ORM integrations (GORM, sqlx)
  - Resource requirements
  - Timeline estimates

- [x] [TESTING_CRITERIA.md](TESTING_CRITERIA.md) - Test requirements
  - database/sql compliance tests
  - Integration tests with real database
  - Performance benchmarks vs pgx (PostgreSQL Go driver)
  - Concurrent access tests (goroutine safety)
  - Context cancellation tests
  - Memory leak tests
  - Fuzzing tests for query parser

- [x] [COMPATIBILITY_MATRIX.md](COMPATIBILITY_MATRIX.md) - Version support
  - Go versions (1.19, 1.20, 1.21, 1.22+)
  - Operating systems (Linux, macOS, Windows)
  - Architectures (amd64, arm64, 386, arm)
  - ORM compatibility (GORM, sqlx, ent)
  - Build tools (go mod, go build)

- [x] [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md) - Migration from other drivers
  - From lib/pq (PostgreSQL driver)
  - From pgx (high-performance PostgreSQL driver)
  - From go-sql-driver/mysql
  - From go-firebirdsql
  - DSN format changes
  - Code migration examples

- [x] [API_REFERENCE.md](API_REFERENCE.md) - Complete API documentation (GoDoc)
  - Driver type
  - Conn type
  - Stmt type
  - Rows type
  - Tx type
  - Error types
  - Helper functions

### Shared References

- [../DRIVER_BASELINE_SPEC.md](../DRIVER_BASELINE_SPEC.md) - Shared V2 driver requirements.

### Examples Directory

- [ ] **examples/basic_connection.go** - Simple connection and query
- [ ] **examples/prepared_statements.go** - Using prepared statements
- [ ] **examples/transactions.go** - Transaction management
- [ ] **examples/context_usage.go** - Context for timeouts and cancellation
- [ ] **examples/bulk_operations.go** - Efficient bulk inserts
- [ ] **examples/concurrent_access.go** - Goroutine-safe concurrent queries
- [ ] **examples/gorm/** - GORM ORM integration
  - **main.go** - GORM setup and usage
  - **models.go** - Model definitions
  - **migrations.go** - Database migrations
- [ ] **examples/sqlx/** - sqlx library integration
- [ ] **examples/web-service/** - HTTP API with database
  - **main.go** - HTTP server setup
  - **handlers.go** - Request handlers
  - **db.go** - Database layer
- [ ] **examples/kubernetes-operator/** - Kubernetes operator pattern

---

## Key Integration Points

### database/sql Package Integration

**Critical**: All Go database drivers must implement the database/sql.Driver interface.

Requirements:
- driver.Driver implementation
- driver.Conn implementation
- driver.Stmt implementation (prepared statements)
- driver.Tx implementation (transactions)
- driver.Rows implementation (result sets)
- driver.Result implementation (INSERT/UPDATE results)
- Context-aware interfaces (driver.ConnBeginTx, driver.ConnPrepareContext, etc.)

Example usage:
```go
package main

import (
    "context"
    "database/sql"
    "fmt"
    "log"
    "time"

    _ "github.com/scratchbird/scratchbird-go" // Register driver
)

func main() {
    // Open database connection
    db, err := sql.Open("scratchbird", "host=localhost port=3092 dbname=mydb user=myuser password=mypass")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    // Configure connection pool
    db.SetMaxOpenConns(25)
    db.SetMaxIdleConns(5)
    db.SetConnMaxLifetime(5 * time.Minute)
    db.SetConnMaxIdleTime(10 * time.Minute)

    // Verify connection
    ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
    defer cancel()

    if err := db.PingContext(ctx); err != nil {
        log.Fatal(err)
    }

    // Simple query
    rows, err := db.QueryContext(ctx, "SELECT id, name, email FROM users")
    if err != nil {
        log.Fatal(err)
    }
    defer rows.Close()

    for rows.Next() {
        var id int
        var name, email string
        if err := rows.Scan(&id, &name, &email); err != nil {
            log.Fatal(err)
        }
        fmt.Printf("User %d: %s <%s>\n", id, name, email)
    }
    if err := rows.Err(); err != nil {
        log.Fatal(err)
    }

    // Parameterized query (prevents SQL injection)
    var userEmail string
    err = db.QueryRowContext(ctx, "SELECT email FROM users WHERE id = $1", 123).Scan(&userEmail)
    if err == sql.ErrNoRows {
        fmt.Println("No user found")
    } else if err != nil {
        log.Fatal(err)
    } else {
        fmt.Println("Email:", userEmail)
    }

    // Transaction example
    tx, err := db.BeginTx(ctx, &sql.TxOptions{
        Isolation: sql.LevelSerializable,
    })
    if err != nil {
        log.Fatal(err)
    }
    defer tx.Rollback() // Rollback if not committed

    _, err = tx.ExecContext(ctx,
        "INSERT INTO users (name, email) VALUES ($1, $2)",
        "Alice", "alice@example.com",
    )
    if err != nil {
        log.Fatal(err)
    }

    _, err = tx.ExecContext(ctx,
        "INSERT INTO users (name, email) VALUES ($1, $2)",
        "Bob", "bob@example.com",
    )
    if err != nil {
        log.Fatal(err)
    }

    if err := tx.Commit(); err != nil {
        log.Fatal(err)
    }
}
```

### GORM Integration

**Important**: GORM is the most popular Go ORM with automatic migrations and associations.

Requirements:
- GORM dialect implementation
- Type mappings
- Migration support
- Association support (has-one, has-many, many-to-many)

Example GORM usage:
```go
package main

import (
    "gorm.io/gorm"
    "gorm.io/driver/scratchbird"
)

type User struct {
    gorm.Model
    Name  string
    Email string `gorm:"uniqueIndex"`
    Posts []Post
}

type Post struct {
    gorm.Model
    Title   string
    Content string
    UserID  uint
    User    User
}

func main() {
    dsn := "host=localhost port=3092 dbname=mydb user=myuser password=mypass"
    db, err := gorm.Open(scratchbird.Open(dsn), &gorm.Config{})
    if err != nil {
        panic(err)
    }

    // Auto-migrate schema
    db.AutoMigrate(&User{}, &Post{})

    // Create
    user := User{Name: "Alice", Email: "alice@example.com"}
    db.Create(&user)

    // Read
    var foundUser User
    db.First(&foundUser, "email = ?", "alice@example.com")

    // Update
    db.Model(&foundUser).Update("Name", "Alice Smith")

    // Delete
    db.Delete(&foundUser)

    // Associations
    db.Preload("Posts").Find(&foundUser)
}
```

### sqlx Integration

**Important**: sqlx provides extensions on top of database/sql for easier struct mapping.

Requirements:
- Full compatibility with sqlx
- Named parameter support
- Struct scanning support

Example sqlx usage:
```go
package main

import (
    "github.com/jmoiron/sqlx"
    _ "github.com/scratchbird/scratchbird-go"
)

type User struct {
    ID    int    `db:"id"`
    Name  string `db:"name"`
    Email string `db:"email"`
}

func main() {
    db, err := sqlx.Connect("scratchbird", "host=localhost port=3092 dbname=mydb user=myuser password=mypass")
    if err != nil {
        panic(err)
    }
    defer db.Close()

    // Select into struct slice
    users := []User{}
    err = db.Select(&users, "SELECT * FROM users WHERE active = $1", true)
    if err != nil {
        panic(err)
    }

    // Select single row into struct
    var user User
    err = db.Get(&user, "SELECT * FROM users WHERE id = $1", 123)
    if err != nil {
        panic(err)
    }

    // Named queries
    _, err = db.NamedExec("INSERT INTO users (name, email) VALUES (:name, :email)",
        map[string]interface{}{
            "name":  "Alice",
            "email": "alice@example.com",
        })
}
```

### Context Support (Cancellation and Timeouts)

Requirements:
- Context propagation through all operations
- Proper cancellation handling
- Timeout support
- Deadline handling

Example:
```go
func queryWithTimeout(db *sql.DB) error {
    // Create context with 5-second timeout
    ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
    defer cancel()

    rows, err := db.QueryContext(ctx, "SELECT * FROM large_table")
    if err != nil {
        if ctx.Err() == context.DeadlineExceeded {
            return fmt.Errorf("query timed out after 5 seconds")
        }
        return err
    }
    defer rows.Close()

    for rows.Next() {
        // Check if context was cancelled
        if ctx.Err() != nil {
            return ctx.Err()
        }

        var data string
        if err := rows.Scan(&data); err != nil {
            return err
        }
        // Process data
    }

    return rows.Err()
}
```

---

## Driver API Implementation

### Driver Registration

```go
package scratchbird

import (
    "database/sql"
    "database/sql/driver"
)

type Driver struct{}

func init() {
    sql.Register("scratchbird", &Driver{})
}

func (d *Driver) Open(dsn string) (driver.Conn, error) {
    connector, err := NewConnector(dsn)
    if err != nil {
        return nil, err
    }
    return connector.Connect(context.Background())
}

func (d *Driver) OpenConnector(dsn string) (driver.Connector, error) {
    return NewConnector(dsn)
}
```

### DSN (Data Source Name) Format

```
host=localhost port=3092 dbname=mydb user=myuser password=mypass sslmode=require

Query parameters:
  host         - Database host (default: localhost)
  port         - Database port (default: 3092)
  dbname       - Database name (required)
  user         - Username (required)
  password     - Password
  sslmode      - SSL mode (disable, allow, prefer, require, verify-ca, verify-full)
  connect_timeout - Connection timeout in seconds
  statement_timeout - Statement timeout in seconds
  timezone     - Session timezone
  application_name - Application name for logging

Alternative URL format:
  scratchbird://user:password@localhost:3092/mydb?sslmode=require
```

### Connection Interface

```go
type Conn struct {
    conn net.Conn
    cfg  *Config
    // internal state
}

func (c *Conn) Prepare(query string) (driver.Stmt, error)
func (c *Conn) PrepareContext(ctx context.Context, query string) (driver.Stmt, error)

func (c *Conn) Begin() (driver.Tx, error)
func (c *Conn) BeginTx(ctx context.Context, opts driver.TxOptions) (driver.Tx, error)

func (c *Conn) Close() error

func (c *Conn) Query(query string, args []driver.Value) (driver.Rows, error)
func (c *Conn) QueryContext(ctx context.Context, query string, args []driver.NamedValue) (driver.Rows, error)

func (c *Conn) Exec(query string, args []driver.Value) (driver.Result, error)
func (c *Conn) ExecContext(ctx context.Context, query string, args []driver.NamedValue) (driver.Result, error)
```

### Error Types

```go
package scratchbird

import (
    "fmt"
)

type Error struct {
    Severity string
    Code     string
    Message  string
    Detail   string
    Hint     string
    Position int
    Where    string
    Schema   string
    Table    string
    Column   string
    File     string
    Line     int
    Routine  string
}

func (e *Error) Error() string {
    return fmt.Sprintf("%s: %s (SQLSTATE %s)", e.Severity, e.Message, e.Code)
}

// Specific error types
var (
    ErrNotConnected      = fmt.Errorf("not connected to database")
    ErrClosed            = fmt.Errorf("connection is closed")
    ErrTimeout           = fmt.Errorf("operation timeout")
    ErrCancelled         = fmt.Errorf("operation cancelled")
    ErrInvalidConnection = fmt.Errorf("invalid connection")
)
```

---

## Performance Targets

Benchmark against pgx (high-performance PostgreSQL Go driver):

| Operation | Target Performance |
|-----------|-------------------|
| Connection establishment | Within 10% of pgx |
| Simple SELECT (1 row) | Within 10% of pgx |
| Bulk SELECT (10,000 rows) | Within 15% of pgx |
| Prepared statement execute | Within 10% of pgx |
| Bulk INSERT (10,000 rows) | Within 15% of pgx |
| Transaction commit | Within 10% of pgx |
| Concurrent queries (100 goroutines) | Within 15% of pgx |
| Memory allocations | Minimize allocations per query |

---

## Dependencies

### Runtime Dependencies

**Preferred: Zero dependencies** (pure Go implementation)

Optional:
- golang.org/x/crypto (for SCRAM authentication)
- golang.org/x/text (for encoding support)

### Development Dependencies

- Go 1.19 or later
- golang.org/x/tools/go/analysis (linting)
- github.com/stretchr/testify (testing)

### Build Requirements

- Go 1.19+ toolchain
- No CGO (pure Go preferred)
- Cross-compilation support for all platforms

---

## Go Module

```
module github.com/scratchbird/scratchbird-go

go 1.19

require (
    golang.org/x/crypto v0.x.x // optional, for auth
)
```

---

## Documentation Requirements

### User Documentation

- [ ] Quick start guide (5-minute tutorial)
- [ ] Installation instructions (go get)
- [ ] DSN format and connection options
- [ ] Connection pooling configuration
- [ ] Transaction management guide
- [ ] Context usage and best practices
- [ ] Error handling patterns
- [ ] Performance tuning tips
- [ ] Deployment guide (containers, Kubernetes)

### API Documentation

- [ ] Complete GoDoc for all exported types
- [ ] Examples in GoDoc
- [ ] Type mapping reference
- [ ] Error code reference

### Integration Guides

- [ ] GORM integration guide
- [ ] sqlx integration guide
- [ ] ent (Facebook's ORM) integration
- [ ] Echo/Gin web framework integration
- [ ] gRPC service integration
- [ ] Kubernetes operator development
- [ ] Cloud Run deployment

---

## Release Criteria

### Functional Completeness

- [ ] database/sql.Driver interface compliance
- [ ] Connection pooling (via database/sql)
- [ ] Prepared statements
- [ ] Transaction management (all isolation levels)
- [ ] Context support for all operations
- [ ] All standard Go SQL types supported
- [ ] Binary and text protocol support
- [ ] SSL/TLS support

### Quality Metrics

- [ ] >90% test coverage
- [ ] database/sql compliance tests passing
- [ ] Performance benchmarks met (within 10-15% of pgx)
- [ ] Goroutine safety verified
- [ ] Context cancellation tests passing
- [ ] Memory leak tests passing
- [ ] Race detector clean (go test -race)

### Documentation

- [ ] Complete GoDoc for all public APIs
- [ ] User guide complete
- [ ] 15+ code examples
- [ ] Migration guide from lib/pq and pgx
- [ ] Troubleshooting guide
- [ ] GORM dialect documentation

### Packaging

- [ ] Go module published (go.mod)
- [ ] Semantic versioning
- [ ] Tagged releases on GitHub
- [ ] pkg.go.dev documentation
- [ ] Automated CI/CD (GitHub Actions)
- [ ] Multi-platform testing (Linux, macOS, Windows)

---

## Community and Support

### Package Locations

- Go Module: github.com/scratchbird/scratchbird-go
- Documentation: https://pkg.go.dev/github.com/scratchbird/scratchbird-go
- GitHub: https://github.com/scratchbird/scratchbird-go
- Examples: https://github.com/scratchbird/scratchbird-go/tree/main/examples

### Issue Tracking

- GitHub Issues: https://github.com/scratchbird/scratchbird-go/issues
- Response time target: <3 days
- Bug fix SLA: Critical <7 days, High <14 days

### Community Channels

- Discord: ScratchBird #go-driver
- Gophers Slack: #scratchbird
- Stack Overflow: Tag `scratchbird-go` or `scratchbird`

---

## References

1. **database/sql Package Documentation**
   https://pkg.go.dev/database/sql

2. **database/sql/driver Package**
   https://pkg.go.dev/database/sql/driver

3. **pgx Driver** (reference implementation)
   https://github.com/jackc/pgx

4. **lib/pq Driver** (standard PostgreSQL driver)
   https://github.com/lib/pq

5. **GORM Documentation**
   https://gorm.io/

6. **sqlx Documentation**
   https://jmoiron.github.io/sqlx/

---

## Next Steps

1. **Create detailed SPECIFICATION.md** with complete driver interface implementation
2. **Develop IMPLEMENTATION_PLAN.md** with milestones and timeline
3. **Write TESTING_CRITERIA.md** with database/sql compliance tests
4. **Begin prototype** with basic Driver and Conn implementation
5. **Set up CI/CD** with GitHub Actions for multi-platform testing
6. **Create GORM dialect** (critical for Go ORM adoption)

---

**Document Version:** 1.0 (Template)
**Last Updated:** 2026-01-03
**Status:** Draft
**Assigned To:** TBD
