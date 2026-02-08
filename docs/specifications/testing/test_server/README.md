# Test Server Specification

**[← Back to Testing Specifications](../README.md)**

## Overview

The ScratchBird Test Server provides a dedicated instance for driver development, GUI testing, security validation, and integration testing. This specification documents the test server's configuration, authentication modes, security features, and usage guidelines.

## Quick Reference

| Property | Value |
|----------|-------|
| **Purpose** | Development and testing of drivers, GUI tools, and security compliance |
| **Location** | `~/.scratchbird/testdb/` (user-space) |
| **Binary** | `build/src/sb_server` |
| **Config** | `~/.scratchbird/testdb/test-server.conf` |
| **HBA Config** | `~/.scratchbird/testdb/scratchbird_hba.conf` |
| **Logs** | `~/.scratchbird/testdb/logs/server.stdout` |

## Connection Information

### Network Endpoints

| Protocol | Host | Port | Auth Mode |
|----------|------|------|-----------|
| Native (SBWP) | 127.0.0.1 | 3092 | Bootstrap / SCRAM-SHA-256 |
| PostgreSQL | 127.0.0.1 | 5432 | Bootstrap / SCRAM-SHA-256 |
| Firebird | 127.0.0.1 | 3050 | Bootstrap / SCRAM-SHA-256 |
| Unix Socket | `build/ipc/scratchbird-*.sock` | - | Bootstrap / SCRAM-SHA-256 |

### Connection Strings

**Bootstrap Mode (Initial Setup):**
```
scratchbird://anyuser:anypass@127.0.0.1:3092/testdb
postgresql://anyuser:anypass@127.0.0.1:5432/testdb
firebird://anyuser:anypass@127.0.0.1:3050/testdb
```

**SCRAM-SHA-256 Mode (After User Creation):**
```
scratchbird://testuser:TestPass2026!@127.0.0.1:3092/testdb
postgresql://admin:AdminPass2026!@127.0.0.1:5432/testdb
firebird://SYSDBA:SYSDBA2026!@127.0.0.1:3050/testdb
```

## Authentication Modes

The test server supports two authentication modes for comprehensive testing:

### Mode 1: Bootstrap Authentication

**Purpose:** Initial database setup and connectivity testing

**Behavior:**
- Any username is accepted
- Any password is accepted
- User is authenticated as SYSTEM (superuser)
- Available only when no real users exist in database

**Use Cases:**
- First-time server setup
- Driver connectivity testing
- GUI tool initial configuration
- Creating real user accounts

**Credentials:**
| Username | Password | Result |
|----------|----------|--------|
| `any` | `any` | ✅ Authenticated as SYSTEM |

### Mode 2: SCRAM-SHA-256 Authentication

**Purpose:** Production-like security compliance testing

**Behavior:**
- RFC 5802 / RFC 7677 compliant SCRAM-SHA-256 challenge-response
- Salt-based password hashing
- Protection against replay attacks
- Server authentication to client

**Use Cases:**
- Security compliance validation
- Driver SCRAM implementation testing
- Password policy verification
- HBA rule enforcement testing

**Credentials:**
| Username | Password | Role |
|----------|----------|------|
| `testuser` | `TestPass2026!` | Standard user |
| `admin` | `AdminPass2026!` | Superuser |
| `SYSDBA` | `SYSDBA2026!` | Superuser (Firebird compatible) |

## Security Features

### Host-Based Authentication (HBA)

Configuration file: `scratchbird_hba.conf`

```
# Local connections - SCRAM-SHA-256 required
local   all   all                 scram-sha-256

# IPv4 local connections - SCRAM-SHA-256 required
host    all   all   127.0.0.1/32  scram-sha-256

# IPv6 local connections - SCRAM-SHA-256 required
host    all   all   ::1/128       scram-sha-256

# Reject all remote connections
host    all   all   0.0.0.0/0     reject
```

### Rate Limiting

| Setting | Value | Description |
|---------|-------|-------------|
| `max_failed_attempts` | 5 | Failed attempts before lockout |
| `lockout_duration` | 300 seconds | Account lockout time |

### Password Policy

| Feature | Status |
|---------|--------|
| Minimum length | Configurable |
| Complexity requirements | Configurable |
| Expiration | Configurable |
| History | Configurable |
| bcrypt hashing | ✅ Available |

## Test Server Management

### Starting the Server

```bash
cd /path/to/ScratchBird

# Using the management script
./scripts/test-server-user.sh start

# Or manually
./build/src/sb_server -F -c "$HOME/.scratchbird/testdb/test-server.conf"
```

### Stopping the Server

```bash
# Using the management script
./scripts/test-server-user.sh stop

# Or manually
pkill -f "sb_server.*test-server.conf"
```

### Checking Status

```bash
# Using the management script
./scripts/test-server-user.sh status

# Or manually
ps aux | grep sb_server
nc -z 127.0.0.1 3092 && echo "Native: OK"
nc -z 127.0.0.1 5432 && echo "PostgreSQL: OK"
nc -z 127.0.0.1 3050 && echo "Firebird: OK"
```

### Viewing Logs

```bash
# Server output
tail -f ~/.scratchbird/testdb/logs/server.stdout

# With grep filters
tail -f ~/.scratchbird/testdb/logs/server.stdout | grep -E "AUTH|ERROR|WARN"
```

## Switching Authentication Modes

### From Bootstrap to SCRAM-SHA-256

