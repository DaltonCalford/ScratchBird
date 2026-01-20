# Connection Problems

**Status:** Complete
**Last Updated:** 2026-01-20

---

## Overview

This guide helps diagnose and resolve connection problems between clients and ScratchBird. Connection issues typically fall into several categories:

- **Network/Firewall** - Can't reach the server
- **Authentication** - Wrong credentials or method
- **Protocol mismatch** - Wrong port or protocol
- **SSL/TLS** - Certificate or encryption issues
- **Server configuration** - Listen address, max connections

---

## Quick Diagnosis Checklist

Before diving into detailed troubleshooting, verify these basics:

```bash
# 1. Is the server running?
systemctl status scratchbird
# or
ps aux | grep scratchbird

# 2. Is the port open?
ss -tlnp | grep -E '3092|5432|3306|3050'

# 3. Can you reach the port from the client?
nc -zv localhost 5432
# or
telnet localhost 5432

# 4. Test with command-line client
psql -h localhost -p 5432 -U app_user -d scratchbird
```

---

## Connection Refused

### Symptoms

```
psycopg2.OperationalError: could not connect to server: Connection refused
    Is the server running on host "localhost" and accepting
    TCP/IP connections on port 5432?

SQLSTATE[08006]: Connection refused

Error: connect ECONNREFUSED 127.0.0.1:5432
```

### Cause 1: Server Not Running

**Diagnosis:**
```bash
# Check if ScratchBird is running
systemctl status scratchbird

# Check process list
ps aux | grep -E 'sb_listener|scratchbird'

# Check logs for crash
journalctl -u scratchbird --since "10 minutes ago"
```

**Solution:**
```bash
# Start the server
sudo systemctl start scratchbird

# Or manually
sudo -u scratchbird /usr/local/bin/sb_listener -c /etc/scratchbird/sb_server.conf
```

### Cause 2: Wrong Listen Address

The server may be listening on a different address than you're connecting to.

**Diagnosis:**
```bash
# Check what addresses the server is listening on
ss -tlnp | grep -E '3092|5432|3306|3050'

# Example output:
# LISTEN  0  128  127.0.0.1:5432  *:*  users:(("sb_listener",pid=1234))
# This means it's only listening on localhost!
```

**Solution - Edit sb_server.conf:**
```ini
# Listen on all interfaces
[network]
listen_address = 0.0.0.0
# Or specific interface
listen_address = 192.168.1.100
```

Then restart:
```bash
sudo systemctl restart scratchbird
```

### Cause 3: Firewall Blocking

**Diagnosis:**
```bash
# Ubuntu/Debian (ufw)
sudo ufw status verbose

# CentOS/RHEL (firewalld)
sudo firewall-cmd --list-all

# Check iptables directly
sudo iptables -L -n | grep -E '3092|5432|3306|3050'
```

**Solution:**
```bash
# Ubuntu/Debian
sudo ufw allow 5432/tcp comment 'ScratchBird PostgreSQL'
sudo ufw allow 3306/tcp comment 'ScratchBird MySQL'
sudo ufw allow 3050/tcp comment 'ScratchBird Firebird'
sudo ufw allow 3092/tcp comment 'ScratchBird Native'

# CentOS/RHEL
sudo firewall-cmd --permanent --add-port=5432/tcp
sudo firewall-cmd --permanent --add-port=3306/tcp
sudo firewall-cmd --permanent --add-port=3050/tcp
sudo firewall-cmd --permanent --add-port=3092/tcp
sudo firewall-cmd --reload
```

### Cause 4: Wrong Port

**Diagnosis:**
```bash
# Verify which protocol listener is on which port
ss -tlnp | grep sb_listener
```

**ScratchBird Default Ports:**

| Protocol | Default Port | Config Key |
|----------|--------------|------------|
| Native | 3092 | `network.native_port` |
| PostgreSQL | 5432 | `network.postgresql_port` |
| MySQL | 3306 | `network.mysql_port` |
| Firebird | 3050 | `network.firebird_port` |

**Solution:**

Update your connection string to use the correct port:
```python
# Wrong - default PostgreSQL port but server uses custom
conn = psycopg2.connect(host='localhost', port=5432, ...)

# Correct - use the actual configured port
conn = psycopg2.connect(host='localhost', port=5433, ...)
```

