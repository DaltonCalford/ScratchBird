# ScratchBird Public Test Server Setup Guide

**Purpose:** Internet-accessible ScratchBird server for driver/application testing  
**Protocol:** Native ScratchBird (SBWP v1.1) only - No emulation  
**Date:** February 6, 2026

---

## Connection Parameters (Local Test Server)

### Server Configuration

```
Host:     127.0.0.1    (localhost - bind to loopback only)
Port:     13092        (Dedicated test port)
Database: testdb
TLS:      Required (TLS 1.3)
```

### Users for Security Testing

| Username | Password | Role | Permissions | Purpose |
|----------|----------|------|-------------|---------|
| **SYSARCH** | `SysArch2026!` | System Architect | ALL | Administrative access, DDL operations |
| **TESTUSER** | `TestUser2026!` | Standard User | SELECT, INSERT, UPDATE, DELETE | Application testing, limited DML |

### Connection Strings

**SYSARCH (Full Access):**
```
scratchbird://SYSARCH:SysArch2026!@127.0.0.1:13092/testdb
```

**TESTUSER (Limited Access):**
```
scratchbird://TESTUSER:TestUser2026!@127.0.0.1:13092/testdb
```

### Connection String Formats

**Native C/C++:**
```c
// SYSARCH - Full administrative access
const char* conn_string_sysarch = 
    "scratchbird://SYSARCH:SysArch2026!@127.0.0.1:13092/testdb?sslmode=require";

// TESTUSER - Standard application access
const char* conn_string_testuser = 
    "scratchbird://TESTUSER:TestUser2026!@127.0.0.1:13092/testdb?sslmode=require";
```

**Python:**
```python
import scratchbird

# SYSARCH - Full access for DDL operations
conn_sysarch = scratchbird.connect(
    host="127.0.0.1",
    port=13092,
    database="testdb",
    user="SYSARCH",
    password="SysArch2026!",
    ssl=True
)

# TESTUSER - Limited access for application testing
conn_testuser = scratchbird.connect(
    host="127.0.0.1",
    port=13092,
    database="testdb",
    user="TESTUSER",
    password="TestUser2026!",
    ssl=True
)
```

**Go:**
```go
// SYSARCH connection
connSysarch, err := scratchbird.Connect(
    "scratchbird://SYSARCH:SysArch2026!@127.0.0.1:13092/testdb?ssl=require")

// TESTUSER connection
connTestuser, err := scratchbird.Connect(
    "scratchbird://TESTUSER:TestUser2026!@127.0.0.1:13092/testdb?ssl=require")
```

**JDBC:**
```java
// SYSARCH
String url = "jdbc:scratchbird://127.0.0.1:13092/testdb?sslmode=require";
Connection connSysarch = DriverManager.getConnection(url, "SYSARCH", "SysArch2026!");

// TESTUSER
Connection connTestuser = DriverManager.getConnection(url, "TESTUSER", "TestUser2026!");
```

**ODBC:**
```
Driver={ScratchBird ODBC Driver};
Server=127.0.0.1;
Port=13092;
Database=testdb;
Uid=SYSARCH;
Pwd=SysArch2026!;
SSLMode=require;
```

**Node.js:**
```javascript
// SYSARCH
const connSysarch = await scratchbird.connect({
    host: '127.0.0.1',
    port: 13092,
    database: 'testdb',
    user: 'SYSARCH',
    password: 'SysArch2026!',
    ssl: { rejectUnauthorized: false }  // For self-signed certs
});

// TESTUSER
const connTestuser = await scratchbird.connect({
    host: '127.0.0.1',
    port: 13092,
    database: 'testdb',
    user: 'TESTUSER',
    password: 'TestUser2026!',
    ssl: { rejectUnauthorized: false }
});
```

---

## Server Setup Instructions

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev liblz4-dev

# CentOS/RHEL
sudo yum install -y gcc-c++ cmake openssl-devel lz4-devel

