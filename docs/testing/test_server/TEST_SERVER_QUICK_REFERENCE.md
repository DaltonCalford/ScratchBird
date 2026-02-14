# ScratchBird Local Test Server - Quick Reference

**One-page reference for the localhost ScratchBird test server.**

---

## Connection Parameters

```
┌─────────────────────────────────────────────────────────────┐
│  SCRATCHBIRD LOCAL TEST SERVER                              │
├─────────────────────────────────────────────────────────────┤
│  Host:     127.0.0.1 (localhost)                           │
│  Port:     13092                                            │
│  Database: testdb                                           │
│  TLS:      Required (TLS 1.3)                               │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  USER ACCOUNTS FOR SECURITY TESTING                         │
├─────────────────────────────────────────────────────────────┤
│  SYSARCH  / SysArch2026!   (Full DDL/DML access)           │
│  TESTUSER / TestUser2026!  (DML only - SELECT/INSERT/      │
│                             UPDATE/DELETE)                  │
└─────────────────────────────────────────────────────────────┘
```

---

## Quick Connect Examples

### Native C Client
```c
// SYSARCH - Full access
sb_connection_t* conn_sysarch = sb_connect(
    "127.0.0.1", 13092, "testdb",
    "SYSARCH", "SysArch2026!", SB_TLS_REQUIRE
);

// TESTUSER - Limited access
sb_connection_t* conn_testuser = sb_connect(
    "127.0.0.1", 13092, "testdb",
    "TESTUSER", "TestUser2026!", SB_TLS_REQUIRE
);
```

### Python
```python
import scratchbird

# SYSARCH - Full DDL/DML access
conn_admin = scratchbird.connect(
    host="127.0.0.1", port=13092, database="testdb",
    user="SYSARCH", password="SysArch2026!", ssl=True
)

# TESTUSER - Application testing (DML only)
conn_app = scratchbird.connect(
    host="127.0.0.1", port=13092, database="testdb",
    user="TESTUSER", password="TestUser2026!", ssl=True
)

# Test security - This should work with SYSARCH
cursor = conn_admin.cursor()
cursor.execute("CREATE TABLE test (id INT PRIMARY KEY)")

# This should fail with TESTUSER (no DDL permissions)
try:
    cursor = conn_app.cursor()
    cursor.execute("CREATE TABLE test2 (id INT)")
except scratchbird.InsufficientPrivilege:
    print("Security test passed: TESTUSER cannot create tables")
```

### Go
```go
// SYSARCH - Full access
connAdmin, err := scratchbird.Connect(
    "scratchbird://SYSARCH:SysArch2026!@127.0.0.1:13092/testdb?ssl=require")

// TESTUSER - Limited access  
connApp, err := scratchbird.Connect(
    "scratchbird://TESTUSER:TestUser2026!@127.0.0.1:13092/testdb?ssl=require")
```

### Java (JDBC)
```java
String url = "jdbc:scratchbird://127.0.0.1:13092/testdb?sslmode=require";

// SYSARCH connection
Connection connAdmin = DriverManager.getConnection(url, "SYSARCH", "SysArch2026!");

// TESTUSER connection
Connection connApp = DriverManager.getConnection(url, "TESTUSER", "TestUser2026!");
```

### Node.js
```javascript
const { Client } = require('scratchbird');

// SYSARCH client
const clientAdmin = new Client({
    host: '127.0.0.1', port: 13092, database: 'testdb',
    user: 'SYSARCH', password: 'SysArch2026!',
    ssl: { rejectUnauthorized: false }  // For self-signed certs
});

// TESTUSER client
const clientApp = new Client({
    host: '127.0.0.1', port: 13092, database: 'testdb',
    user: 'TESTUSER', password: 'TestUser2026!',
    ssl: { rejectUnauthorized: false }
});
```

### ODBC
```
Driver={ScratchBird ODBC Driver};
Server=127.0.0.1;
Port=13092;
Database=testdb;
Uid=SYSARCH;
Pwd=SysArch2026!;
SSLMode=require;
```