---

## Authentication Failed

### Symptoms

```
FATAL: password authentication failed for user "app_user"

psycopg2.OperationalError: FATAL: password authentication failed

Error 1045: Access denied for user 'app_user'@'localhost'

Your user name and password are not defined
```

### Cause 1: Wrong Password

**Diagnosis:**
```bash
# Test with known-good credentials
psql -h localhost -p 5432 -U postgres -d scratchbird

# Check if user exists
psql -h localhost -p 5432 -U postgres -c "SELECT usename FROM pg_user;"
```

**Solution:**
```sql
-- Reset password (connect as superuser)
ALTER USER app_user WITH PASSWORD 'new_secure_password';

-- Or for MySQL protocol users
ALTER USER 'app_user'@'%' IDENTIFIED BY 'new_secure_password';
```

### Cause 2: User Doesn't Exist

**Diagnosis:**
```sql
-- Check users (PostgreSQL protocol)
SELECT usename, usecreatedb, usesuper FROM pg_user;

-- Check users (MySQL protocol)
SELECT user, host FROM mysql.user;
```

**Solution:**
```sql
-- Create user (PostgreSQL syntax)
CREATE USER app_user WITH PASSWORD 'secure_password';
GRANT CONNECT ON DATABASE scratchbird TO app_user;
GRANT USAGE ON SCHEMA public TO app_user;
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO app_user;

-- Create user (MySQL syntax)
CREATE USER 'app_user'@'%' IDENTIFIED BY 'secure_password';
GRANT ALL PRIVILEGES ON scratchbird.* TO 'app_user'@'%';
FLUSH PRIVILEGES;
```

### Cause 3: Authentication Method Mismatch

ScratchBird supports multiple authentication methods. The client and server must agree.

**Diagnosis:**
```bash
# Check server authentication configuration
cat /etc/scratchbird/sb_server.conf | grep -A10 '\[auth\]'

# Check client authentication requirements
# (varies by driver - check driver documentation)
```

**Authentication Methods:**

| Method | Description | When to Use |
|--------|-------------|-------------|
| trust | No authentication | Development only |
| password | Plain text password | Not recommended |
| md5 | MD5 hashed password | Legacy compatibility |
| scram-sha-256 | SCRAM authentication | Recommended |
| cert | Client certificate | High security |

**Solution - sb_server.conf:**
```ini
[auth]
# Use SCRAM-SHA-256 (recommended)
default_auth_method = scram-sha-256

# Or fallback to md5 for legacy clients
default_auth_method = md5
```

### Cause 4: Host-Based Access Denied

**Diagnosis:**
```bash
# Check access rules (similar to pg_hba.conf)
cat /etc/scratchbird/sb_hba.conf
```

**Solution - sb_hba.conf:**
```
# TYPE  DATABASE  USER       ADDRESS         METHOD

# Local connections
local   all       all                        scram-sha-256

# IPv4 local connections
host    all       all        127.0.0.1/32    scram-sha-256

# IPv4 remote connections (your network)
host    all       all        192.168.0.0/16  scram-sha-256

# Allow specific user from specific host
host    mydb      app_user   10.0.0.50/32    scram-sha-256
```

Then reload:
```bash
sudo systemctl reload scratchbird
# or send SIGHUP
sudo kill -HUP $(pidof sb_listener)
```

---

## SSL/TLS Errors

### Symptoms

```
SSL error: certificate verify failed

SQLSTATE[08006]: SSL SYSCALL error: Connection reset by peer

Error: self signed certificate in certificate chain

SSL connection is required but server doesn't support it
```

### Cause 1: SSL Required but Not Enabled

**Diagnosis:**
```bash
# Check if SSL is enabled on server
cat /etc/scratchbird/sb_server.conf | grep -A10 '\[ssl\]'

# Test SSL connection
openssl s_client -connect localhost:5432 -starttls postgres
```

**Solution - Enable SSL on server:**
```ini
# sb_server.conf
[ssl]
enabled = true
cert_file = /etc/scratchbird/ssl/server.crt
key_file = /etc/scratchbird/ssl/server.key
ca_file = /etc/scratchbird/ssl/ca.crt  # Optional, for client certs
```

