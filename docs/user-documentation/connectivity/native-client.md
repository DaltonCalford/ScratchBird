# Native Client Library

Connect to ScratchBird using the native client library.

[Back to Connectivity Index](index.md) | [Back to Documentation Index](../index.md)

---

## Overview

The ScratchBird native client library (`libscratchbird_client`) provides direct access to ScratchBird using the native wire protocol. It offers the best performance and full feature support.

**Default Port:** 3092

---

## Installation

### Package Installation

The client library is included with ScratchBird packages:

```bash
# Debian/Ubuntu
sudo apt install libscratchbird-client

# RHEL/Fedora
sudo dnf install scratchbird-client-libs
```

### From Source

```bash
cd /path/to/scratchbird
mkdir build && cd build
cmake .. -DBUILD_CLIENT=ON
make -j8
sudo make install
```

---

## C API

### Header

```c
#include <scratchbird/client/connection.h>
```

### Connection

```c
#include <scratchbird/client/connection.h>

int main() {
    sb_error err;
    sb_connection *conn = sb_connect("sb://admin:secret@localhost:3092/mydb", &err);

    if (!conn) {
        fprintf(stderr, "Connection failed: %s\n", err.message);
        return 1;
    }

    // Use connection...

    sb_close(conn);
    return 0;
}
```

### Query Execution

```c
// Simple query
sb_result *result = sb_execute(conn, "SELECT * FROM users", &err);
if (!result) {
    fprintf(stderr, "Query failed: %s\n", err.message);
    return 1;
}

// Iterate results
sb_row row;
while (sb_fetch(result, &row) == SB_OK) {
    printf("ID: %ld, Name: %s\n",
           sb_get_int64(&row, 0),
           sb_get_string(&row, 1));
}

sb_free_result(result);
```

### Prepared Statements

```c
// Prepare
sb_prepared *stmt = sb_prepare(conn,
    "SELECT * FROM users WHERE id = $1 AND active = $2", &err);

// Execute with parameters
sb_params params;
sb_params_init(&params);
sb_params_add_int64(&params, 123);
sb_params_add_bool(&params, true);

sb_result *result = sb_execute_prepared(stmt, &params, &err);

// Cleanup
sb_params_free(&params);
sb_free_prepared(stmt);
```

### Transactions

```c
// Begin transaction
sb_begin(conn, &err);

// Execute statements
sb_execute(conn, "UPDATE accounts SET balance = balance - 100 WHERE id = 1", &err);
sb_execute(conn, "UPDATE accounts SET balance = balance + 100 WHERE id = 2", &err);

// Commit or rollback
if (success) {
    sb_commit(conn, &err);
} else {
    sb_rollback(conn, &err);
}
```

---

## C++ API

### Header

```cpp
#include <scratchbird/client/connection.h>
```

### Connection

```cpp
#include <scratchbird/client/connection.h>

int main() {
    try {
        scratchbird::client::Connection conn("sb://admin:secret@localhost:3092/mydb");

        auto result = conn.execute("SELECT * FROM users");

        for (const auto& row : result) {
            std::cout << "ID: " << row.get<int64_t>(0)
                      << ", Name: " << row.get<std::string>(1) << "\n";
        }

    } catch (const scratchbird::Exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

### Prepared Statements

```cpp
auto stmt = conn.prepare("SELECT * FROM users WHERE id = $1");
auto result = stmt.execute(123);

for (const auto& row : result) {
    std::cout << row.get<std::string>("name") << "\n";
}
```

### Transactions

```cpp
{
    auto tx = conn.begin();

    conn.execute("UPDATE accounts SET balance = balance - 100 WHERE id = 1");
    conn.execute("UPDATE accounts SET balance = balance + 100 WHERE id = 2");

    tx.commit();  // or tx.rollback()
}
// Transaction auto-rolls back if not committed
```

---

## Connection Strings

### Network Connection

```
sb://user:password@host:port/database
sb://admin:secret@localhost:3092/mydb
sb://admin@db.example.com:3092/production?ssl=true
```

### Embedded Mode

```
embedded:/path/to/database.sbdb
embedded:/var/lib/scratchbird/mydb.sbdb
```

### Connection Options

| Option | Description |
|--------|-------------|
| `ssl=true` | Enable SSL/TLS |
| `connect_timeout=10` | Connection timeout (seconds) |
| `statement_timeout=30` | Query timeout (seconds) |
| `application_name=myapp` | Application identifier |

Example:
```
sb://admin:secret@localhost:3092/mydb?ssl=true&connect_timeout=10
```

---

## Linking

### CMake

```cmake
find_package(ScratchBird REQUIRED)
target_link_libraries(myapp scratchbird_client)
```

### pkg-config

```bash
# Compile flags
pkg-config --cflags scratchbird-client

