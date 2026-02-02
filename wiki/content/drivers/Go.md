# Go Driver Guide

**Status:** Complete
**Last Updated:** 2026-01-30

---

## Overview

ScratchBird supports multiple connection protocols for Go applications:

| Protocol | Port | Driver | Best For |
|----------|------|--------|----------|
| Native | 3092 | scratchbird-go (SBWP v1.1) | Full ScratchBird feature set |
| PostgreSQL | 5432 | pgx, lib/pq | Ecosystem compatibility |
| MySQL | 3306 | go-sql-driver/mysql | MySQL compatibility |
| Firebird | 3050 | nakagami/firebirdsql | Firebird migration |

**Recommendation:** Use **scratchbird-go** for full SBWP v1.1 feature coverage. Use pgx/lib/pq only when you need emulation compatibility.

---

## Part 1: Quick Start

### Installation

```bash
# ScratchBird native driver (recommended)
go get github.com/scratchbird/scratchbird-go

# pgx v5 (PostgreSQL protocol - recommended)
go get github.com/jackc/pgx/v5

# lib/pq (PostgreSQL protocol - database/sql compatible)
go get github.com/lib/pq

# MySQL driver
go get github.com/go-sql-driver/mysql

# Firebird driver
go get github.com/nakagami/firebirdsql

# GORM (ORM)
go get gorm.io/gorm
go get gorm.io/driver/postgres
```

### Install via sb_setup (Installer Utility)

If you installed ScratchBird with the installer, you can add the native driver pack later:

```bash
sb_setup --interactive
```

Select `scratchbird-driver-go` or the `scratchbird-drivers-all` meta package. On Linux, run with `sudo`.

### First Connection

```go
package main

import (
    "log"

    "database/sql"
    _ "github.com/scratchbird/scratchbird-go"
)

func main() {
    db, err := sql.Open("scratchbird", "scratchbird://app_user:secret@localhost:3092/scratchbird")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    var one int
    if err := db.QueryRow("SELECT 1").Scan(&one); err != nil {
        log.Fatal(err)
    }
}
```

The native driver uses SBWP v1.1 with server-side prepare/bind and binary-only
parameters. Wrapper types for JSONB/RANGE/GEOMETRY are exposed by the driver API.

---

## Part 2: pgx (PostgreSQL Protocol - Emulation)

pgx is the recommended driver for **PostgreSQL emulation**, offering excellent performance when you
need compatibility with the PostgreSQL ecosystem.

### Connection Options

**Connection string:**
```go
connString := "postgres://app_user:secret@localhost:5432/scratchbird"
conn, err := pgx.Connect(ctx, connString)
```

**Programmatic configuration:**
```go
config, err := pgx.ParseConfig("postgres://localhost:5432/scratchbird")
if err != nil {
    log.Fatal(err)
}

config.User = "app_user"
config.Password = "secret"
config.Database = "scratchbird"
config.ConnectTimeout = 5 * time.Second
config.RuntimeParams["application_name"] = "myapp"

conn, err := pgx.ConnectConfig(ctx, config)
```

**With SSL:**
```go
config, _ := pgx.ParseConfig(connString)
config.TLSConfig = &tls.Config{
    InsecureSkipVerify: false,
    ServerName:         "localhost",
}
```

### CRUD Operations

**Create (INSERT):**
```go
// Single insert with RETURNING
var userID int64
err := conn.QueryRow(ctx,
    `INSERT INTO users (name, email, created_at)
     VALUES ($1, $2, $3)
     RETURNING id`,
    "Alice", "alice@example.com", time.Now(),
).Scan(&userID)

if err != nil {
    log.Fatal(err)
}
fmt.Printf("Created user ID: %d\n", userID)
```

**Read (SELECT):**
```go
// Single row
type User struct {
    ID        int64
    Name      string
    Email     string
    CreatedAt time.Time
}

var user User
err := conn.QueryRow(ctx,
    "SELECT id, name, email, created_at FROM users WHERE id = $1",
    1,
).Scan(&user.ID, &user.Name, &user.Email, &user.CreatedAt)

if err != nil {
    if errors.Is(err, pgx.ErrNoRows) {
        fmt.Println("User not found")
    } else {
        log.Fatal(err)
    }
}

// Multiple rows
rows, err := conn.Query(ctx,
    "SELECT id, name, email, created_at FROM users WHERE active = $1",
    true,
)
if err != nil {
    log.Fatal(err)
}
defer rows.Close()

var users []User
for rows.Next() {
    var u User
    err := rows.Scan(&u.ID, &u.Name, &u.Email, &u.CreatedAt)
    if err != nil {
        log.Fatal(err)
    }
    users = append(users, u)
}

if err := rows.Err(); err != nil {
    log.Fatal(err)
}
```