### Ruby
```ruby
require 'scratchbird'

# SYSARCH
conn_admin = ScratchBird::Connection.new(
  host: '127.0.0.1', port: 13092, database: 'testdb',
  user: 'SYSARCH', password: 'SysArch2026!', sslmode: 'require'
)

# TESTUSER
conn_app = ScratchBird::Connection.new(
  host: '127.0.0.1', port: 13092, database: 'testdb',
  user: 'TESTUSER', password: 'TestUser2026!', sslmode: 'require'
)
```

### Rust
```rust
use scratchbird::{Client, Config};

// SYSARCH
let config_admin = Config::new()
    .host("127.0.0.1").port(13092).database("testdb")
    .user("SYSARCH").password("SysArch2026!")
    .ssl_mode(SslMode::Require);

// TESTUSER
let config_app = Config::new()
    .host("127.0.0.1").port(13092).database("testdb")
    .user("TESTUSER").password("TestUser2026!")
    .ssl_mode(SslMode::Require);
```

### PHP
```php
// SYSARCH
$conn_admin = scratchbird_connect(
    "host=127.0.0.1 port=13092 dbname=testdb " .
    "user=SYSARCH password=SysArch2026! sslmode=require"
);

// TESTUSER
$conn_app = scratchbird_connect(
    "host=127.0.0.1 port=13092 dbname=testdb " .
    "user=TESTUSER password=TestUser2026! sslmode=require"
);
```

### .NET
```csharp
var connString = "Host=127.0.0.1;Port=13092;Database=testdb;" +
                 "SSL Mode=Require";

// SYSARCH
await using var connAdmin = new ScratchBirdConnection(
    connString + "Username=SYSARCH;Password=SysArch2026!");

// TESTUSER
await using var connApp = new ScratchBirdConnection(
    connString + "Username=TESTUSER;Password=TestUser2026!");
```

---

## Test Schema

### Tables

```sql
-- Simple users table
CREATE TABLE test_schema.users (
    id INTEGER PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    username VARCHAR(50) NOT NULL,
    email VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    active BOOLEAN DEFAULT TRUE
);

-- Data types test table
CREATE TABLE test_schema.data_types (
    id INTEGER PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    small_int SMALLINT,
    normal_int INTEGER,
    big_int BIGINT,
    real_num REAL,
    double_num DOUBLE PRECISION,
    decimal_num DECIMAL(18,4),
    var_char VARCHAR(255),
    text_field TEXT,
    json_data JSON,
    jsonb_data JSONB,
    uuid_field UUID
);
```

### Sample Queries

```sql
-- Basic SELECT
SELECT * FROM test_schema.users;

-- Filtered
SELECT * FROM test_schema.users WHERE active = TRUE;

-- Join (self-join example)
SELECT u1.username, u2.username as referred_by
FROM test_schema.users u1
LEFT JOIN test_schema.users u2 ON u1.id = u2.id;

-- Insert
INSERT INTO test_schema.users (username, email, active)
VALUES ('testuser', 'test@example.com', TRUE);

-- Update
UPDATE test_schema.users 
SET active = FALSE 
WHERE username = 'testuser';

-- Delete
DELETE FROM test_schema.users 
WHERE username = 'testuser';

-- JSON operations
SELECT json_data->>'key' FROM test_schema.data_types;
```

---

## Server Setup (Local)

### One-Line Setup
```bash
curl -fsSL https://raw.githubusercontent.com/DaltonCalford/ScratchBird/main/scripts/setup-test-server.sh | sudo bash -s localhost
```

### Manual Setup
```bash
# 1. Clone and build
git clone https://github.com/DaltonCalford/ScratchBird.git
cd ScratchBird
cmake -S . -B build && cmake --build build

# 2. Run setup script (binds to 127.0.0.1)
sudo ./scripts/setup-test-server.sh localhost

# 3. Test SYSARCH connection (full access)
./build/bin/sb_isql \
    --host=127.0.0.1 --port=13092 \
    --user=SYSARCH --password='SysArch2026!' \
    --query="CREATE TABLE security_test (id INT);"

# 4. Test TESTUSER connection (DML only)
./build/bin/sb_isql \
    --host=127.0.0.1 --port=13092 \
    --user=TESTUSER --password='TestUser2026!' \
    --query="INSERT INTO security_test VALUES (1);"

# 5. Verify security - This should fail:
./build/bin/sb_isql \
    --host=127.0.0.1 --port=13092 \
    --user=TESTUSER --password='TestUser2026!' \
    --query="DROP TABLE security_test;"  # Should fail!
```

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Connection refused | Check firewall: `sudo ufw allow 13092/tcp` |
| TLS/SSL error | Use `--sslmode=allow` for testing (not production) |
| Authentication failed | Verify username/password |
| Timeout | Check server is running: `sudo systemctl status scratchbird-test` |
| Slow queries | Check logs: `sudo tail -f /var/log/scratchbird/testdb.log` |

