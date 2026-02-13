# Test Server Operations Guide

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**[← Back to Test Server Specification](README.md)**

## Overview

This guide covers the operational procedures for managing the ScratchBird Test Server, including startup, shutdown, configuration management, and maintenance tasks.

## Prerequisites

- ScratchBird built successfully (`build/src/sb_server` exists)
- User-space directories created (`~/.scratchbird/testdb/`)
- Configuration files in place

## Directory Structure

```
~/.scratchbird/testdb/
├── testdb.sdb              # Database file (auto-created)
├── test-server.conf        # Server configuration
├── scratchbird_hba.conf    # HBA (Host-Based Authentication) rules
├── server.pid              # PID file (auto-created)
└── logs/
    └── server.stdout       # Server output log
```

## Quick Start Commands

```bash
# Full setup (one-time)
./scripts/test-server-user.sh setup

# Start server
./scripts/test-server-user.sh start

# Check status
./scripts/test-server-user.sh status

# View logs
./scripts/test-server-user.sh logs

# Stop server
./scripts/test-server-user.sh stop

# Restart server
./scripts/test-server-user.sh restart
```

## Detailed Operations

### 1. Initial Setup

```bash
cd /path/to/ScratchBird

# Create directories and initial config
mkdir -p ~/.scratchbird/testdb/logs
mkdir -p build/ipc
mkdir -p build/run

# Create HBA configuration
cat > ~/.scratchbird/testdb/scratchbird_hba.conf << 'EOF'
# Local connections - SCRAM-SHA-256
local   all   all                 scram-sha-256

# IPv4 local connections - SCRAM-SHA-256
host    all   all   127.0.0.1/32  scram-sha-256

# IPv6 local connections - SCRAM-SHA-256
host    all   all   ::1/128       scram-sha-256

# Reject remote connections
host    all   all   0.0.0.0/0     reject
EOF

# Create server configuration
cat > ~/.scratchbird/testdb/test-server.conf << 'EOF'
[server]
mode = single-database
database = /home/USER/.scratchbird/testdb/testdb.sdb
pid_file = /home/USER/.scratchbird/testdb/server.pid
auto_create = true

[network]
bind_address = 127.0.0.1
port = 13092

[security]
hba_enabled = true
hba_file = /home/USER/.scratchbird/testdb/scratchbird_hba.conf
rate_limit_enabled = true
max_failed_attempts = 5
lockout_duration = 300

[logging]
level = info
EOF

# Replace USER with actual username
sed -i "s/USER/$(whoami)/g" ~/.scratchbird/testdb/test-server.conf
```

### 2. Starting the Server

#### Method 1: Using the Management Script (Recommended)

```bash
./scripts/test-server-user.sh start
```

**What it does:**
- Checks if server is already running
- Creates database if it doesn't exist
- Starts server in foreground mode (for debugging)
- Waits for server to be ready
- Shows connection information

#### Method 2: Manual Start (For Debugging)

```bash
cd /path/to/ScratchBird
./build/src/sb_server -F -c "$HOME/.scratchbird/testdb/test-server.conf"
```