**Update:**
```go
tag, err := conn.Exec(ctx,
    `UPDATE users SET email = $1, updated_at = $2 WHERE id = $3`,
    "alice.new@example.com", time.Now(), 1,
)
if err != nil {
    log.Fatal(err)
}
fmt.Printf("Updated %d rows\n", tag.RowsAffected())
```

**Delete:**
```go
tag, err := conn.Exec(ctx,
    "DELETE FROM users WHERE id = $1",
    1,
)
if err != nil {
    log.Fatal(err)
}
fmt.Printf("Deleted %d rows\n", tag.RowsAffected())
```

### pgx.CollectRows Helper

```go
// Simplified row collection
users, err := pgx.CollectRows(
    conn.Query(ctx, "SELECT id, name, email FROM users"),
    pgx.RowToStructByName[User],
)
if err != nil {
    log.Fatal(err)
}

for _, u := range users {
    fmt.Printf("%d: %s <%s>\n", u.ID, u.Name, u.Email)
}
```

### Transactions

**Basic transaction:**
```go
tx, err := conn.Begin(ctx)
if err != nil {
    log.Fatal(err)
}
defer tx.Rollback(ctx) // Rollback if not committed

// Debit account
_, err = tx.Exec(ctx,
    "UPDATE accounts SET balance = balance - $1 WHERE id = $2",
    100.00, 1,
)
if err != nil {
    log.Fatal(err)
}

// Credit account
_, err = tx.Exec(ctx,
    "UPDATE accounts SET balance = balance + $1 WHERE id = $2",
    100.00, 2,
)
if err != nil {
    log.Fatal(err)
}

// Commit
err = tx.Commit(ctx)
if err != nil {
    log.Fatal(err)
}
fmt.Println("Transfer completed")
```

**With options:**
```go
txOptions := pgx.TxOptions{
    IsoLevel:   pgx.Serializable,
    AccessMode: pgx.ReadWrite,
}

tx, err := conn.BeginTx(ctx, txOptions)
if err != nil {
    log.Fatal(err)
}
defer tx.Rollback(ctx)
```

**Savepoints:**
```go
tx, err := conn.Begin(ctx)
if err != nil {
    log.Fatal(err)
}
defer tx.Rollback(ctx)

// Create order
_, err = tx.Exec(ctx, "INSERT INTO orders (customer_id) VALUES ($1)", 1)
if err != nil {
    log.Fatal(err)
}

// Savepoint before items
sp, err := tx.Begin(ctx) // Nested transaction = savepoint
if err != nil {
    log.Fatal(err)
}

_, err = tx.Exec(ctx, "INSERT INTO order_items (order_id, product_id) VALUES ($1, $2)", 1, 999)
if err != nil {
    // Rollback to savepoint
    sp.Rollback(ctx)
    fmt.Println("Item insert failed, order kept")
} else {
    sp.Commit(ctx)
}

tx.Commit(ctx)
```

### Connection Pooling (pgxpool)

**Basic pool:**
```go
import "github.com/jackc/pgx/v5/pgxpool"

func main() {
    ctx := context.Background()

    pool, err := pgxpool.New(ctx, "postgres://app_user:secret@localhost:5432/scratchbird")
    if err != nil {
        log.Fatal(err)
    }
    defer pool.Close()

    // Use pool
    var version string
    err = pool.QueryRow(ctx, "SELECT version()").Scan(&version)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Println(version)
}
```

**Pool configuration:**
```go
config, err := pgxpool.ParseConfig("postgres://localhost:5432/scratchbird")
if err != nil {
    log.Fatal(err)
}

config.MaxConns = 25
config.MinConns = 5
config.MaxConnLifetime = time.Hour
config.MaxConnIdleTime = 30 * time.Minute
config.HealthCheckPeriod = time.Minute

config.ConnConfig.User = "app_user"
config.ConnConfig.Password = "secret"

pool, err := pgxpool.NewWithConfig(ctx, config)
```

**Pool statistics:**
```go
stat := pool.Stat()
fmt.Printf("Total connections: %d\n", stat.TotalConns())
fmt.Printf("Acquired connections: %d\n", stat.AcquiredConns())
fmt.Printf("Idle connections: %d\n", stat.IdleConns())
```

### Prepared Statements

```go
// Prepare once, execute many times
conn, err := pool.Acquire(ctx)
if err != nil {
    log.Fatal(err)
}
defer conn.Release()

_, err = conn.Conn().Prepare(ctx, "get_user", "SELECT id, name, email FROM users WHERE id = $1")
if err != nil {
    log.Fatal(err)
}

// Use prepared statement
var user User
err = conn.QueryRow(ctx, "get_user", 1).Scan(&user.ID, &user.Name, &user.Email)
```

### Batch Operations