# Link flags
pkg-config --libs scratchbird-client
```

### Manual

```bash
gcc myapp.c -lscratchbird_client -o myapp
```

---

## Error Handling

### C API

```c
sb_error err;
sb_result *result = sb_execute(conn, sql, &err);

if (!result) {
    fprintf(stderr, "Error %d: %s\n", err.code, err.message);
    fprintf(stderr, "SQL State: %s\n", err.sqlstate);
}
```

### C++ API

```cpp
try {
    auto result = conn.execute(sql);
} catch (const scratchbird::ConnectionError& e) {
    std::cerr << "Connection error: " << e.what() << "\n";
} catch (const scratchbird::QueryError& e) {
    std::cerr << "Query error: " << e.what() << "\n";
    std::cerr << "SQL State: " << e.sqlstate() << "\n";
}
```

---

## Data Types

### Type Mapping

| ScratchBird | C Type | C++ Type |
|-------------|--------|----------|
| INTEGER | `int32_t` | `int32_t` |
| BIGINT | `int64_t` | `int64_t` |
| REAL | `float` | `float` |
| DOUBLE | `double` | `double` |
| TEXT | `const char*` | `std::string` |
| BOOLEAN | `int` (0/1) | `bool` |
| TIMESTAMP | `sb_timestamp` | `std::chrono::time_point` |
| UUID | `sb_uuid` | `std::array<uint8_t, 16>` |
| JSON | `const char*` | `std::string` |
| BLOB | `sb_blob` | `std::vector<uint8_t>` |

### Getting Values

```c
// C API
int64_t id = sb_get_int64(&row, 0);
const char *name = sb_get_string(&row, 1);
bool active = sb_get_bool(&row, 2);

// Check for NULL
if (sb_is_null(&row, 3)) {
    // Handle NULL
}
```

```cpp
// C++ API
auto id = row.get<int64_t>(0);
auto name = row.get<std::string>("name");
auto active = row.get<bool>(2);

// Optional for nullable
auto email = row.get_optional<std::string>("email");
if (email) {
    std::cout << *email << "\n";
}
```

---

## Connection Pooling

### C++ Connection Pool

```cpp
#include <scratchbird/client/pool.h>

scratchbird::client::Pool pool(
    "sb://admin:secret@localhost:3092/mydb",
    {
        .min_connections = 5,
        .max_connections = 20,
        .idle_timeout = std::chrono::minutes(5)
    }
);

{
    auto conn = pool.acquire();
    auto result = conn->execute("SELECT * FROM users");
    // Connection returned to pool when conn goes out of scope
}
```

---

## Embedded Mode

Run ScratchBird within your application without a separate server:

```cpp
#include <scratchbird/client/embedded.h>

int main() {
    // Open embedded database
    scratchbird::client::Connection conn("embedded:/data/myapp.sbdb");

    // Use like normal connection
    conn.execute("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT)");
    conn.execute("INSERT INTO users (name) VALUES ('Alice')");

    auto result = conn.execute("SELECT * FROM users");
    // ...

    return 0;
}
```

---

## Thread Safety

- Connections are **not** thread-safe
- Use connection pools for multi-threaded applications
- Prepared statements are bound to their connection

```cpp
// Thread-safe pattern
scratchbird::client::Pool pool(connection_string);

void worker_thread() {
    auto conn = pool.acquire();  // Thread-safe
    // Use conn exclusively in this thread
}
```

---

## Performance Tips

1. **Use prepared statements** for repeated queries
2. **Use connection pooling** for concurrent access
3. **Batch operations** with transactions
4. **Use embedded mode** when no network needed
5. **Set appropriate timeouts**

---

## Building Examples

### Simple Example

```c
// example.c
#include <scratchbird/client/connection.h>
#include <stdio.h>

int main() {
    sb_error err;
    sb_connection *conn = sb_connect("sb://admin:secret@localhost:3092/mydb", &err);

    if (!conn) {
        fprintf(stderr, "Failed: %s\n", err.message);
        return 1;
    }

    sb_result *result = sb_execute(conn, "SELECT version()", &err);
    if (result) {
        sb_row row;
        if (sb_fetch(result, &row) == SB_OK) {
            printf("Version: %s\n", sb_get_string(&row, 0));
        }
        sb_free_result(result);
    }

    sb_close(conn);
    return 0;
}
```

Compile:
```bash
gcc example.c -lscratchbird_client -o example
```

---

## See Also

- [sb_isql](../tools/sb-isql.md)
- [PostgreSQL Clients](postgresql-clients.md)
- [Performance Tuning](../admin/performance-tuning.md)
