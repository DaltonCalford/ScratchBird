# Phase 23: Client Libraries

## Objective
Implement client libraries for multiple languages.

## Prerequisites
- Phase 22 complete (performance tools)

## Tasks

### 23.1 C/C++ Client Library
```cpp
class ScratchBirdClient {
    Connection connect(string host, int port, string user, string password);
    Result execute(string query);
    PreparedStatement prepare(string query);
    void disconnect();
};
```

### 23.2 Python Client
```python
import scratchbird

conn = scratchbird.connect(
    host="localhost",
    port=5432,
    user="user",
    password="pass"
)
cursor = conn.cursor()
cursor.execute("SELECT * FROM users")
rows = cursor.fetchall()
```

### 23.3 Connection Pooling
- Reuse connections
- Thread-safe pool
- Automatic reconnection

### 23.4 Async Support
- Non-blocking operations
- Callback/promise based
- Event loop integration

### 23.5 ORM Integration
- SQLAlchemy dialect (Python)
- Basic ORM mapping
- Migration support

## Files to Create
- `client/cpp/include/scratchbird_client.h`
- `client/cpp/src/client.cpp`
- `client/python/scratchbird/__init__.py`
- `client/python/scratchbird/connection.py`

## Validation Tests
```cpp
// C++ client
ScratchBirdClient client;
auto conn = client.connect("localhost", 5432, "user", "pass");
auto result = client.execute("SELECT 1");
assert(result.rows[0][0] == "1");

// Python client
import scratchbird
conn = scratchbird.connect(...)
cursor = conn.cursor()
cursor.execute("INSERT INTO test VALUES (%s, %s)", (1, "data"))
conn.commit()

// Connection pool
auto pool = ConnectionPool(max_size=10);
auto conn1 = pool.acquire();
auto conn2 = pool.acquire();
// ... use connections
pool.release(conn1);
```

## Exit Criteria
- Clients connect and execute queries
- Prepared statements work
- Connection pooling efficient
- Python client pip-installable