```go
batch := &pgx.Batch{}

batch.Queue("INSERT INTO logs (message) VALUES ($1)", "Event 1")
batch.Queue("INSERT INTO logs (message) VALUES ($1)", "Event 2")
batch.Queue("INSERT INTO logs (message) VALUES ($1)", "Event 3")

br := conn.SendBatch(ctx, batch)
defer br.Close()

for i := 0; i < batch.Len(); i++ {
    _, err := br.Exec()
    if err != nil {
        log.Printf("Batch item %d failed: %v", i, err)
    }
}
```

**Batch with results:**
```go
batch := &pgx.Batch{}

batch.Queue("SELECT id, name FROM users WHERE id = $1", 1)
batch.Queue("SELECT COUNT(*) FROM orders WHERE user_id = $1", 1)

br := conn.SendBatch(ctx, batch)
defer br.Close()

var user User
err := br.QueryRow().Scan(&user.ID, &user.Name)
if err != nil {
    log.Fatal(err)
}

var orderCount int
err = br.QueryRow().Scan(&orderCount)
if err != nil {
    log.Fatal(err)
}

fmt.Printf("User %s has %d orders\n", user.Name, orderCount)
```

### COPY Protocol

```go
// Bulk insert using COPY
rows := [][]interface{}{
    {"Alice", "alice@example.com", time.Now()},
    {"Bob", "bob@example.com", time.Now()},
    {"Charlie", "charlie@example.com", time.Now()},
}

copyCount, err := conn.CopyFrom(
    ctx,
    pgx.Identifier{"users"},
    []string{"name", "email", "created_at"},
    pgx.CopyFromRows(rows),
)
if err != nil {
    log.Fatal(err)
}
fmt.Printf("Inserted %d rows via COPY\n", copyCount)
```

---

## Part 3: database/sql Interface

For applications requiring database/sql compatibility.

### With pgx stdlib

```go
import (
    "database/sql"

    _ "github.com/jackc/pgx/v5/stdlib"
)

func main() {
    db, err := sql.Open("pgx", "postgres://app_user:secret@localhost:5432/scratchbird")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    // Configure pool
    db.SetMaxOpenConns(25)
    db.SetMaxIdleConns(5)
    db.SetConnMaxLifetime(time.Hour)

    // Query
    var version string
    err = db.QueryRow("SELECT version()").Scan(&version)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Println(version)
}
```

### With lib/pq

```go
import (
    "database/sql"

    _ "github.com/lib/pq"
)

func main() {
    connStr := "host=localhost port=5432 user=app_user password=secret dbname=scratchbird sslmode=disable"
    db, err := sql.Open("postgres", connStr)
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    db.SetMaxOpenConns(25)
    db.SetMaxIdleConns(5)

    err = db.Ping()
    if err != nil {
        log.Fatal(err)
    }

    fmt.Println("Connected with lib/pq")
}
```

### database/sql CRUD

```go
// Insert
result, err := db.ExecContext(ctx,
    "INSERT INTO users (name, email) VALUES ($1, $2)",
    "Alice", "alice@example.com",
)
if err != nil {
    log.Fatal(err)
}
id, _ := result.LastInsertId() // Note: PostgreSQL doesn't support this

// Better: Use RETURNING with QueryRow
var userID int64
err = db.QueryRowContext(ctx,
    "INSERT INTO users (name, email) VALUES ($1, $2) RETURNING id",
    "Alice", "alice@example.com",
).Scan(&userID)

// Select single row
var user User
err = db.QueryRowContext(ctx,
    "SELECT id, name, email FROM users WHERE id = $1",
    1,
).Scan(&user.ID, &user.Name, &user.Email)

if err == sql.ErrNoRows {
    fmt.Println("Not found")
} else if err != nil {
    log.Fatal(err)
}

// Select multiple rows
rows, err := db.QueryContext(ctx, "SELECT id, name, email FROM users")
if err != nil {
    log.Fatal(err)
}
defer rows.Close()

for rows.Next() {
    var u User
    if err := rows.Scan(&u.ID, &u.Name, &u.Email); err != nil {
        log.Fatal(err)
    }
    fmt.Printf("%d: %s\n", u.ID, u.Name)
}
```

### database/sql Transactions

```go
tx, err := db.BeginTx(ctx, &sql.TxOptions{
    Isolation: sql.LevelSerializable,
    ReadOnly:  false,
})
if err != nil {
    log.Fatal(err)
}
defer tx.Rollback()

_, err = tx.ExecContext(ctx,
    "UPDATE accounts SET balance = balance - $1 WHERE id = $2",
    100.00, 1,
)
if err != nil {
    log.Fatal(err)
}

_, err = tx.ExecContext(ctx,
    "UPDATE accounts SET balance = balance + $1 WHERE id = $2",
    100.00, 2,
)
if err != nil {
    log.Fatal(err)
}

err = tx.Commit()
if err != nil {
    log.Fatal(err)
}
```

---

## Part 4: MySQL Protocol