**Options:**
- `-F` - Run in foreground (don't daemonize)
- `-c <file>` - Specify configuration file
- `--check` - Validate configuration and exit
- `-v` - Verbose logging

### 3. Stopping the Server

#### Method 1: Using the Management Script

```bash
./scripts/test-server-user.sh stop
```

#### Method 2: Manual Stop

```bash
# Find PID
pgrep -f "sb_server.*test-server.conf"

# Kill gracefully
kill <PID>

# Or force kill
pkill -9 -f "sb_server.*test-server.conf"
```

### 4. Checking Server Status

```bash
# Using script
./scripts/test-server-user.sh status

# Manual checks
ps aux | grep sb_server | grep -v grep
netstat -tlnp | grep -E "3092|5432|3050"
ls -la ~/.scratchbird/testdb/server.pid
ls -la build/ipc/scratchbird*.sock
```

### 5. Viewing Logs

```bash
# Real-time log tail
./scripts/test-server-user.sh logs

# Or manually
tail -f ~/.scratchbird/testdb/logs/server.stdout

# Filtered logs
tail -f ~/.scratchbird/testdb/logs/server.stdout | grep -E "AUTH|ERROR|WARN"

# Last 100 lines
tail -100 ~/.scratchbird/testdb/logs/server.stdout
```

## Configuration Management

### Modifying Server Configuration

1. **Edit configuration file:**
   ```bash
   nano ~/.scratchbird/testdb/test-server.conf
   ```

2. **Common configuration changes:**

   **Change port:**
   ```ini
   [network]
   port = 13093  # Change from default 3092
   ```

   **Disable HBA (bootstrap only):**
   ```ini
   [security]
   hba_enabled = false
   ```

   **Change log level:**
   ```ini
   [logging]
   level = debug  # info, warning, error
   ```

3. **Validate configuration:**
   ```bash
   ./build/src/sb_server --check -c "$HOME/.scratchbird/testdb/test-server.conf"
   ```

4. **Restart server to apply changes:**
   ```bash
   ./scripts/test-server-user.sh restart
   ```

### Modifying HBA Rules

1. **Edit HBA configuration:**
   ```bash
   nano ~/.scratchbird/testdb/scratchbird_hba.conf
   ```

2. **Example rule changes:**

   **Allow trust auth for local testing:**
   ```
   local   all   all                 trust
   host    all   all   127.0.0.1/32  trust
   ```

   **Add specific user exception:**
   ```
   local   all   testuser            trust
   host    all   admin    127.0.0.1/32  scram-sha-256
   ```

3. **Reload HBA without restart:**
   ```bash
   # Send SIGHUP to reload config
   kill -HUP $(cat ~/.scratchbird/testdb/server.pid)
   ```

## Maintenance Tasks

### Resetting the Database

**⚠️ WARNING: This deletes all data!**

```bash
# Stop server
./scripts/test-server-user.sh stop

# Remove database file
rm ~/.scratchbird/testdb/testdb.sdb

# Start server (will auto-create)
./scripts/test-server-user.sh start
```

### Cleaning Up Stale Files

```bash
# Remove stale PID file
rm -f ~/.scratchbird/testdb/server.pid

# Remove old logs
rm -f ~/.scratchbird/testdb/logs/server.stdout

# Clean up IPC sockets
rm -f build/ipc/scratchbird*.sock

# Clean up run files
rm -f build/run/*.pid
```

### Backup and Restore

**Backup database:**
```bash
# Stop server first
./scripts/test-server-user.sh stop

# Create backup
cp ~/.scratchbird/testdb/testdb.sdb \
   ~/.scratchbird/testdb/testdb.sdb.backup.$(date +%Y%m%d)

# Start server
./scripts/test-server-user.sh start
```

**Restore database:**
```bash
# Stop server
./scripts/test-server-user.sh stop

# Restore from backup
cp ~/.scratchbird/testdb/testdb.sdb.backup.20260206 \
   ~/.scratchbird/testdb/testdb.sdb

# Start server
./scripts/test-server-user.sh start
```

## Troubleshooting

### Server Fails to Start

**Symptom:** `./scripts/test-server-user.sh start` fails

**Checklist:**

1. **Port already in use:**
   ```bash
   lsof -i :3092
   # or
   netstat -tlnp | grep 3092
   ```
   **Solution:** Change port in config or kill existing process

2. **Permission denied:**
   ```bash
   ls -la ~/.scratchbird/testdb/
   ```
   **Solution:** Fix permissions: `chmod 755 ~/.scratchbird/testdb/`

3. **Stale PID file:**
   ```bash
   cat ~/.scratchbird/testdb/server.pid
   ps aux | grep $(cat ~/.scratchbird/testdb/server.pid)
   ```
   **Solution:** `rm ~/.scratchbird/testdb/server.pid`

4. **Configuration error:**
   ```bash
   ./build/src/sb_server --check -c "$HOME/.scratchbird/testdb/test-server.conf"
   ```
   **Solution:** Fix syntax errors in config file

### Connection Refused

**Symptom:** Cannot connect to server

**Checklist:**

1. **Server running:**
   ```bash
   ps aux | grep sb_server
   ```

2. **Correct port:**
   ```bash
   netstat -tlnp | grep sb_server
   ```

3. **Firewall blocking:**
   ```bash
   sudo iptables -L | grep 3092
   ```

4. **Wrong host:**
   - Must use `127.0.0.1`, not `localhost` (avoid DNS issues)
   - Check `bind_address` in config

### Authentication Failures

**Symptom:** Cannot authenticate even with correct credentials

**Checklist:**

1. **HBA rules:**
   ```bash
   cat ~/.scratchbird/testdb/scratchbird_hba.conf
   ```

2. **User exists:**
   - Connect with bootstrap mode
   - Query: `SELECT * FROM system.users`

3. **Account locked:**
   - Check logs for "rate limit" messages
   - Wait for lockout duration (default: 5 minutes)

4. **Password hash format:**
   - Must be bcrypt format: `$2a$10$...`

## Advanced Operations

### Running Multiple Test Servers

```bash
# Server 1 - Default
cp ~/.scratchbird/testdb/test-server.conf \
   ~/.scratchbird/testdb/test-server-1.conf

# Server 2 - Different port
sed -i 's/port = 3092/port = 3093/' \
    ~/.scratchbird/testdb/test-server-2.conf
sed -i 's/testdb.sdb/testdb-2.sdb/' \
    ~/.scratchbird/testdb/test-server-2.conf

# Start both
./build/src/sb_server -F -c "$HOME/.scratchbird/testdb/test-server-1.conf" &
./build/src/sb_server -F -c "$HOME/.scratchbird/testdb/test-server-2.conf" &
```

### Performance Tuning

```ini
[server]
shared_buffers = 256MB
max_connections = 200

[network]
port = 3092

[performance]
work_mem = 64MB
maintenance_work_mem = 256MB
```

### Enabling Debug Logging

```ini
[logging]
level = debug
log_connections = true
log_disconnections = true
log_authentication = true
```

## See Also

- [Test Server Specification](README.md) - Overview and configuration
- [Security Testing Procedures](SECURITY_TESTING.md) - Security testing
- [Driver Testing Guide](DRIVER_TESTING.md) - Driver development

---

**Last Updated:** February 2026
