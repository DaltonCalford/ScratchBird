# Test Server Deployment Guide

**Purpose:** Deploy a ScratchBird test server for other agents to use  
**Scope:** Production binaries only - NO test compilation  
**Target:** Localhost (127.0.0.1) on port 13092

---

## Quick Start (3 Steps)

### Step 1: Build Production Binaries

```bash
cd /opt/ScratchBird

# Option A: Build only production binaries (fast - 5-10 min)
./scripts/build-production.sh

# Option B: Build and install to /opt/scratchbird
./scripts/build-production.sh install
```

**What gets built:**
- `sb_server` - Main database server
- `sb_isql` - SQL client
- `sb_admin` - Administration utility
- `sb_backup` - Backup tool
- `sb_security` - User management

**What does NOT get built:**
- Test suites
- Test harnesses
- Benchmark tools
- Development utilities

---

### Step 2: Deploy Test Server

```bash
# Initial setup (run once)
sudo ./scripts/test-server-deploy.sh setup

# Start server
sudo ./scripts/test-server-deploy.sh start

# Check status
sudo ./scripts/test-server-deploy.sh status
```

---

### Step 3: Verify Connection

```bash
# Test with SYSARCH (full access)
/opt/ScratchBird/build/bin/sb_isql \
    --host=127.0.0.1 --port=13092 \
    --user=SYSARCH --password='SysArch2026!' \
    --query="SELECT 'Server is ready!' AS status;"
```

---

## Deployment Commands

| Command | Description |
|---------|-------------|
| `setup` | Initial setup (users, directories, certificates) |
| `start` | Start the test server |
| `stop` | Stop the test server |
| `restart` | Restart the test server |
| `status` | Show server status and connection info |
| `create-db` | Create/recreate test database |
| `drop-db` | Delete test database (destructive) |
| `logs` | View server logs in real-time |

---

## Connection Information

```
Host:     127.0.0.1
Port:     13092
Database: testdb
TLS:      Required (self-signed certificate)

Users:
  SYSARCH  / SysArch2026!    (Full DDL/DML access)
  TESTUSER / TestUser2026!   (DML only: SELECT/INSERT/UPDATE/DELETE)
```

### Connection Strings

**SYSARCH (Full Access):**
```
scratchbird://SYSARCH:SysArch2026!@127.0.0.1:13092/testdb
```

**TESTUSER (Application Testing):**
```
scratchbird://TESTUSER:TestUser2026!@127.0.0.1:13092/testdb
```

---

## Directory Structure

```
/var/scratchbird/
└── testdb/
    └── testdb.sdb          # Database file

/var/log/scratchbird/
└── testdb.log              # Server logs

/etc/scratchbird/
├── server.crt              # TLS certificate
├── server.key              # TLS private key
└── testdb.conf             # Server configuration
```

---

## Systemd Service

The server runs as a systemd service:

```bash
# Check status
sudo systemctl status scratchbird-test

# Manual control
sudo systemctl start scratchbird-test
sudo systemctl stop scratchbird-test
sudo systemctl restart scratchbird-test

# View logs
sudo journalctl -u scratchbird-test -f
```

---

## Security Testing

The two-user setup allows testing privilege separation:

```python
import scratchbird

# SYSARCH can do DDL
conn_admin = scratchbird.connect(
    host="127.0.0.1", port=13092,
    user="SYSARCH", password="SysArch2026!"
)
conn_admin.execute("CREATE TABLE test (id INT)")  # ✅ Works

# TESTUSER cannot do DDL
conn_app = scratchbird.connect(
    host="127.0.0.1", port=13092,
    user="TESTUSER", password="TestUser2026!"
)
conn_app.execute("CREATE TABLE hack (id INT)")  # ❌ Fails
conn_app.execute("SELECT * FROM users")         # ✅ Works
```

---

## Troubleshooting

### Server won't start

```bash
# Check for port conflict
sudo lsof -i :13092

# Check logs
sudo ./scripts/test-server-deploy.sh logs

# Check systemd
sudo systemctl status scratchbird-test
sudo journalctl -u scratchbird-test -n 50
```

### Cannot connect

```bash
# Verify server is running
sudo ./scripts/test-server-deploy.sh status

# Test with verbose output
/opt/ScratchBird/build/bin/sb_isql \
    --host=127.0.0.1 --port=13092 \
    --user=SYSARCH --password='SysArch2026!' \
    --verbose \
    --query="SELECT 1;"
```

### Binaries not found

```bash
# Tell the script where binaries are
export SB_BIN_DIR=/path/to/binaries
sudo -E ./scripts/test-server-deploy.sh start
```

---

## Complete Reset

To completely reset the test server:

```bash
# Stop server
sudo ./scripts/test-server-deploy.sh stop

# Delete database
sudo ./scripts/test-server-deploy.sh drop-db

# Recreate everything
sudo ./scripts/test-server-deploy.sh setup
sudo ./scripts/test-server-deploy.sh start
```

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Other Agents (Python, Go, Java, etc.)                      │
│  Connect via: scratchbird://127.0.0.1:13092                │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼ TLS 1.3
┌─────────────────────────────────────────────────────────────┐
│  sb_server (127.0.0.1:13092)                               │
│  - Multi-threaded request handling                          │
│  - SCRAM-SHA-256 authentication                             │
│  - Session management                                       │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼ IPC
┌─────────────────────────────────────────────────────────────┐
│  ScratchBird Engine                                         │
│  - MVCC transactions                                        │
│  - Storage engine                                           │
│  - Index management                                         │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼ File I/O
┌─────────────────────────────────────────────────────────────┐
│  /var/scratchbird/testdb/testdb.sdb                        │
│  - 16KB page size                                           │
│  - Encrypted (TLS at wire level)                            │
└─────────────────────────────────────────────────────────────┘
```

---

## Comparison with Old Setup

| Aspect | Old Setup | New Deployment |
|--------|-----------|----------------|
| Build tests? | Yes (slow) | No (fast) |
| CMake errors? | Yes (missing test files) | No |
| Binary location | Build directory | Configurable |
| Setup time | 30+ min | 5-10 min |
| Test suite | Built | Not needed |
| Focus | Development | Production |

---

**Ready to deploy!** Run: `sudo ./scripts/test-server-deploy.sh setup`