### go-sql-driver/mysql

```go
import (
    "database/sql"
    "time"

    _ "github.com/go-sql-driver/mysql"
)

func main() {
    // DSN format
    dsn := "app_user:secret@tcp(localhost:3306)/scratchbird?parseTime=true"

    db, err := sql.Open("mysql", dsn)
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    db.SetMaxOpenConns(25)
    db.SetMaxIdleConns(5)
    db.SetConnMaxLifetime(time.Hour)

    err = db.Ping()
    if err != nil {
        log.Fatal(err)
    }

    fmt.Println("Connected via MySQL protocol")
}
```

### MySQL DSN Options

```go
// Full DSN with options
dsn := "app_user:secret@tcp(localhost:3306)/scratchbird?" +
    "parseTime=true&" +           // Parse TIME/DATE into time.Time
    "loc=UTC&" +                  // Timezone
    "timeout=5s&" +               // Connection timeout
    "readTimeout=30s&" +          // Read timeout
    "writeTimeout=30s&" +         // Write timeout
    "charset=utf8mb4&" +          // Character set
    "collation=utf8mb4_unicode_ci&" +
    "tls=skip-verify"             // SSL mode
```

### MySQL CRUD

```go
// Insert with last insert ID
result, err := db.ExecContext(ctx,
    "INSERT INTO users (name, email) VALUES (?, ?)",
    "Bob", "bob@example.com",
)
if err != nil {
    log.Fatal(err)
}
lastID, _ := result.LastInsertId()
fmt.Printf("Inserted ID: %d\n", lastID)

// Note: MySQL uses ? placeholders, not $1, $2
rows, err := db.QueryContext(ctx,
    "SELECT id, name, email FROM users WHERE active = ?",
    true,
)
```

---

## Part 5: Firebird Protocol

### nakagami/firebirdsql

```go
import (
    "database/sql"

    _ "github.com/nakagami/firebirdsql"
)

func main() {
    // DSN format: user:password@host:port/database
    dsn := "SYSDBA:masterkey@localhost:3050/scratchbird"

    db, err := sql.Open("firebirdsql", dsn)
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    err = db.Ping()
    if err != nil {
        log.Fatal(err)
    }

    fmt.Println("Connected via Firebird protocol")
}
```

### Firebird Queries

```go
// Firebird uses FIRST/SKIP for pagination
rows, err := db.QueryContext(ctx,
    "SELECT FIRST 10 SKIP 0 id, name, email FROM users ORDER BY name",
)
if err != nil {
    log.Fatal(err)
}
defer rows.Close()

// UPDATE OR INSERT (upsert)
_, err = db.ExecContext(ctx,
    "UPDATE OR INSERT INTO users (id, name, email) VALUES (?, ?, ?) MATCHING (id)",
    1, "Updated Name", "email@example.com",
)
```

---

## Part 6: GORM Integration

### Setup

```go
import (
    "gorm.io/driver/postgres"
    "gorm.io/gorm"
    "gorm.io/gorm/logger"
)

type User struct {
    ID        uint   `gorm:"primaryKey"`
    Name      string `gorm:"size:100;not null"`
    Email     string `gorm:"size:255;uniqueIndex"`
    Active    bool   `gorm:"default:true"`
    CreatedAt time.Time
    UpdatedAt time.Time
}

type Order struct {
    ID        uint `gorm:"primaryKey"`
    UserID    uint
    User      User `gorm:"foreignKey:UserID"`
    Amount    float64
    Status    string
    CreatedAt time.Time
}

func main() {
    dsn := "host=localhost port=5432 user=app_user password=secret dbname=scratchbird sslmode=disable"

    db, err := gorm.Open(postgres.Open(dsn), &gorm.Config{
        Logger: logger.Default.LogMode(logger.Info),
    })
    if err != nil {
        log.Fatal(err)
    }

    // Auto migrate
    db.AutoMigrate(&User{}, &Order{})
}
```

### GORM CRUD

```go
// Create
user := User{Name: "Alice", Email: "alice@example.com"}
result := db.Create(&user)
if result.Error != nil {
    log.Fatal(result.Error)
}
fmt.Printf("Created user ID: %d\n", user.ID)

// Read
var foundUser User
db.First(&foundUser, 1)                      // By primary key
db.First(&foundUser, "email = ?", "alice@example.com")  // By condition

// Find all
var users []User
db.Find(&users)
db.Where("active = ?", true).Find(&users)

// Read with associations
var userWithOrders User
db.Preload("Orders").First(&userWithOrders, 1)

// Update
db.Model(&user).Update("email", "alice.new@example.com")
db.Model(&user).Updates(User{Name: "Alice Updated", Email: "new@example.com"})
db.Model(&user).Updates(map[string]interface{}{"name": "Alice", "active": false})

// Delete
db.Delete(&user, 1)
db.Where("active = ?", false).Delete(&User{})
```