Generate certificates if needed:
```bash
# Self-signed (development)
openssl req -new -x509 -days 365 -nodes \
    -out /etc/scratchbird/ssl/server.crt \
    -keyout /etc/scratchbird/ssl/server.key \
    -subj "/CN=scratchbird"

# Set permissions
chmod 600 /etc/scratchbird/ssl/server.key
chown scratchbird:scratchbird /etc/scratchbird/ssl/*
```

### Cause 2: Certificate Verification Failing

**Diagnosis:**
```bash
# Check certificate details
openssl x509 -in /etc/scratchbird/ssl/server.crt -text -noout

# Verify certificate chain
openssl verify -CAfile /path/to/ca.crt /etc/scratchbird/ssl/server.crt
```

**Solution - Client connection string:**
```python
# Python - Disable verification (development only!)
conn = psycopg2.connect(
    "host=localhost dbname=scratchbird sslmode=require sslrootcert=/path/to/ca.crt"
)

# Or skip verification (NOT for production)
conn = psycopg2.connect(
    "host=localhost dbname=scratchbird sslmode=require"
)
```

**SSL Modes:**

| Mode | Description | Use Case |
|------|-------------|----------|
| disable | No SSL | Testing only |
| allow | Try SSL, fallback to plain | Transitional |
| prefer | Prefer SSL, fallback to plain | Default |
| require | Require SSL, don't verify cert | Basic encryption |
| verify-ca | Verify server cert CA | Production |
| verify-full | Verify CA and hostname | High security |

### Cause 3: SSL Mode Mismatch

**Diagnosis:**
```bash
# Check server SSL mode requirement
cat /etc/scratchbird/sb_hba.conf | grep hostssl
```

**Solution:**
```
# sb_hba.conf - require SSL for remote connections
hostssl  all  all  0.0.0.0/0  scram-sha-256

# Or allow both SSL and non-SSL
host     all  all  0.0.0.0/0  scram-sha-256
hostssl  all  all  0.0.0.0/0  scram-sha-256
```

---

## Timeout Errors

### Symptoms

```
OperationalError: could not connect to server: Connection timed out

Error: Connection timed out after 30000ms

SQLSTATE[HY000]: Connection timed out
```

### Cause 1: Network Routing Issues

**Diagnosis:**
```bash
# Trace route to server
traceroute db.example.com

# Check if packets are reaching the server
tcpdump -i any port 5432

# Test with longer timeout
nc -zv -w 30 db.example.com 5432
```

**Solution:**

1. Check VPN connections if applicable
2. Verify DNS resolution: `nslookup db.example.com`
3. Check for network ACLs or security groups (cloud environments)

### Cause 2: Server Overloaded

**Diagnosis:**
```sql
-- Check current connections
SELECT count(*) FROM pg_stat_activity;

-- Check max connections setting
SHOW max_connections;
```

**Solution:**
```ini
# sb_server.conf - increase limits
[connections]
max_connections = 200

# Consider using connection pooling (PgBouncer, etc.)
```

### Cause 3: Client Timeout Too Short

**Solution - Increase client timeout:**

```python
# Python (psycopg2)
conn = psycopg2.connect(
    host='db.example.com',
    connect_timeout=30  # seconds
)

# Python (asyncpg)
conn = await asyncpg.connect(
    host='db.example.com',
    timeout=30
)
```

```javascript
// Node.js (pg)
const pool = new Pool({
    connectionTimeoutMillis: 30000
});
```

```java
// Java (JDBC)
Properties props = new Properties();
props.setProperty("connectTimeout", "30");
props.setProperty("socketTimeout", "60");
Connection conn = DriverManager.getConnection(url, props);
```

---

## Too Many Connections

### Symptoms

```
FATAL: too many connections for role "app_user"

FATAL: sorry, too many clients already

Error 1040: Too many connections
```

### Cause 1: Connection Limit Reached

**Diagnosis:**
```sql
-- Check current vs max connections
SELECT count(*) as current,
       (SELECT setting::int FROM pg_settings WHERE name = 'max_connections') as max
FROM pg_stat_activity;

-- See who's connected
SELECT usename, client_addr, state, query_start, query
FROM pg_stat_activity
ORDER BY query_start;

-- Check per-user limits
SELECT rolname, rolconnlimit FROM pg_roles WHERE rolconnlimit > 0;
```