1. **Connect via bootstrap mode** and create real users:
   ```sql
   CREATE USER testuser PASSWORD 'TestPass2026!';
   CREATE USER admin PASSWORD 'AdminPass2026!' SUPERUSER;
   CREATE USER SYSDBA PASSWORD 'SYSDBA2026!' SUPERUSER;
   ```

2. **Verify users exist**:
   ```sql
   SELECT user_name, is_superuser FROM system.users;
   ```

3. **Restart server** (automatically switches to SCRAM mode when real users exist)

4. **Test SCRAM authentication**:
   ```bash
   # Should prompt for SCRAM challenge-response
   ./build/src/scratchbird --host=127.0.0.1 --port=3092 --user=testuser
   ```

## Testing Scenarios

### Scenario 1: Driver Connectivity Testing

**Objective:** Verify driver can connect and execute basic operations

**Steps:**
1. Start test server in bootstrap mode
2. Connect with driver using any credentials
3. Execute test queries
4. Verify results

**Expected Results:**
- Connection accepted
- Queries execute successfully
- Results match expected output

### Scenario 2: SCRAM-SHA-256 Compliance

**Objective:** Verify SCRAM-SHA-256 challenge-response authentication

**Steps:**
1. Create real users in bootstrap mode
2. Restart server
3. Connect with SCRAM-SHA-256
4. Monitor authentication exchange

**Expected Results:**
- Client receives salt from server
- Client sends proof
- Server verifies proof
- Connection established

### Scenario 3: HBA Rule Enforcement

**Objective:** Verify HBA rules block unauthorized connections

**Steps:**
1. Attempt connection from remote host (should fail)
2. Attempt connection with wrong auth method (should fail)
3. Attempt local connection with correct method (should succeed)

**Expected Results:**
- Remote connections rejected
- Wrong auth method rejected
- Local connections accepted

### Scenario 4: Rate Limiting and Lockout

**Objective:** Verify account lockout after failed attempts

**Steps:**
1. Attempt 5 failed logins
2. Attempt 6th login (should be locked)
3. Wait 5 minutes
4. Attempt login again (should succeed)

**Expected Results:**
- Account locks after 5 failures
- Correct password rejected during lockout
- Account unlocks after duration

### Scenario 5: GUI Tool Integration

**Objective:** Verify GUI tools can connect and browse schema

**Steps:**
1. Configure GUI with connection parameters
2. Connect to test server
3. Browse system catalogs
4. Execute queries via GUI

**Expected Results:**
- GUI connects successfully
- Schema browser populated
- Query execution works

## Configuration Reference

### Server Configuration (`test-server.conf`)

```ini
[server]
mode = single-database
database = /home/user/.scratchbird/testdb/testdb.sdb
pid_file = /home/user/.scratchbird/testdb/server.pid
auto_create = true

[network]
bind_address = 127.0.0.1
port = 13092
postgres_port = 0
mysql_port = 0
firebird_port = 0

[security]
hba_enabled = true
hba_file = /home/user/.scratchbird/testdb/scratchbird_hba.conf
default_auth_method = scram-sha-256
password_encryption = scram-sha-256
rate_limit_enabled = true
max_failed_attempts = 5
lockout_duration = 300

[logging]
level = info
```

### HBA Configuration (`scratchbird_hba.conf`)

**Format:**
```
connection_type database user address auth_method [auth_options]
```

**Connection Types:**
- `local` - Unix domain socket connections
- `host` - TCP/IP connections (SSL or non-SSL)
- `hostssl` - SSL-encrypted TCP connections
- `hostnossl` - Non-SSL TCP connections

**Auth Methods:**
- `trust` - Allow any user without authentication
- `reject` - Reject connection
- `scram-sha-256` - SCRAM-SHA-256 challenge-response
- `scram-sha-512` - SCRAM-SHA-512 challenge-response
- `password` - Plain password (not recommended)
- `md5` - MD5 hashed password (legacy)

## Troubleshooting

### Server Won't Start

**Check:**
- Port already in use: `lsof -i :3092`
- Database file permissions: `ls -la ~/.scratchbird/testdb/`
- PID file stale: `rm ~/.scratchbird/testdb/server.pid`
- Config syntax: Check for typos in `.conf` file

### Authentication Failures

**Check:**
- HBA rules matching: Verify connection type and address
- User exists: `SELECT * FROM system.users`
- Password hash: Verify SCRAM-SHA-256 format
- Rate limiting: Check for account lockout

### Connection Refused

**Check:**
- Server running: `ps aux | grep sb_server`
- Firewall rules: `iptables -L` or `ufw status`
- Bind address: Verify `127.0.0.1` not `localhost` resolution
- Port availability: `nc -z 127.0.0.1 3092`

## Security Considerations

⚠️ **WARNING: Test Server is for Development Only**

- **Never expose to public networks**
- **Default configuration binds to localhost only**
- **Bootstrap mode accepts any credentials**
- **Use strong passwords when creating real users**
- **Rotate test passwords regularly**

## Related Documentation

- **[Test Server Operations Guide](OPERATIONS.md)** - Detailed operational procedures
- **[Security Testing Procedures](SECURITY_TESTING.md)** - Security compliance testing
- **[Driver Testing Guide](DRIVER_TESTING.md)** - Driver development testing
- **[GUI Integration Guide](GUI_INTEGRATION.md)** - GUI tool integration

## See Also

- [Testing Specifications Index](../README.md)
- [Security Design Specification](../../Security%20Design%20Specification/)
- [Wire Protocols](../../wire_protocols/)
- [Alpha 3 Test Plan](../ALPHA3_TEST_PLAN.md)

---

**Last Updated:** February 2026  
**Version:** 1.0  
**Status:** Active