# macOS
brew install cmake openssl lz4
```

### 1. Build ScratchBird

```bash
cd /opt
git clone https://github.com/DaltonCalford/ScratchBird.git
cd ScratchBird

cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DSCRATCHBIRD_ENABLE_TLS=ON \
    -DSCRATCHBIRD_PORT=13092

cmake --build build -j$(nproc)

# Verify build
./build/bin/sb_server --version
```

### 2. Create Test Database

```bash
# Create database directory
sudo mkdir -p /var/scratchbird/testdb
sudo chown -R scratchbird:scratchbird /var/scratchbird

# Create database with 16KB page size (recommended)
./build/bin/sb_createdb \
    --database=/var/scratchbird/testdb/testdb.sdb \
    --page-size=16384 \
    --encoding=UTF8 \
    --owner=testuser

# Alternative page sizes for testing:
# --page-size=8192   (8KB - small footprint)
# --page-size=32768  (32KB - balanced)
# --page-size=65536  (64KB - large datasets)
# --page-size=131072 (128KB - DSS workloads)
```

### 3. Create Test Users (SYSARCH and TESTUSER)

```bash
# Start server temporarily for user creation
./build/bin/sb_server \
    --database=/var/scratchbird/testdb/testdb.sdb \
    --port=13092 \
    --bind=127.0.0.1 &

SERVER_PID=$!
sleep 2

# Create SYSARCH user (System Architect - Full Access)
./build/bin/sb_security user-create \
    --host=127.0.0.1 \
    --port=13092 \
    --username=admin \
    --new-user=SYSARCH \
    --password='SysArch2026!' \
    --role=sysarch

# Grant SYSARCH full privileges
./build/bin/sb_security grant \
    --host=127.0.0.1 \
    --port=13092 \
    --username=admin \
    --grantee=SYSARCH \
    --database=testdb \
    --privileges=ALL

# Create TESTUSER (Standard Application User - Limited Access)
./build/bin/sb_security user-create \
    --host=127.0.0.1 \
    --port=13092 \
    --username=admin \
    --new-user=TESTUSER \
    --password='TestUser2026!' \
    --role=standard

# Grant TESTUSER DML privileges only (no DDL)
./build/bin/sb_security grant \
    --host=127.0.0.1 \
    --port=13092 \
    --username=admin \
    --grantee=TESTUSER \
    --database=testdb \
    --privileges=SELECT,INSERT,UPDATE,DELETE

# Stop temporary server
kill $SERVER_PID
```

### 4. Create Test Schema

```bash
# Start server
./build/bin/sb_server \
    --database=/var/scratchbird/testdb/testdb.sdb \
    --port=13092 \
    --tls-cert=/etc/scratchbird/server.crt \
    --tls-key=/etc/scratchbird/server.key &

# Connect and create schema
./build/bin/sb_isql \
    --host=localhost \
    --port=13092 \
    --database=testdb \
    --user=testuser \
    --password=SbTest2026!Alpha \
    --ssl << 'EOF'

-- Create test schema
CREATE SCHEMA test_schema;

-- Simple test table
CREATE TABLE test_schema.users (
    id INTEGER PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    username VARCHAR(50) NOT NULL,
    email VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    active BOOLEAN DEFAULT TRUE
);

-- Test table with various types
CREATE TABLE test_schema.data_types (
    id INTEGER PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    
    -- Numeric types
    small_int SMALLINT,
    normal_int INTEGER,
    big_int BIGINT,
    real_num REAL,
    double_num DOUBLE PRECISION,
    decimal_num DECIMAL(18,4),
    
    -- String types
    var_char VARCHAR(255),
    char_char CHAR(10),
    text_field TEXT,
    
    -- Binary
    bytea_data BYTEA,
    
    -- Temporal
    date_field DATE,
    time_field TIME,
    timestamp_field TIMESTAMP,
    timestamptz_field TIMESTAMPTZ,
    
    -- JSON
    json_data JSON,
    jsonb_data JSONB,
    
    -- UUID
    uuid_field UUID,
    
    -- Arrays
    int_array INTEGER[],
    text_array VARCHAR[]
);