**Solution:**
```ini
# sb_server.conf - increase limit (requires restart)
[connections]
max_connections = 200
```

Or set per-user limits:
```sql
-- Limit user to 20 connections
ALTER USER app_user CONNECTION LIMIT 20;
```

### Cause 2: Connection Leak

Applications not closing connections properly.

**Diagnosis:**
```sql
-- Find idle connections
SELECT usename, application_name, client_addr, state,
       EXTRACT(EPOCH FROM (now() - state_change)) as idle_seconds
FROM pg_stat_activity
WHERE state = 'idle'
ORDER BY idle_seconds DESC;
```

**Solution - Application side:**
```python
# Python - Always use context managers
with psycopg2.connect(...) as conn:
    with conn.cursor() as cur:
        cur.execute("SELECT ...")

# Or use connection pooling
from psycopg2 import pool
connection_pool = pool.SimpleConnectionPool(1, 20, ...)
```

```javascript
// Node.js - Use pool and release connections
const pool = new Pool({ max: 20 });

// Always release connections
const client = await pool.connect();
try {
    await client.query('SELECT ...');
} finally {
    client.release();  // Important!
}
```

**Solution - Server side (terminate idle connections):**
```sql
-- Kill connections idle for more than 1 hour
SELECT pg_terminate_backend(pid)
FROM pg_stat_activity
WHERE state = 'idle'
AND state_change < NOW() - INTERVAL '1 hour';
```

Or configure automatic idle timeout:
```ini
# sb_server.conf
[connections]
idle_in_transaction_session_timeout = 300000  # 5 minutes in ms
```

---

## Protocol Mismatch

### Symptoms

```
Error: Invalid startup packet

Protocol error: unexpected message type

ERROR: unsupported frontend protocol
```

### Cause: Wrong Protocol/Port Combination

**Diagnosis:**

Ensure you're using the correct driver for the port:

| Port | Protocol | Correct Driver |
|------|----------|----------------|
| 5432 | PostgreSQL | psycopg2, pg, Npgsql, pgx |
| 3306 | MySQL | mysql-connector, mysql2, MySqlConnector |
| 3050 | Firebird | fdb, node-firebird, FirebirdClient |
| 3092 | Native | libscratchbird (future) |

**Solution:**

```python
# Wrong: Using MySQL driver on PostgreSQL port
import mysql.connector
conn = mysql.connector.connect(host='localhost', port=5432, ...)  # Wrong!

# Correct: Use PostgreSQL driver for port 5432
import psycopg2
conn = psycopg2.connect(host='localhost', port=5432, ...)  # Correct!

# Or use MySQL driver with MySQL port
import mysql.connector
conn = mysql.connector.connect(host='localhost', port=3306, ...)  # Correct!
```

---

## DNS Resolution Issues

### Symptoms

```
could not translate host name "db.example.com" to address

Name or service not known

getaddrinfo ENOTFOUND db.example.com
```

### Cause: DNS Not Resolving

**Diagnosis:**
```bash
# Check DNS resolution
nslookup db.example.com
dig db.example.com

# Check /etc/hosts
cat /etc/hosts | grep db.example.com

# Check resolv.conf
cat /etc/resolv.conf
```

**Solution:**

1. Add to /etc/hosts (temporary fix):
```
192.168.1.100  db.example.com
```

2. Fix DNS configuration:
```bash
# Check systemd-resolved (Ubuntu)
systemd-resolve --status

# Or use specific DNS server
echo "nameserver 8.8.8.8" | sudo tee /etc/resolv.conf
```

3. Use IP address directly:
```python
conn = psycopg2.connect(host='192.168.1.100', ...)
```

---

## Docker/Container Connection Issues

### Symptoms

```
Connection refused (connecting to localhost from container)

could not connect to server: No route to host
```

### Cause 1: Localhost Doesn't Work in Containers

**Solution:**

```yaml
# docker-compose.yml
services:
  app:
    environment:
      - DATABASE_HOST=scratchbird  # Use service name, not localhost
    depends_on:
      - scratchbird

  scratchbird:
    image: scratchbird/scratchbird
    ports:
      - "5432:5432"
```