### GORM Transactions

```go
err := db.Transaction(func(tx *gorm.DB) error {
    // Debit
    if err := tx.Model(&Account{}).Where("id = ?", 1).
        Update("balance", gorm.Expr("balance - ?", 100)).Error; err != nil {
        return err
    }

    // Credit
    if err := tx.Model(&Account{}).Where("id = ?", 2).
        Update("balance", gorm.Expr("balance + ?", 100)).Error; err != nil {
        return err
    }

    return nil // Commit
})

if err != nil {
    log.Fatal(err)
}
```

### GORM Query Builder

```go
// Complex queries
var users []User
db.Where("active = ?", true).
    Where("created_at > ?", time.Now().AddDate(0, 0, -30)).
    Order("name ASC").
    Limit(10).
    Offset(0).
    Find(&users)

// Raw SQL
var result []map[string]interface{}
db.Raw("SELECT id, name FROM users WHERE active = ?", true).Scan(&result)

// Subqueries
subQuery := db.Model(&Order{}).Select("user_id").Where("amount > ?", 100)
db.Where("id IN (?)", subQuery).Find(&users)

// Joins
type UserWithOrderCount struct {
    User
    OrderCount int64
}

var usersWithCounts []UserWithOrderCount
db.Model(&User{}).
    Select("users.*, COUNT(orders.id) as order_count").
    Joins("LEFT JOIN orders ON orders.user_id = users.id").
    Group("users.id").
    Scan(&usersWithCounts)
```

### Connection Pool with GORM

```go
sqlDB, err := db.DB()
if err != nil {
    log.Fatal(err)
}

sqlDB.SetMaxOpenConns(25)
sqlDB.SetMaxIdleConns(5)
sqlDB.SetConnMaxLifetime(time.Hour)
```

---

## Part 7: sqlx Integration

### Setup

```go
import (
    "github.com/jmoiron/sqlx"
    _ "github.com/jackc/pgx/v5/stdlib"
)

type User struct {
    ID        int64     `db:"id"`
    Name      string    `db:"name"`
    Email     string    `db:"email"`
    Active    bool      `db:"active"`
    CreatedAt time.Time `db:"created_at"`
}

func main() {
    db, err := sqlx.Connect("pgx", "postgres://app_user:secret@localhost:5432/scratchbird")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    db.SetMaxOpenConns(25)
    db.SetMaxIdleConns(5)
}
```

### sqlx Operations

```go
// Get single row into struct
var user User
err := db.Get(&user, "SELECT * FROM users WHERE id = $1", 1)
if err != nil {
    log.Fatal(err)
}

// Get multiple rows
var users []User
err = db.Select(&users, "SELECT * FROM users WHERE active = $1", true)
if err != nil {
    log.Fatal(err)
}

// Named queries
user = User{Name: "Dave", Email: "dave@example.com"}
_, err = db.NamedExec(
    "INSERT INTO users (name, email) VALUES (:name, :email)",
    user,
)

// Named query with map
_, err = db.NamedExec(
    "INSERT INTO users (name, email) VALUES (:name, :email)",
    map[string]interface{}{
        "name":  "Eve",
        "email": "eve@example.com",
    },
)

// Batch insert
users = []User{
    {Name: "User 1", Email: "user1@example.com"},
    {Name: "User 2", Email: "user2@example.com"},
}
_, err = db.NamedExec(
    "INSERT INTO users (name, email) VALUES (:name, :email)",
    users,
)
```

### sqlx Transactions

```go
tx, err := db.Beginx()
if err != nil {
    log.Fatal(err)
}
defer tx.Rollback()

var user User
err = tx.Get(&user, "SELECT * FROM users WHERE id = $1 FOR UPDATE", 1)
if err != nil {
    log.Fatal(err)
}

_, err = tx.Exec("UPDATE users SET name = $1 WHERE id = $2", "Updated", 1)
if err != nil {
    log.Fatal(err)
}

err = tx.Commit()
if err != nil {
    log.Fatal(err)
}
```

---

## Part 8: HTTP Service Example

### Basic REST API