-- Insert test data
INSERT INTO test_schema.users (username, email, active) VALUES
    ('alice', 'alice@example.com', TRUE),
    ('bob', 'bob@example.com', TRUE),
    ('charlie', 'charlie@example.com', FALSE),
    ('diana', 'diana@example.com', TRUE);

INSERT INTO test_schema.data_types (
    small_int, normal_int, big_int, real_num, double_num, decimal_num,
    var_char, char_char, text_field,
    date_field, time_field, timestamp_field,
    json_data, jsonb_data, uuid_field
) VALUES (
    100, 1000, 1000000,
    3.14, 2.718281828, 12345.6789,
    'Test String', 'FIXED     ', 'This is a longer text field for testing',
    '2026-02-06', '14:30:00', '2026-02-06 14:30:00',
    '{"key": "value", "number": 42}', '{"indexed": true, "data": "test"}',
    '550e8400-e29b-41d4-a716-446655440000'
);

-- Create indexes
CREATE INDEX idx_users_email ON test_schema.users(email);
CREATE INDEX idx_users_active ON test_schema.users(active);

COMMIT;

SELECT 'Test database initialized successfully' AS status;

EOF

# Keep server running
```

---

## 5. TLS Certificate Setup

### Option A: Let's Encrypt (Recommended for Public Server)

```bash
# Install certbot
sudo apt-get install -y certbot

# Obtain certificate
sudo certbot certonly \
    --standalone \
    -d scratchbird-test.daltoncalford.dev \
    --agree-tos \
    --email admin@daltoncalford.dev

