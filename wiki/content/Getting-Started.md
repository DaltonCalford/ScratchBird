# Getting Started

**Last Updated:** 2026-01-30

---

## Overview

This guide helps you install ScratchBird, connect for the first time, and start running SQL queries. Choose your platform and follow the steps to get up and running.

---

## Quick Start

### 1. Choose Your Platform

| Platform | Guide | Recommended For |
|----------|-------|-----------------|
| [Linux](installation/Linux.md) | Build from source or Docker | Production, development |
| [Windows](installation/Windows.md) | Docker/WSL2 or Visual Studio | Development |
| [macOS](installation/macOS.md) | Docker or Homebrew/source | Development |
| [Kubernetes](installation/Kubernetes.md) | Helm or manifests | Cloud deployments |
| [Docker](installation/Linux.md#method-2-docker-container) | Any platform | Quick evaluation |

### 2. Start the Server

After installation, start the ScratchBird server:

```bash
# Linux/macOS with systemd
sudo systemctl start scratchbird

# Foreground mode (development)
sb_server -F --config /etc/scratchbird/sb_server.conf

# Docker
docker run -d -p 3092:3092 -p 5432:5432 scratchbird/scratchbird:latest
```

### 3. Connect

ScratchBird supports multiple connection protocols:

| Protocol | Port | Client |
|----------|------|--------|
| Native | 3092 | `sb_isql` |
| PostgreSQL | 5432 | `psql`, JDBC, psycopg2 |
| MySQL | 3306 | `mysql`, MySQL Connector |
| Firebird | 3050 | Firebird clients |

**Using the native client:**
```bash
sb_isql -H localhost -p 3092 -U admin -d scratchbird
```

**Using psql:**
```bash
psql -h localhost -p 5432 -U admin -d scratchbird
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

## Getting Started Guides

### Installation

Step-by-step installation guides for each platform:

- [Linux Installation](installation/Linux.md) - Ubuntu, Debian, Fedora, RHEL, Arch
- [Windows Installation](installation/Windows.md) - Docker/WSL2 or Visual Studio build
- [macOS Installation](installation/macOS.md) - Docker or Homebrew/source build
- [Kubernetes Installation](installation/Kubernetes.md) - Helm charts and manifests

### First Steps

After installation, follow these guides:

- [First Connection](getting-started/first-connection.md) - Connect using various clients
- [Basic SQL](getting-started/basic-sql.md) - CRUD operations tutorial

---

## Connection Quick Reference

### Default Ports

| Protocol | Port | Description |
|----------|------|-------------|
| Native | 3092 | ScratchBird native protocol |
| PostgreSQL | 5432 | PostgreSQL wire protocol |
| MySQL | 3306 | MySQL wire protocol |
| Firebird | 3050 | Firebird wire protocol |
| Metrics | 9090 | Prometheus metrics (optional) |

### Connection Strings

**Native:**
```
host=localhost port=3092 dbname=scratchbird user=admin password=secret
```

**PostgreSQL:**
```
postgresql://admin:secret@localhost:5432/scratchbird
```

**MySQL:**
```
mysql://admin:secret@localhost:3306/scratchbird
```

### Default Credentials

| Setting | Default Value |
|---------|---------------|
| Username | `admin` |
| Database | `scratchbird` |
| Data directory | `/var/lib/scratchbird` |
| Config file | `/etc/scratchbird/sb_server.conf` |

---

## Verify Your Installation

Run these commands to verify ScratchBird is working:

```bash
# Check server status
sudo systemctl status scratchbird  # Linux with systemd

# Check listening ports
ss -tlnp | grep -E '3092|5432|3306'

# Connect and run a test query
sb_isql -H localhost -p 3092 -U admin -c "SELECT version()"
```

Expected output:
```
                  version
--------------------------------------------
 ScratchBird Alpha on Linux x86_64
(1 row)
```

---

## Common Tasks

### Create a Database

```sql
CREATE DATABASE myapp;
```

### Create a Table

```sql
CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    price DECIMAL(10, 2)
);
```

### Insert Data

```sql
INSERT INTO products (name, price) VALUES ('Widget', 19.99);
```

### Query Data

```sql
SELECT * FROM products WHERE price > 10;
```

---

## Troubleshooting

### Server Won't Start

```bash
# Check configuration
sb_server --config /etc/scratchbird/sb_server.conf --check

# Check if ports are in use
ss -tlnp | grep 3092

# View logs
sudo journalctl -u scratchbird -n 50
```

### Connection Refused

```bash
# Verify server is running
pgrep sb_server

# Check firewall
sudo ufw status  # Ubuntu
sudo firewall-cmd --list-all  # RHEL/Fedora
```

### Authentication Failed

```bash
# Check user exists
sb_isql -H localhost -U admin -c "SELECT * FROM sb_catalog.users"
```

See platform-specific troubleshooting in each installation guide.

---

## Next Steps

After completing the getting started guide:

### Learn SQL
- [Native SQL Guide](language-guides/native/README.md) - Complete SQL reference
- [Data Types](reference/Data-Types.md) - Supported data types
- [Functions](reference/Functions.md) - Built-in functions

### Build Applications
- [Tutorials](tutorials/README.md) - Step-by-step application guides
- [Driver Documentation](drivers/Driver-Comparison.md) - Language-specific drivers
  - If you installed ScratchBird with the installer, you can add driver packs later via:
    `sb_setup --interactive` (select the driver packages you want)

### Administration
- [CLI Tools](cli-tools/README.md) - Command-line utilities
- [Backup and Restore](admin/backup-restore.md) - Data protection
- [Security](admin/security.md) - Authentication and TLS

### Development
- [Developer Guide](developer-guide/README.md) - Internal architecture
- [Contributing](Contributing.md) - How to contribute

---

## Getting Help

- **Documentation:** Browse the [wiki](Home.md)
- **Issues:** Report bugs at [GitHub Issues](https://github.com/scratchbird/scratchbird/issues)
- **Discussions:** Ask questions in [GitHub Discussions](https://github.com/scratchbird/scratchbird/discussions)