```go
package main

import (
    "context"
    "encoding/json"
    "log"
    "net/http"
    "strconv"

    "github.com/jackc/pgx/v5/pgxpool"
)

var pool *pgxpool.Pool

type User struct {
    ID    int64  `json:"id"`
    Name  string `json:"name"`
    Email string `json:"email"`
}

func main() {
    ctx := context.Background()

    var err error
    pool, err = pgxpool.New(ctx, "postgres://app_user:secret@localhost:5432/scratchbird")
    if err != nil {
        log.Fatal(err)
    }
    defer pool.Close()

    http.HandleFunc("GET /users", listUsers)
    http.HandleFunc("GET /users/{id}", getUser)
    http.HandleFunc("POST /users", createUser)
    http.HandleFunc("PUT /users/{id}", updateUser)
    http.HandleFunc("DELETE /users/{id}", deleteUser)

    log.Println("Server starting on :8080")
    log.Fatal(http.ListenAndServe(":8080", nil))
}

func listUsers(w http.ResponseWriter, r *http.Request) {
    rows, err := pool.Query(r.Context(), "SELECT id, name, email FROM users")
    if err != nil {
        http.Error(w, err.Error(), http.StatusInternalServerError)
        return
    }
    defer rows.Close()

    var users []User
    for rows.Next() {
        var u User
        if err := rows.Scan(&u.ID, &u.Name, &u.Email); err != nil {
            http.Error(w, err.Error(), http.StatusInternalServerError)
            return
        }
        users = append(users, u)
    }

    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(users)
}

func getUser(w http.ResponseWriter, r *http.Request) {
    idStr := r.PathValue("id")
    id, err := strconv.ParseInt(idStr, 10, 64)
    if err != nil {
        http.Error(w, "Invalid ID", http.StatusBadRequest)
        return
    }

    var user User
    err = pool.QueryRow(r.Context(),
        "SELECT id, name, email FROM users WHERE id = $1",
        id,
    ).Scan(&user.ID, &user.Name, &user.Email)

    if err != nil {
        http.Error(w, "User not found", http.StatusNotFound)
        return
    }

    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(user)
}

func createUser(w http.ResponseWriter, r *http.Request) {
    var user User
    if err := json.NewDecoder(r.Body).Decode(&user); err != nil {
        http.Error(w, err.Error(), http.StatusBadRequest)
        return
    }

    err := pool.QueryRow(r.Context(),
        "INSERT INTO users (name, email) VALUES ($1, $2) RETURNING id",
        user.Name, user.Email,
    ).Scan(&user.ID)

    if err != nil {
        http.Error(w, err.Error(), http.StatusInternalServerError)
        return
    }

    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(http.StatusCreated)
    json.NewEncoder(w).Encode(user)
}

func updateUser(w http.ResponseWriter, r *http.Request) {
    idStr := r.PathValue("id")
    id, err := strconv.ParseInt(idStr, 10, 64)
    if err != nil {
        http.Error(w, "Invalid ID", http.StatusBadRequest)
        return
    }

    var user User
    if err := json.NewDecoder(r.Body).Decode(&user); err != nil {
        http.Error(w, err.Error(), http.StatusBadRequest)
        return
    }

    tag, err := pool.Exec(r.Context(),
        "UPDATE users SET name = $1, email = $2 WHERE id = $3",
        user.Name, user.Email, id,
    )
    if err != nil {
        http.Error(w, err.Error(), http.StatusInternalServerError)
        return
    }

    if tag.RowsAffected() == 0 {
        http.Error(w, "User not found", http.StatusNotFound)
        return
    }

    user.ID = id
    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(user)
}

func deleteUser(w http.ResponseWriter, r *http.Request) {
    idStr := r.PathValue("id")
    id, err := strconv.ParseInt(idStr, 10, 64)
    if err != nil {
        http.Error(w, "Invalid ID", http.StatusBadRequest)
        return
    }

    tag, err := pool.Exec(r.Context(), "DELETE FROM users WHERE id = $1", id)
    if err != nil {
        http.Error(w, err.Error(), http.StatusInternalServerError)
        return
    }

    if tag.RowsAffected() == 0 {
        http.Error(w, "User not found", http.StatusNotFound)
        return
    }

    w.WriteHeader(http.StatusNoContent)
}
```

---

## Part 9: Error Handling

### pgx Errors

```go
import (
    "errors"

    "github.com/jackc/pgx/v5"
    "github.com/jackc/pgx/v5/pgconn"
)

func handleError(err error) {
    if err == nil {
        return
    }

    // No rows found
    if errors.Is(err, pgx.ErrNoRows) {
        fmt.Println("No rows found")
        return
    }

    // PostgreSQL error
    var pgErr *pgconn.PgError
    if errors.As(err, &pgErr) {
        switch pgErr.Code {
        case "23505": // unique_violation
            fmt.Printf("Duplicate key: %s\n", pgErr.ConstraintName)
        case "23503": // foreign_key_violation
            fmt.Printf("Foreign key violation: %s\n", pgErr.ConstraintName)
        case "23502": // not_null_violation
            fmt.Printf("NULL value in column: %s\n", pgErr.ColumnName)
        case "42P01": // undefined_table
            fmt.Printf("Table does not exist: %s\n", pgErr.TableName)
        default:
            fmt.Printf("PostgreSQL error [%s]: %s\n", pgErr.Code, pgErr.Message)
        }
        return
    }

    // Connection error
    fmt.Printf("Unknown error: %v\n", err)
}
```