# Copy certificates
sudo mkdir -p /etc/scratchbird
sudo cp /etc/letsencrypt/live/scratchbird-test.daltoncalford.dev/fullchain.pem /etc/scratchbird/server.crt
sudo cp /etc/letsencrypt/live/scratchbird-test.daltoncalford.dev/privkey.pem /etc/scratchbird/server.key
sudo chmod 600 /etc/scratchbird/server.key
sudo chown scratchbird:scratchbird /etc/scratchbird/*.pem

# Auto-renewal hook
echo '#!/bin/bash
cp /etc/letsencrypt/live/scratchbird-test.daltoncalford.dev/fullchain.pem /etc/scratchbird/server.crt
cp /etc/letsencrypt/live/scratchbird-test.daltoncalford.dev/privkey.pem /etc/scratchbird/server.key
chown scratchbird:scratchbird /etc/scratchbird/*.pem
systemctl restart scratchbird' | sudo tee /etc/letsencrypt/renewal-hooks/deploy/scratchbird
sudo chmod +x /etc/letsencrypt/renewal-hooks/deploy/scratchbird
```

### Option B: Self-Signed Certificate (Development Only)

```bash
sudo mkdir -p /etc/scratchbird
sudo openssl req -x509 -nodes -days 365 -newkey rsa:4096 \
    -keyout /etc/scratchbird/server.key \
    -out /etc/scratchbird/server.crt \
    -subj "/C=US/ST=State/L=City/O=ScratchBird/CN=scratchbird-test.daltoncalford.dev"
sudo chmod 600 /etc/scratchbird/server.key
sudo chown scratchbird:scratchbird /etc/scratchbird/*.pem
```

**Note:** Drivers will need to set `sslmode=allow` or disable certificate verification for self-signed certs.

---

## 6. Systemd Service Configuration

```bash
sudo tee /etc/systemd/system/scratchbird-test.service << 'EOF'
[Unit]
Description=ScratchBird Test Server
After=network.target

[Service]
Type=simple
User=scratchbird
Group=scratchbird
WorkingDirectory=/var/scratchbird/testdb

ExecStart=/opt/ScratchBird/build/bin/sb_server \
    --database=/var/scratchbird/testdb/testdb.sdb \
    --port=13092 \
    --bind=127.0.0.1 \
    --tls-cert=/etc/scratchbird/server.crt \
    --tls-key=/etc/scratchbird/server.key \
    --log-level=info \
    --log-file=/var/log/scratchbird/testdb.log \
    --max-connections=100

Restart=always
RestartSec=5

# Security
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/scratchbird/testdb /var/log/scratchbird

[Install]
WantedBy=multi-user.target
EOF

# Create log directory
sudo mkdir -p /var/log/scratchbird
sudo chown -R scratchbird:scratchbird /var/log/scratchbird

# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable scratchbird-test
sudo systemctl start scratchbird-test

# Check status
sudo systemctl status scratchbird-test
sudo tail -f /var/log/scratchbird/testdb.log
```

---

## 7. Firewall Configuration

```bash
# UFW (Ubuntu)
sudo ufw allow 13092/tcp comment 'ScratchBird Test Server'
sudo ufw reload

# firewalld (CentOS/RHEL)
sudo firewall-cmd --permanent --add-port=13092/tcp
sudo firewall-cmd --reload

# iptables
sudo iptables -A INPUT -p tcp --dport 13092 -j ACCEPT
sudo iptables-save

# Cloud-specific (AWS Security Group, GCP Firewall, etc.)
# Allow TCP port 13092 from 0.0.0.0/0 (or restrict to specific IPs)
```

---

## 8. Docker Deployment (Alternative)

### Dockerfile

```dockerfile
FROM ubuntu:24.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential cmake libssl-dev liblz4-dev \
    git wget \
    && rm -rf /var/lib/apt/lists/*

# Build ScratchBird
WORKDIR /opt
RUN git clone https://github.com/DaltonCalford/ScratchBird.git
WORKDIR /opt/ScratchBird
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j$(nproc)

# Create database
RUN mkdir -p /var/scratchbird/testdb \
    && ./build/bin/sb_createdb \
        --database=/var/scratchbird/testdb/testdb.sdb \
        --page-size=16384 \
        --encoding=UTF8

# Generate self-signed cert
RUN openssl req -x509 -nodes -days 365 -newkey rsa:4096 \
    -keyout /var/scratchbird/server.key \
    -out /var/scratchbird/server.crt \
    -subj "/CN=localhost"

# Expose port
EXPOSE 13092

# Start server
CMD ["/opt/ScratchBird/build/bin/sb_server", \
     "--database=/var/scratchbird/testdb/testdb.sdb", \
     "--port=13092", \
     "--bind=127.0.0.1", \
     "--tls-cert=/var/scratchbird/server.crt", \
     "--tls-key=/var/scratchbird/server.key"]
```

### Docker Run

```bash
# Build
docker build -t scratchbird-test:latest .

# Run
docker run -d \
    --name scratchbird-test \
    -p 13092:13092 \
    -v scratchbird-data:/var/scratchbird/testdb \
    scratchbird-test:latest

# Check logs
docker logs -f scratchbird-test

# Connect from host
docker exec -it scratchbird-test \
    /opt/ScratchBird/build/bin/sb_isql \
    --host=localhost --port=13092 \
    --database=testdb --user=testuser
```

### Docker Compose

```yaml
version: '3.8'

services:
  scratchbird-test:
    build: .
    container_name: scratchbird-test
    ports:
      - "13092:13092"
    volumes:
      - scratchbird-data:/var/scratchbird/testdb
      - ./ssl:/etc/scratchbird/ssl:ro
    environment:
      - SB_LOG_LEVEL=info
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "sb_isql", "--host=localhost", "--port=13092", "--query=SELECT 1"]
      interval: 30s
      timeout: 10s
      retries: 3

volumes:
  scratchbird-data:
```

---

## 9. Health Monitoring

### Health Check Script

```bash
#!/bin/bash
# /usr/local/bin/scratchbird-health.sh

HOST="scratchbird-test.daltoncalford.dev"
PORT="13092"
DB="testdb"
USER="testuser"
PASS="SbTest2026!Alpha"

# Test connection
/opt/ScratchBird/build/bin/sb_isql \
    --host=$HOST --port=$PORT --database=$DB \
    --user=$USER --password=$PASS \
    --ssl --query="SELECT 'healthy' AS status;" > /dev/null 2>&1

if [ $? -eq 0 ]; then
    echo "$(date): ScratchBird test server is healthy"
    exit 0
else
    echo "$(date): ScratchBird test server is DOWN"
    # Send alert (configure as needed)
    # echo "Server down" | mail -s "ScratchBird Alert" admin@example.com
    exit 1
fi
```

### Prometheus Metrics (if available)

```bash
# Add to prometheus.yml
scrape_configs:
  - job_name: 'scratchbird-test'
    static_configs:
      - targets: ['scratchbird-test.daltoncalford.dev:13093']  # Metrics port
```

---

## 10. Test Database Schema Reference

### Tables

| Table | Schema | Purpose |
|-------|--------|---------|
| `users` | `test_schema` | Basic CRUD testing |
| `data_types` | `test_schema` | Type coverage testing |

### Users

| Username | Password | Role | Permissions |
|----------|----------|------|-------------|
| `testuser` | `SbTest2026!Alpha` | Standard | ALL on testdb |
| `readonly` | `ReadOnly2026!` | Read-only | SELECT on testdb |

### Connection Limits

- Max connections: 100
- Idle timeout: 300 seconds
- Query timeout: 0 (unlimited)

---

## Troubleshooting

### Cannot Connect

```bash
# Test from server localhost
./build/bin/sb_isql --host=localhost --port=13092 --database=testdb --user=testuser

# Check server is listening
sudo netstat -tlnp | grep 13092

# Check firewall
sudo iptables -L | grep 13092

# Check logs
sudo tail -f /var/log/scratchbird/testdb.log
```

### TLS Errors

```bash
# Verify certificate
openssl s_client -connect scratchbird-test.daltoncalford.dev:13092 -tls1_3

# Check certificate dates
openssl x509 -in /etc/scratchbird/server.crt -noout -dates

# Test with disabled verification (development only)
sb_isql --host=scratchbird-test.daltoncalford.dev --port=13092 \
    --sslmode=allow --user=testuser
```

### Performance Issues

```bash
# Check connections
./build/bin/sb_admin stats --host=localhost --port=13092

# Monitor resources
htop / iostat / vmstat

# Check for locks
./build/bin/sb_isql --query="SELECT * FROM sys.locks;"
```

---

## Quick Reference Card

```
╔══════════════════════════════════════════════════════════════╗
║           SCRATCHBIRD TEST SERVER - QUICK REF               ║
╠══════════════════════════════════════════════════════════════╣
║ Host:     scratchbird-test.daltoncalford.dev                ║
║ Port:     13092                                             ║
║ Database: testdb                                            ║
║ Username: testuser                                          ║
║ Password: SbTest2026!Alpha                                  ║
╠══════════════════════════════════════════════════════════════╣
║ Connection String:                                          ║
║ scratchbird://testuser:SbTest2026!Alpha@                    ║
║   scratchbird-test.daltoncalford.dev:13092/testdb           ║
╠══════════════════════════════════════════════════════════════╣
║ TLS: Required (TLS 1.3)                                     ║
║ Page Size: 16KB                                             ║
║ Encoding: UTF8                                              ║
╚══════════════════════════════════════════════════════════════╝
```

---

**Last Updated:** 2026-02-06  
**Server Admin:** admin@daltoncalford.dev  
**Issue Tracker:** https://github.com/DaltonCalford/ScratchBird/issues
