# ScratchBird Test Server - Quick Reference

**One-page reference for the public ScratchBird test server.**

---

## Connection Parameters

```
┌─────────────────────────────────────────────────────────────┐
│  SCRATCHBIRD TEST SERVER                                    │
├─────────────────────────────────────────────────────────────┤
│  Host:     scratchbird-test.daltoncalford.dev              │
│  Port:     13092                                            │
│  Database: testdb                                           │
│  Username: testuser                                         │
│  Password: SbTest2026!Alpha                                 │
│  TLS:      Required (TLS 1.3)                               │
└─────────────────────────────────────────────────────────────┘
```

---

## Quick Connect Examples

### Native C Client
```c
sb_connection_t* conn = sb_connect(
    "scratchbird-test.daltoncalford.dev",
    13092,
    "testdb",
    "testuser",
    "SbTest2026!Alpha",
    SB_TLS_REQUIRE
);
```

### Python
```python
import scratchbird

conn = scratchbird.connect(
    host="scratchbird-test.daltoncalford.dev",
    port=13092,
    database="testdb",
    user="testuser",
    password="SbTest2026!Alpha",
    ssl=True
)

cursor = conn.cursor()
cursor.execute("SELECT * FROM test_schema.users")
for row in cursor:
    print(row)
```

### Go
```go
conn, err := scratchbird.Connect(
    "scratchbird://testuser:SbTest2026!Alpha@" +
    "scratchbird-test.daltoncalford.dev:13092/testdb?ssl=require")
```

### Java (JDBC)
```java
String url = "jdbc:scratchbird://scratchbird-test.daltoncalford.dev:13092/testdb" +
             "?sslmode=require";
Connection conn = DriverManager.getConnection(url, "testuser", "SbTest2026!Alpha");
```

### Node.js
```javascript
const { Client } = require('scratchbird');

const client = new Client({
    host: 'scratchbird-test.daltoncalford.dev',
    port: 13092,
    database: 'testdb',
    user: 'testuser',
    password: 'SbTest2026!Alpha',
    ssl: { rejectUnauthorized: true }
});

await client.connect();
const result = await client.query('SELECT * FROM test_schema.users');
console.log(result.rows);
```

### ODBC
```
Driver={ScratchBird ODBC Driver};
Server=scratchbird-test.daltoncalford.dev;
Port=13092;
Database=testdb;
Uid=testuser;
Pwd=SbTest2026!Alpha;
SSLMode=require;
```

### Ruby
```ruby
require 'scratchbird'

conn = ScratchBird::Connection.new(
  host: 'scratchbird-test.daltoncalford.dev',
  port: 13092,
  database: 'testdb',
  user: 'testuser',
  password: 'SbTest2026!Alpha',
  sslmode: 'require'
)

result = conn.exec('SELECT * FROM test_schema.users')
result.each do |row|
  puts row
end
```

### Rust
```rust
use scratchbird::{Client, Config};

let config = Config::new()
    .host("scratchbird-test.daltoncalford.dev")
    .port(13092)
    .database("testdb")
    .user("testuser")
    .password("SbTest2026!Alpha")
    .ssl_mode(SslMode::Require);

let mut client = Client::connect(&config).await?;
let rows = client.query("SELECT * FROM test_schema.users", &[]).await?;
```

### PHP
```php
$conn = scratchbird_connect(
    "host=scratchbird-test.daltoncalford.dev " .
    "port=13092 dbname=testdb user=testuser " .
    "password=SbTest2026!Alpha sslmode=require"
);

$result = scratchbird_query($conn, "SELECT * FROM test_schema.users");
while ($row = scratchbird_fetch_assoc($result)) {
    print_r($row);
}
```

### .NET
```csharp
var connString = "Host=scratchbird-test.daltoncalford.dev;" +
                 "Port=13092;Database=testdb;Username=testuser;" +
                 "Password=SbTest2026!Alpha;SSL Mode=Require";

await using var conn = new ScratchBirdConnection(connString);
await conn.OpenAsync();

await using var cmd = new ScratchBirdCommand(
    "SELECT * FROM test_schema.users", conn);
await using var reader = await cmd.ExecuteReaderAsync();
while (await reader.ReadAsync())
{
    Console.WriteLine(reader.GetString(0));
}
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

## Server Setup (if you need your own)

### One-Line Setup
```bash
curl -fsSL https://raw.githubusercontent.com/DaltonCalford/ScratchBird/main/scripts/setup-test-server.sh | sudo bash -s your-hostname.com
```

### Manual Setup
```bash
# 1. Clone and build
git clone https://github.com/DaltonCalford/ScratchBird.git
cd ScratchBird
cmake -S . -B build && cmake --build build

# 2. Run setup script
sudo ./scripts/setup-test-server.sh scratchbird-test.yourdomain.com

# 3. Test connection
./build/bin/sb_isql \
    --host=localhost --port=13092 \
    --user=testuser --password='SbTest2026!Alpha' \
    --query="SELECT 'Hello World';"
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

## Limits & Guidelines

| Resource | Limit |
|----------|-------|
| Max connections | 100 |
| Query timeout | None (be considerate) |
| Database size | Auto-purged after 1GB |
| Idle timeout | 5 minutes |
| TLS | Required |

**Please:**
- Don't run production workloads
- Don't store sensitive data
- Clean up large test data
- Report issues at https://github.com/DaltonCalford/ScratchBird/issues

---

## Links

- **Main Repository:** https://github.com/DaltonCalford/ScratchBird
- **Driver Repository:** https://github.com/DaltonCalford/ScratchBird-driver
- **Full Setup Guide:** `PUBLIC_TEST_SERVER_SETUP.md`
- **Issue Tracker:** https://github.com/DaltonCalford/ScratchBird/issues

---

**Last Updated:** 2026-02-06