### Common PostgreSQL Error Codes

| Code | Name | Description |
|------|------|-------------|
| 23505 | unique_violation | Duplicate key |
| 23503 | foreign_key_violation | FK constraint failed |
| 23502 | not_null_violation | NULL in NOT NULL column |
| 23514 | check_violation | CHECK constraint failed |
| 42P01 | undefined_table | Table doesn't exist |
| 42703 | undefined_column | Column doesn't exist |
| 08006 | connection_failure | Connection lost |
| 40001 | serialization_failure | Transaction conflict |
| 40P01 | deadlock_detected | Deadlock detected |

### Retry Logic

```go
import (
    "time"
)

func withRetry[T any](fn func() (T, error), maxRetries int) (T, error) {
    var result T
    var err error

    for i := 0; i < maxRetries; i++ {
        result, err = fn()
        if err == nil {
            return result, nil
        }

        if !isRetryable(err) {
            return result, err
        }

        backoff := time.Duration(1<<uint(i)) * time.Second
        time.Sleep(backoff)
    }

    return result, err
}

func isRetryable(err error) bool {
    var pgErr *pgconn.PgError
    if errors.As(err, &pgErr) {
        switch pgErr.Code {
        case "40001", "40P01": // serialization/deadlock
            return true
        case "08006", "08001", "08004": // connection errors
            return true
        case "57P01": // admin shutdown
            return true
        }
    }
    return false
}

// Usage
user, err := withRetry(func() (User, error) {
    var u User
    err := pool.QueryRow(ctx, "SELECT id, name FROM users WHERE id = $1", 1).
        Scan(&u.ID, &u.Name)
    return u, err
}, 3)
```

---

## Part 10: Special Data Types

### JSON/JSONB

```go
import "encoding/json"

type Settings struct {
    Theme    string `json:"theme"`
    Language string `json:"language"`
    Notify   bool   `json:"notify"`
}

// Insert JSON
settings := Settings{Theme: "dark", Language: "en", Notify: true}
settingsJSON, _ := json.Marshal(settings)

_, err := conn.Exec(ctx,
    "INSERT INTO user_settings (user_id, settings) VALUES ($1, $2)",
    1, settingsJSON,
)

// Query JSON
var settingsBytes []byte
err = conn.QueryRow(ctx,
    "SELECT settings FROM user_settings WHERE user_id = $1",
    1,
).Scan(&settingsBytes)

var loadedSettings Settings
json.Unmarshal(settingsBytes, &loadedSettings)

// Query JSON fields
var theme string
err = conn.QueryRow(ctx,
    "SELECT settings->>'theme' FROM user_settings WHERE user_id = $1",
    1,
).Scan(&theme)
```

### Arrays

```go
// Insert array
tags := []string{"go", "database", "scratchbird"}
_, err := conn.Exec(ctx,
    "INSERT INTO articles (title, tags) VALUES ($1, $2)",
    "Getting Started with Go", tags,
)

// Query array
var loadedTags []string
err = conn.QueryRow(ctx,
    "SELECT tags FROM articles WHERE id = $1",
    1,
).Scan(&loadedTags)

// Array contains
rows, err := conn.Query(ctx,
    "SELECT title FROM articles WHERE $1 = ANY(tags)",
    "go",
)
```

### UUID

```go
import "github.com/google/uuid"

// Insert UUID
id := uuid.New()
_, err := conn.Exec(ctx,
    "INSERT INTO sessions (id, user_id) VALUES ($1, $2)",
    id, 1,
)

// Query by UUID
var sessionID uuid.UUID
var userID int64
err = conn.QueryRow(ctx,
    "SELECT id, user_id FROM sessions WHERE id = $1",
    id,
).Scan(&sessionID, &userID)
```

### Date/Time

```go
// Insert timestamps
_, err := conn.Exec(ctx,
    "INSERT INTO events (name, event_date, start_time, created_at) VALUES ($1, $2, $3, $4)",
    "Conference",
    time.Date(2026, 6, 15, 0, 0, 0, 0, time.UTC),  // date
    time.Date(0, 1, 1, 9, 0, 0, 0, time.UTC),       // time
    time.Now().UTC(),                               // timestamp
)

// Query timestamps
var eventDate, startTime, createdAt time.Time
err = conn.QueryRow(ctx,
    "SELECT event_date, start_time, created_at FROM events WHERE id = $1",
    1,
).Scan(&eventDate, &startTime, &createdAt)
```

### Vectors (HNSW/Vector Search)

