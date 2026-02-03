# First Connection

**Last Updated:** 2026-02-03

---

## Overview

This guide walks you through connecting to ScratchBird for the first time.
Listeners and ports are configured in `sb_server.conf`; only enabled listeners
accept connections.

---

## Prerequisites

1. ScratchBird server is running
2. You know the host/port for the listener you enabled
3. You have valid credentials (default: `SYSARCH` / `ScratchBirdBeta1!`)

### Verify Server is Running

```bash
pgrep sb_server
sudo systemctl status scratchbird
```

### Check Listening Ports

```bash
ss -tlnp | grep -E '3092|5432'
```

---

## Native Connection (sb_isql)

Use the native client if it is installed with your distribution.

```bash
sb_isql -H localhost -p 3092 -U SYSARCH -P ScratchBirdBeta1! -d scratchbird
```

---

## PostgreSQL Protocol (psql)

If the PostgreSQL listener is enabled:

```bash
psql -h localhost -p 5432 -U SYSARCH -d scratchbird
```

---

## Connection Strings

**Native:**
```
host=localhost port=3092 dbname=scratchbird user=SYSARCH password=ScratchBirdBeta1!
```

**PostgreSQL:**
```
postgresql://SYSARCH:ScratchBirdBeta1!@localhost:5432/scratchbird
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