---

## Service Commands

```bash
# Status
sudo systemctl status scratchbird-test

# Restart
sudo systemctl restart scratchbird-test

# View logs
sudo tail -f /var/log/scratchbird/testdb.log

# Stop
sudo systemctl stop scratchbird-test

# Disable auto-start
sudo systemctl disable scratchbird-test
```

---

## Security Testing Guide

### User Privilege Matrix

| Operation | SYSARCH | TESTUSER |
|-----------|---------|----------|
| SELECT | ✅ Yes | ✅ Yes |
| INSERT | ✅ Yes | ✅ Yes |
| UPDATE | ✅ Yes | ✅ Yes |
| DELETE | ✅ Yes | ✅ Yes |
| CREATE TABLE | ✅ Yes | ❌ No |
| DROP TABLE | ✅ Yes | ❌ No |
| CREATE INDEX | ✅ Yes | ❌ No |
| ALTER TABLE | ✅ Yes | ❌ No |
| GRANT | ✅ Yes | ❌ No |
| VACUUM | ✅ Yes | ❌ No |

### Testing Security

```python
# Test 1: Verify TESTUSER cannot create tables
import scratchbird

conn = scratchbird.connect(
    host="127.0.0.1", port=13092,
    user="TESTUSER", password="TestUser2026!"
)

try:
    conn.execute("CREATE TABLE hack_attempt (data TEXT)")
    print("SECURITY FAIL: TESTUSER should not create tables!")
except scratchbird.InsufficientPrivilege:
    print("SECURITY PASS: TESTUSER correctly blocked from DDL")

# Test 2: Verify SYSARCH can perform DDL
conn_admin = scratchbird.connect(
    host="127.0.0.1", port=13092,
    user="SYSARCH", password="SysArch2026!"
)
conn_admin.execute("CREATE TABLE legit_table (id INT)")  # Should succeed
print("SYSARCH can perform DDL as expected")
```

---

## Quick Reference Card

```
╔══════════════════════════════════════════════════════════════╗
║        SCRATCHBIRD LOCAL TEST SERVER - QUICK REF            ║
╠══════════════════════════════════════════════════════════════╣
║ Host:     127.0.0.1 (localhost)                            ║
║ Port:     13092                                             ║
║ Database: testdb                                            ║
║ TLS:      Required (TLS 1.3)                                ║
╠══════════════════════════════════════════════════════════════╣
║  SYSARCH  / SysArch2026!     [Full DDL/DML Access]         ║
║  TESTUSER / TestUser2026!    [DML Only: SELECT/INSERT/     ║
║                                UPDATE/DELETE]               ║
╠══════════════════════════════════════════════════════════════╣
║ Connection Strings:                                         ║
║ scratchbird://SYSARCH:SysArch2026!@127.0.0.1:13092/testdb  ║
║ scratchbird://TESTUSER:TestUser2026!@127.0.0.1:13092/testdb║
╠══════════════════════════════════════════════════════════════╣
║ TLS: Required (TLS 1.3)                                     ║
║ Page Size: 16KB                                             ║
║ Encoding: UTF8                                              ║
║ Bind: 127.0.0.1 (local only)                                ║
╚══════════════════════════════════════════════════════════════╝
```

---

## Links

- **Main Repository:** https://github.com/DaltonCalford/ScratchBird
- **Driver Repository:** https://github.com/DaltonCalford/ScratchBird-driver
- **Full Setup Guide:** `PUBLIC_TEST_SERVER_SETUP.md`
- **Issue Tracker:** https://github.com/DaltonCalford/ScratchBird/issues

---

**Last Updated:** 2026-02-06