```go
import "github.com/pgvector/pgvector-go"

// Register vector type
config, _ := pgx.ParseConfig(connString)
config.AfterConnect = func(ctx context.Context, conn *pgx.Conn) error {
    return pgvector.RegisterTypes(ctx, conn)
}

// Insert vector
embedding := pgvector.NewVector([]float32{0.1, 0.2, 0.3})
_, err := conn.Exec(ctx,
    "INSERT INTO documents (title, embedding) VALUES ($1, $2)",
    "Sample Document", embedding,
)

// Vector similarity search
queryVec := pgvector.NewVector([]float32{0.15, 0.25, 0.35})
rows, err := conn.Query(ctx,
    `SELECT title, embedding <-> $1 AS distance
     FROM documents
     ORDER BY embedding <-> $1
     LIMIT 5`,
    queryVec,
)

for rows.Next() {
    var title string
    var distance float64
    rows.Scan(&title, &distance)
    fmt.Printf("%s: distance=%.4f\n", title, distance)
}
```

---

## Part 11: Common Issues

### Connection Pool Exhaustion

**Symptoms:**
- "cannot acquire connection" errors
- Timeouts waiting for connections

**Solutions:**
```go
// 1. Increase pool size
config.MaxConns = 50

// 2. Reduce connection lifetime
config.MaxConnLifetime = 30 * time.Minute

// 3. Monitor pool stats
stat := pool.Stat()
if stat.AcquiredConns() > int32(float64(stat.MaxConns())*0.8) {
    log.Println("Warning: Pool near capacity")
}

// 4. Use context with timeout
ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
defer cancel()
conn, err := pool.Acquire(ctx)
```

### Context Cancellation

```go
// Always use context for cancellation
ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
defer cancel()

rows, err := pool.Query(ctx, "SELECT * FROM large_table")
if err != nil {
    if errors.Is(err, context.DeadlineExceeded) {
        log.Println("Query timed out")
    }
    if errors.Is(err, context.Canceled) {
        log.Println("Query was canceled")
    }
    return
}
```

### Memory with Large Results

```go
// Stream results instead of loading all
rows, err := pool.Query(ctx, "SELECT * FROM large_table")
if err != nil {
    log.Fatal(err)
}
defer rows.Close()

// Process one row at a time
for rows.Next() {
    var item Item
    rows.Scan(&item.ID, &item.Data)
    processItem(item) // Process immediately
}

// Or use cursor
_, err = pool.Exec(ctx, "DECLARE my_cursor CURSOR FOR SELECT * FROM large_table")
// Fetch in batches
rows, _ = pool.Query(ctx, "FETCH 100 FROM my_cursor")
```

### Prepared Statement Cache

```go
// pgx caches prepared statements automatically
// To disable or configure:
config.DefaultQueryExecMode = pgx.QueryExecModeSimpleProtocol

// Or clear cache
conn.DeallocateAll(ctx)
```

---

## Quick Reference

### Connection String Templates

**pgx:**
```
postgres://user:pass@localhost:5432/scratchbird?sslmode=disable&pool_max_conns=25
```

**lib/pq:**
```
host=localhost port=5432 user=app_user password=secret dbname=scratchbird sslmode=disable
```

**MySQL:**
```
user:pass@tcp(localhost:3306)/scratchbird?parseTime=true&timeout=5s
```

**Firebird:**
```
SYSDBA:masterkey@localhost:3050/scratchbird
```

### Go Modules

| Module | Protocol | Purpose |
|--------|----------|---------|
| github.com/jackc/pgx/v5 | PostgreSQL | Emulation driver (recommended) |
| github.com/lib/pq | PostgreSQL | database/sql compatible |
| github.com/go-sql-driver/mysql | MySQL | MySQL driver |
| github.com/nakagami/firebirdsql | Firebird | Firebird driver |
| gorm.io/gorm | Any | Full-featured ORM |
| github.com/jmoiron/sqlx | Any | Extended database/sql |
| github.com/pgvector/pgvector-go | PostgreSQL | Vector search |

### Common Operations

| Operation | pgx | database/sql |
|-----------|-----|--------------|
| Connect | `pgx.Connect(ctx, dsn)` | `sql.Open("pgx", dsn)` |
| Query single | `conn.QueryRow(ctx, sql, args...)` | `db.QueryRowContext(ctx, sql, args...)` |
| Query multiple | `conn.Query(ctx, sql, args...)` | `db.QueryContext(ctx, sql, args...)` |
| Execute | `conn.Exec(ctx, sql, args...)` | `db.ExecContext(ctx, sql, args...)` |
| Begin tx | `conn.Begin(ctx)` | `db.BeginTx(ctx, opts)` |
| Commit | `tx.Commit(ctx)` | `tx.Commit()` |
| Rollback | `tx.Rollback(ctx)` | `tx.Rollback()` |

---

## See Also

- [Driver Comparison](Driver-Comparison.md) - Compare all available drivers
- [Connection Guide](../getting-started/first-connection.md) - First connection walkthrough
- [Performance Tuning](../user-guides/Performance-Tuning.md) - Optimize database performance
- [Vector Search](../user-guides/Vector-Search.md) - AI and vector search guide
