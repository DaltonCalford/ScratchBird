# Configuration Guide

Configure ScratchBird for your environment.

[Back to Documentation Index](../index.md)

---

## Configuration Files

ScratchBird uses several configuration files:

| File | Purpose | Location |
|------|---------|----------|
| `sb_server.conf` | Main server configuration | `/etc/scratchbird/` |
| `hba.conf` | Host-based authentication | `/etc/scratchbird/` |
| SSL certificates | TLS encryption | `/etc/scratchbird/ssl/` |
| `.scratchbird.yml` | Git integration config (when enabled) | Repository root |

---

## Configuration Guides

| Guide | Description |
|-------|-------------|
| [sb_server.conf Reference](sb_server.conf.md) | Complete server configuration reference |
| [Host-Based Authentication](hba.conf.md) | Connection security rules |
| [SSL/TLS Setup](ssl-setup.md) | Configure encrypted connections |
| [Environment Variables](environment-vars.md) | Runtime configuration options |

---

## Quick Start

### Locate Configuration

```bash
# Default location
ls /etc/scratchbird/

# Or check where server is looking
sb_server --config-path
```

### Edit Configuration

```bash
sudo nano /etc/scratchbird/sb_server.conf
```

### Apply Changes

```bash
# Reload configuration (no restart needed for most settings)
sudo systemctl reload scratchbird

# Or restart for all settings
sudo systemctl restart scratchbird
```

---

## Configuration Syntax

The configuration file uses INI-style syntax:

```ini
# Comments start with # or ;
; This is also a comment

[section]
# Key-value pairs
key = value

# Values can be quoted
path = "/path/with spaces/file"

# Environment variables
data_dir = ${SCRATCHBIRD_DATA_DIR}

# Include other files
@include /etc/scratchbird/local.conf
```

---

## Configuration Sections

| Section | Purpose |
|---------|---------|
| `[server]` | Server mode, data directory, limits |
| `[network]` | Ports, bind address, socket settings |
| `[ssl]` | TLS/SSL certificates and options |
| `[memory]` | Buffer sizes and memory allocation |
| `[authentication]` | Auth methods and password policies |
| `[logging]` | Log levels and destinations |
| `[statistics]` | Metrics and Prometheus export |

---

## Common Configuration Tasks

### Change Listening Ports

```ini
[network]
native_port = 3092
pg_port = 5432
mysql_port = 3306
fb_port = 3050
```

### Enable SSL

```ini
[ssl]
enabled = true
cert_file = /etc/scratchbird/ssl/server.crt
key_file = /etc/scratchbird/ssl/server.key
```

### Increase Connections

```ini
[server]
max_connections = 200
```

### Enable Prometheus Metrics

```ini
[statistics]
enabled = true
export = prometheus
prometheus_port = 9090
```

---

## Verifying Configuration

### Check Syntax

```bash
sb_server --config /etc/scratchbird/sb_server.conf --check
```

### View Effective Configuration

```sql
-- In sb_isql
SHOW ALL;

-- Specific setting
SHOW max_connections;
```

---

## Default Values

If a setting is not specified, defaults are used:

| Setting | Default |
|---------|---------|
| `mode` | `multi-database` |
| `data_dir` | `/var/lib/scratchbird` |
| `max_connections` | `100` |
| `native_port` | `3092` |
| `pg_port` | `5432` |
| `mysql_port` | `3306` |
| `fb_port` | `3050` |
| `log_level` | `info` |
| `ssl.enabled` | `false` |

---

## Best Practices

1. **Keep a backup** of working configuration
2. **Test changes** in development first
3. **Use `reload`** instead of `restart` when possible
4. **Document changes** with comments
5. **Use environment variables** for secrets

---

## Upgrade Notes

Git integration configuration keys were normalized in the Git spec to
`repository.repo_*`. The current parser still expects legacy keys such as
`repository.url` and `repository.branch` until the config parser update lands.
Plan to migrate to the canonical names when support is implemented.

---

## Next Steps

- [Complete sb_server.conf reference](sb_server.conf.md)
- [Set up authentication rules](hba.conf.md)
- [Enable SSL/TLS](ssl-setup.md)
