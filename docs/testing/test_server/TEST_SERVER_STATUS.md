# ScratchBird Test Server - Security Testing Configuration

## Server Status: RUNNING ✅

**PID:** Check with `pgrep -f "sb_server.*test-server.conf"`  
**Database:** `/home/dcalford/.scratchbird/testdb/testdb.sdb`  
**Config:** `/home/dcalford/.scratchbird/testdb/test-server.conf`  
**HBA Config:** `/home/dcalford/.scratchbird/testdb/scratchbird_hba.conf`  

---

## Authentication Modes

The test server supports **two authentication modes** for comprehensive security testing:

### Mode 1: Bootstrap Authentication (Current/Default)
**Use this for initial setup and testing basic connectivity.**

When the database has no real users (only SYSTEM user exists), the server allows **bootstrap authentication**:
- Any username is accepted
- Any password is accepted  
- User is logged in as SYSTEM (superuser)
- Intended for initial user creation

**Test Credentials (Bootstrap Mode):**
| Username | Password | Result |
|----------|----------|--------|
| Any name | Any pass | ✅ Authenticated as SYSTEM |

### Mode 2: SCRAM-SHA-256 Authentication (For Security Testing)
**Use this for full security compliance testing.**

To enable real authentication:
1. First connect using bootstrap mode to create real users
2. Then restart with HBA rules enforced

---

## Connection Information

### Unix Socket (Recommended for Local Testing)
- **Path:** `build/ipc/scratchbird-homedcalfordscratchbirdtestdbtestdbsdb.sock`
- **Auth:** Bootstrap mode (current) / SCRAM-SHA-256 (when HBA enabled)

### TCP Native Protocol
- **Host:** `127.0.0.1`
- **Port:** `3092`
- **Auth:** Bootstrap mode (current) / SCRAM-SHA-256 (when HBA enabled)

### PostgreSQL Protocol
- **Host:** `127.0.0.1`
- **Port:** `5432`
- **Auth:** Bootstrap mode (current) / SCRAM-SHA-256 (when HBA enabled)

### Firebird Protocol
- **Host:** `127.0.0.1`
- **Port:** `3050`
- **Auth:** Bootstrap mode (current) / SCRAM-SHA-256 (when HBA enabled)

---

## Security Testing Scenarios

### Scenario A: Bootstrap Authentication Testing
Test that drivers/GUI handle bootstrap mode correctly:
```
Host: 127.0.0.1
Port: 3092
Username: testuser
Password: anypassword
Expected: Connection accepted (logged in as SYSTEM)
```

### Scenario B: HBA Rule Enforcement Testing
When HBA is enabled (see below), test these rejection scenarios:
```
Host: 192.168.1.1 (remote)
Port: 3092
Expected: Connection rejected (HBA rules block non-local)
```

### Scenario C: SCRAM-SHA-256 Authentication Testing
Test SCRAM-SHA-256 challenge-response authentication:
```
Host: 127.0.0.1
Port: 3092
Auth Method: SCRAM-SHA-256
Expected: Salt exchange + proof verification
```

---

## Enabling Full Security Mode (SCRAM-SHA-256)

To switch from bootstrap mode to real authentication:

1. **Create real users** (connect via bootstrap first):
   ```sql
   CREATE USER testuser PASSWORD 'TestPass2026!';
   CREATE USER admin PASSWORD 'AdminPass2026!' SUPERUSER;
   ```

2. **Enable HBA configuration**:
   ```bash
   # Edit the server config
   cat > ~/.scratchbird/testdb/test-server.conf << 'CONF'
   [server]
   mode = single-database
   database = /home/dcalford/.scratchbird/testdb/testdb.sdb
   pid_file = /home/dcalford/.scratchbird/testdb/server.pid
   
   [network]
   bind_address = 127.0.0.1
   port = 13092
   
   [security]
   hba_enabled = true
   hba_file = /home/dcalford/.scratchbird/testdb/scratchbird_hba.conf
   CONF
   ```

3. **Restart server**:
   ```bash
   pkill -f "sb_server.*test-server.conf"
   ./build/src/sb_server -F -c "$HOME/.scratchbird/testdb/test-server.conf"
   ```

4. **Test with real credentials**:
   - Username: `testuser`
   - Password: `TestPass2026!`

---

## HBA Configuration

Current HBA rules (`scratchbird_hba.conf`):
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

---

## Test Credentials Summary

| Mode | Username | Password | Description |
|------|----------|----------|-------------|
| Bootstrap | Any | Any | Initial setup mode (current) |
| SCRAM | testuser | TestPass2026! | Standard user (after creation) |
| SCRAM | admin | AdminPass2026! | Superuser (after creation) |
| SCRAM | SYSDBA | SYSDBA2026! | Firebird compatibility (after creation) |

---

## Security Features Available for Testing

| Feature | Status | Description |
|---------|--------|-------------|
| Bootstrap Auth | ✅ Active | Any user/pass accepted when no real users |
| SCRAM-SHA-256 | ✅ Available | Challenge-response authentication |
| HBA Rules | ✅ Configured | Host-based access control |
| Rate Limiting | ✅ Available | Configurable failed attempt limits |
| Account Lockout | ✅ Available | Auto-lock after failed attempts |
| Password Policy | ✅ Available | Configurable complexity requirements |

---

## Log Locations

```bash
# Server logs
tail -f ~/.scratchbird/testdb/logs/server.stdout

# HBA/auth events
# (Logged to server stdout with [AUTH] prefix)
```

---

## Quick Test Commands

```bash
# Check server is running
ps aux | grep sb_server

# Test TCP port
nc -z 127.0.0.1 3092 && echo "Native protocol OK"
nc -z 127.0.0.1 5432 && echo "PostgreSQL protocol OK"
nc -z 127.0.0.1 3050 && echo "Firebird protocol OK"

# Test Unix socket
ls -la build/ipc/scratchbird*.sock

# View logs
tail -f ~/.scratchbird/testdb/logs/server.stdout
```

---

## For Driver/GUI Testing

### Connection Strings

**Bootstrap Mode (Current):**
```
scratchbird://anyuser:anypass@127.0.0.1:3092/testdb
postgresql://anyuser:anypass@127.0.0.1:5432/testdb
firebird://anyuser:anypass@127.0.0.1:3050/testdb
```

**SCRAM-SHA-256 Mode (After user creation):**
```
scratchbird://testuser:TestPass2026!@127.0.0.1:3092/testdb
postgresql://admin:AdminPass2026!@127.0.0.1:5432/testdb
firebird://SYSDBA:SYSDBA2026!@127.0.0.1:3050/testdb
```

### Testing Checklist

- [ ] Connect with bootstrap credentials (any user/pass)
- [ ] Verify SCRAM-SHA-256 challenge-response flow
- [ ] Test HBA rejection of remote connections
- [ ] Test rate limiting after 5 failed attempts
- [ ] Test account lockout mechanisms
- [ ] Verify password complexity enforcement
- [ ] Test TLS/SSL connection encryption
- [ ] Verify audit logging of authentication events

---

**Note:** The server is currently in bootstrap mode. For full security compliance testing, create real users and restart with HBA enabled as described above.
