# Getting Started

**Last Updated:** 2026-02-03

---

## Overview

This guide helps you install ScratchBird, connect for the first time, and run SQL queries.

---

## Quick Start

### 1. Choose Your Platform

| Platform | Guide |
|----------|-------|
| Linux | [installation/Linux.md](installation/Linux.md) |
| Windows | [installation/Windows.md](installation/Windows.md) |
| macOS | [installation/macOS.md](installation/macOS.md) |
| Docker | [installation/Docker.md](installation/Docker.md) |
| Kubernetes | [installation/Kubernetes.md](installation/Kubernetes.md) |

### 2. Start the Server

After installation, start the ScratchBird server:

```bash
# Linux (systemd)
sudo systemctl start scratchbird

# Foreground mode (development)
sb_server -F --config /etc/scratchbird/sb_server.conf
```

### 3. Connect

ScratchBird supports multiple protocols via listeners configured in `sb_server.conf`.
Common setups enable the native protocol and optional PostgreSQL compatibility.

Use a compatible client:

```bash
# Native protocol client
sb_isql -H localhost -p 3092 -U SYSARCH -P ScratchBirdBeta1! -d scratchbird

# PostgreSQL protocol client (if enabled)
psql -h localhost -p 5432 -U SYSARCH -d scratchbird
```

See [First Connection](getting-started/first-connection.md) for detailed instructions.

### 4. Run Your First Query

```sql
-- Create a table
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE
);

-- Insert data
INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com');

-- Query data
SELECT * FROM users;
```

See [Basic SQL](getting-started/basic-sql.md) for a complete tutorial.

---

## Connection Quick Reference

### Default Ports

Ports are configured per listener. Common defaults:

| Protocol | Port |
|----------|------|
| Native | 3092 |
| PostgreSQL | 5432 |

### Connection Strings

**Native:**
```
host=localhost port=3092 dbname=scratchbird user=SYSARCH password=ScratchBirdBeta1!
```

**PostgreSQL:**
```
postgresql://SYSARCH:ScratchBirdBeta1!@localhost:5432/scratchbird
```

### Default Credentials

| Setting | Default Value |
|---------|---------------|
| Username | `SYSARCH` |
| Password | `ScratchBirdBeta1!` |

---

## Verify Your Installation

```bash
# Check server status
sudo systemctl status scratchbird

# Check listening ports
ss -tlnp | grep -E '3092|5432'
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
