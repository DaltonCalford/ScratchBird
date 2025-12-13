# Environment Variables

Runtime configuration through environment variables.

[Back to Configuration Index](index.md) | [Back to Documentation Index](../index.md)

---

## Overview

Environment variables provide an alternative to configuration files, useful for:
- Docker containers
- CI/CD pipelines
- Temporary overrides
- Secrets management

---

## Server Variables

### Core Settings

| Variable | Description | Default |
|----------|-------------|---------|
| `SCRATCHBIRD_DATA_DIR` | Database file directory | `/var/lib/scratchbird` |
| `SCRATCHBIRD_CONFIG` | Configuration file path | `/etc/scratchbird/sb_server.conf` |
| `SCRATCHBIRD_LOG_LEVEL` | Log level (debug, info, warning, error) | `info` |
| `SCRATCHBIRD_LOG_FILE` | Log file path | stdout |

### Network Settings

| Variable | Description | Default |
|----------|-------------|---------|
| `SCRATCHBIRD_BIND_ADDRESS` | Listen address | `0.0.0.0` |
| `SCRATCHBIRD_NATIVE_PORT` | Native protocol port | `3092` |
| `SCRATCHBIRD_PG_PORT` | PostgreSQL protocol port | `5432` |
| `SCRATCHBIRD_MYSQL_PORT` | MySQL protocol port | `3306` |
| `SCRATCHBIRD_FB_PORT` | Firebird protocol port | `3050` |

### Connection Limits

| Variable | Description | Default |
|----------|-------------|---------|
| `SCRATCHBIRD_MAX_CONNECTIONS` | Maximum connections | `100` |
| `SCRATCHBIRD_IDLE_TIMEOUT` | Idle connection timeout (seconds) | `3600` |
| `SCRATCHBIRD_STATEMENT_TIMEOUT` | Query timeout (milliseconds) | `0` |

### Memory Settings

| Variable | Description | Default |
|----------|-------------|---------|
| `SCRATCHBIRD_SHARED_BUFFERS` | Buffer pool size | `128MB` |
| `SCRATCHBIRD_WORK_MEM` | Per-operation memory | `4MB` |

### SSL Settings

| Variable | Description | Default |
|----------|-------------|---------|
| `SCRATCHBIRD_SSL_ENABLED` | Enable SSL | `false` |
| `SCRATCHBIRD_SSL_CERT` | Certificate file path | - |
| `SCRATCHBIRD_SSL_KEY` | Key file path | - |

### Authentication

| Variable | Description | Default |
|----------|-------------|---------|
| `SCRATCHBIRD_ADMIN_PASSWORD` | Initial admin password | - |
| `SCRATCHBIRD_AUTH_METHOD` | Authentication method | `scram-sha-256` |

---

## Client Variables

### Connection Defaults

| Variable | Description | Default |
|----------|-------------|---------|
| `SBHOST` | Default server host | `localhost` |
| `SBPORT` | Default server port | `3092` |
| `SBUSER` | Default username | - |
| `SBPASSWORD` | Default password | - |
| `SBDATABASE` | Default database | - |

### PostgreSQL-Compatible Variables

For compatibility with PostgreSQL tools:

| Variable | Description |
|----------|-------------|
| `PGHOST` | Server host |
| `PGPORT` | Server port |
| `PGUSER` | Username |
| `PGPASSWORD` | Password |
| `PGDATABASE` | Database name |
| `PGSSLMODE` | SSL mode |

---

## Using Environment Variables

### Shell Export

```bash
# Export variables
export SCRATCHBIRD_MAX_CONNECTIONS=200
export SCRATCHBIRD_LOG_LEVEL=debug

# Start server
sb_server
```

### Systemd Service

Edit `/etc/systemd/system/scratchbird.service`:

```ini
[Service]
Environment="SCRATCHBIRD_MAX_CONNECTIONS=200"
Environment="SCRATCHBIRD_LOG_LEVEL=info"
```

Or use environment file:

```ini
[Service]
EnvironmentFile=/etc/scratchbird/scratchbird.env
```

Create `/etc/scratchbird/scratchbird.env`:

```bash
SCRATCHBIRD_MAX_CONNECTIONS=200
SCRATCHBIRD_LOG_LEVEL=info
SCRATCHBIRD_ADMIN_PASSWORD=secure_password_here
```

### Docker

```bash
docker run -d \
    -e SCRATCHBIRD_MAX_CONNECTIONS=200 \
    -e SCRATCHBIRD_ADMIN_PASSWORD=secret \
    -p 5432:5432 \
    scratchbird:latest
```

Or with docker-compose:

```yaml
services:
  scratchbird:
    image: scratchbird:latest
    environment:
      - SCRATCHBIRD_MAX_CONNECTIONS=200
      - SCRATCHBIRD_ADMIN_PASSWORD=${DB_PASSWORD}
    env_file:
      - ./scratchbird.env
```

---

## Variable Precedence

When the same setting is specified multiple ways:

1. **Command-line arguments** (highest priority)
2. **Environment variables**
3. **Configuration file**
4. **Built-in defaults** (lowest priority)

Example:

```bash
# Config file says: max_connections = 100
# Environment says: SCRATCHBIRD_MAX_CONNECTIONS=200
# Command line says: --max-connections=300

# Result: 300 (command-line wins)
```

---

## Configuration File References

Use environment variables in configuration files:

```ini
[server]
data_dir = ${SCRATCHBIRD_DATA_DIR}
max_connections = ${SCRATCHBIRD_MAX_CONNECTIONS:-100}

[authentication]
# Secret from environment
admin_password = ${ADMIN_PASSWORD}
```

The `:-100` syntax provides a default value if the variable is not set.

---

## Docker-Specific Variables

### Container Initialization

| Variable | Description |
|----------|-------------|
| `SCRATCHBIRD_ADMIN_PASSWORD` | Set admin password on first run |
| `SCRATCHBIRD_INIT_SQL` | SQL to run on initialization |
| `SCRATCHBIRD_CREATE_DB` | Database to create on startup |

### Volume Paths

```bash
docker run -d \
    -e SCRATCHBIRD_DATA_DIR=/data \
    -v scratchbird-data:/data \
    scratchbird:latest
```

---

## Client Tool Variables

### sb_isql

```bash
# Set defaults
export SBHOST=db.example.com
export SBPORT=5432
export SBUSER=myuser

# Now connects without specifying each time
sb_isql mydb
```

### psql Compatibility

```bash
# PostgreSQL variables work too
export PGHOST=localhost
export PGPORT=5432
export PGUSER=admin
export PGDATABASE=mydb

# psql connects using these
psql
```

### Password Files

For security, avoid `PGPASSWORD`. Use `.pgpass` file instead:

```bash
# Create ~/.pgpass
echo "localhost:5432:mydb:admin:secret" > ~/.pgpass
chmod 600 ~/.pgpass
```

---

## Security Considerations

### Sensitive Variables

**Never expose passwords in:**
- Docker inspect output
- Process listings (`ps aux`)
- CI/CD logs

**Better approaches:**

1. **Secrets files:**
```bash
# Use file reference
docker run -d \
    -e SCRATCHBIRD_ADMIN_PASSWORD_FILE=/run/secrets/db_password \
    -v ./db_password:/run/secrets/db_password:ro \
    scratchbird:latest
```

2. **Docker secrets:**
```yaml
services:
  scratchbird:
    secrets:
      - db_password
    environment:
      - SCRATCHBIRD_ADMIN_PASSWORD_FILE=/run/secrets/db_password

secrets:
  db_password:
    file: ./db_password.txt
```

3. **Vault integration:**
```bash
export SCRATCHBIRD_ADMIN_PASSWORD=$(vault read -field=password secret/scratchbird)
```

### Audit Environment

Check what's exposed:

```bash
# What environment can the process see?
cat /proc/$(pgrep sb_server)/environ | tr '\0' '\n' | grep SCRATCHBIRD
```

---

## Debugging

### View Effective Configuration

```bash
# Server startup logs show effective config
journalctl -u scratchbird | grep -i config
```

### Test Variable Expansion

```bash
# Check if variable is set
echo $SCRATCHBIRD_MAX_CONNECTIONS

# Check in context
env | grep SCRATCHBIRD
```

---

## Common Patterns

### Development

```bash
export SCRATCHBIRD_LOG_LEVEL=debug
export SCRATCHBIRD_DATA_DIR=$HOME/.scratchbird/data
export SCRATCHBIRD_MAX_CONNECTIONS=10
```

### Production (Systemd)

```ini
# /etc/scratchbird/scratchbird.env
SCRATCHBIRD_MAX_CONNECTIONS=500
SCRATCHBIRD_SHARED_BUFFERS=4GB
SCRATCHBIRD_LOG_LEVEL=warning
```

### CI/CD Testing

```yaml
# GitHub Actions
env:
  SCRATCHBIRD_ADMIN_PASSWORD: ${{ secrets.DB_PASSWORD }}
  SCRATCHBIRD_DATA_DIR: /tmp/testdb
```

---

## Next Steps

- [Server configuration reference](sb_server.conf.md)
- [Docker deployment](../installation/docker.md)
- [Security best practices](../admin/security.md)