```python
# In container, use Docker service name
conn = psycopg2.connect(
    host='scratchbird',  # Not 'localhost'
    port=5432,
    ...
)
```

### Cause 2: Network Mode Issues

**Solution:**
```yaml
# Use same network
services:
  app:
    networks:
      - dbnet
  scratchbird:
    networks:
      - dbnet

networks:
  dbnet:
    driver: bridge
```

### Cause 3: Connecting from Host to Container

```bash
# Ensure port is mapped
docker ps | grep scratchbird
# Should show: 0.0.0.0:5432->5432/tcp

# Connect using localhost and mapped port
psql -h localhost -p 5432 -U app_user -d scratchbird
```

---

## Connection Diagnostic Commands

### Quick Health Check Script

```bash
#!/bin/bash
# save as check_connection.sh

HOST="${1:-localhost}"
PORT="${2:-5432}"
USER="${3:-app_user}"
DB="${4:-scratchbird}"

echo "=== ScratchBird Connection Diagnostics ==="
echo "Target: $HOST:$PORT as $USER"
echo

echo "1. DNS Resolution:"
getent hosts $HOST || echo "  FAILED: Cannot resolve hostname"
echo

echo "2. Port Reachability:"
nc -zv -w5 $HOST $PORT 2>&1 || echo "  FAILED: Cannot reach port"
echo

echo "3. SSL Handshake (if applicable):"
timeout 5 openssl s_client -connect $HOST:$PORT -starttls postgres 2>&1 | head -20
echo

echo "4. Database Connection:"
PGPASSWORD="$PGPASSWORD" psql -h $HOST -p $PORT -U $USER -d $DB -c "SELECT version();" 2>&1
echo

echo "=== Diagnostics Complete ==="
```

### Python Diagnostic Script

```python
#!/usr/bin/env python3
"""ScratchBird connection diagnostic tool"""

import socket
import ssl
import sys

def check_connection(host, port, user, password, database):
    print(f"Testing connection to {host}:{port}")

    # 1. DNS resolution
    try:
        ip = socket.gethostbyname(host)
        print(f"[OK] DNS resolved: {host} -> {ip}")
    except socket.gaierror as e:
        print(f"[FAIL] DNS resolution failed: {e}")
        return False

    # 2. TCP connection
    try:
        sock = socket.create_connection((host, port), timeout=10)
        print(f"[OK] TCP connection established")
        sock.close()
    except socket.error as e:
        print(f"[FAIL] TCP connection failed: {e}")
        return False

    # 3. Database connection
    try:
        import psycopg2
        conn = psycopg2.connect(
            host=host,
            port=port,
            user=user,
            password=password,
            database=database,
            connect_timeout=10
        )
        cur = conn.cursor()
        cur.execute("SELECT version()")
        version = cur.fetchone()[0]
        print(f"[OK] Database connection successful")
        print(f"     Server: {version}")
        conn.close()
        return True
    except Exception as e:
        print(f"[FAIL] Database connection failed: {e}")
        return False

if __name__ == "__main__":
    import os
    check_connection(
        host=os.getenv('DB_HOST', 'localhost'),
        port=int(os.getenv('DB_PORT', 5432)),
        user=os.getenv('DB_USER', 'app_user'),
        password=os.getenv('DB_PASSWORD', ''),
        database=os.getenv('DB_NAME', 'scratchbird')
    )
```

---

## Quick Reference: Connection String Formats

### PostgreSQL Protocol

```
# URI format
postgresql://user:password@host:5432/database?sslmode=require

# Key-value format
host=localhost port=5432 dbname=scratchbird user=app_user password=secret sslmode=prefer
```

### MySQL Protocol

```
# URI format
mysql://user:password@host:3306/database

# DSN format
host=localhost;port=3306;dbname=scratchbird;user=app_user;password=secret
```

### Firebird Protocol

```
# Connection string
SYSDBA:masterkey@localhost/3050:scratchbird
```

---

## See Also

- [Performance Issues](Performance-Issues.md) - Slow query and throughput problems
- [Common Errors](Common-Errors.md) - Error code reference
- [Security Guide](../admin/security.md) - Authentication and SSL configuration
- [Driver Comparison](../drivers/Driver-Comparison.md) - Choose the right driver
