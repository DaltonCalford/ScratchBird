# sb_server

ScratchBird database server daemon.

[Back to Tools Index](index.md) | [Back to Documentation Index](../index.md)

---

## Synopsis

```
sb_server [OPTIONS]
```

---

## Description

`sb_server` is the main ScratchBird database server process. It accepts connections from clients using multiple wire protocols (Native, PostgreSQL, MySQL, Firebird) and manages database files.

---

## Options

### Server Mode

| Option | Description |
|--------|-------------|
| `--config FILE` | Configuration file path |
| `--database PATH` | Single database file (single-database mode) |
| `--data-dir DIR` | Data directory (multi-database mode) |
| `--check` | Validate configuration and exit |

### Network

| Option | Description |
|--------|-------------|
| `--bind ADDR` | Bind address (default: 0.0.0.0) |
| `--native-port PORT` | Native protocol port (default: 3092) |
| `--pg-port PORT` | PostgreSQL port (default: 5432) |
| `--mysql-port PORT` | MySQL port (default: 3306) |
| `--fb-port PORT` | Firebird port (default: 3050) |

### Process

| Option | Description |
|--------|-------------|
| `--daemon` | Run as background daemon |
| `--pid-file FILE` | PID file location |
| `--user USER` | Run as specified user |
| `--group GROUP` | Run as specified group |

### Logging

| Option | Description |
|--------|-------------|
| `--log-level LEVEL` | Log level (debug, info, warning, error) |
| `--log-file FILE` | Log file path |

### Other

| Option | Description |
|--------|-------------|
| `--help` | Show help and exit |
| `--version` | Show version and exit |

---

## Usage Examples

### Foreground (Development)

```bash
# Single database
sb_server --database /path/to/mydb.sbdb

# Multi-database
sb_server --data-dir /var/lib/scratchbird
```

### With Configuration File

```bash
sb_server --config /etc/scratchbird/sb_server.conf
```

### Validate Configuration

```bash
sb_server --config /etc/scratchbird/sb_server.conf --check
```

### Custom Ports

```bash
sb_server --database /path/to/mydb.sbdb \
    --native-port 13092 \
    --pg-port 15432 \
    --mysql-port 0 \
    --fb-port 0
```

### As Daemon

```bash
sb_server --daemon \
    --config /etc/scratchbird/sb_server.conf \
    --pid-file /var/run/scratchbird/sb_server.pid \
    --user scratchbird \
    --group scratchbird
```

---

## Server Modes

### Single-Database Mode

One database file, all connections go to it:

```bash
sb_server --database /var/lib/scratchbird/mydb.sbdb
```

### Multi-Database Mode

Multiple databases in a directory:

```bash
sb_server --data-dir /var/lib/scratchbird
```

Each `.sbdb` file becomes a database. Connect by name:
```bash
sb_isql -H localhost -d mydb  # Uses mydb.sbdb
```

---

## Signals

| Signal | Action |
|--------|--------|
| `SIGHUP` | Reload configuration |
| `SIGTERM` | Graceful shutdown |
| `SIGINT` | Graceful shutdown |
| `SIGQUIT` | Immediate shutdown |

```bash
# Reload config
kill -HUP $(cat /var/run/scratchbird/sb_server.pid)

# Graceful stop
kill -TERM $(cat /var/run/scratchbird/sb_server.pid)
```

---

## Systemd Integration

The server integrates with systemd:

- **Type=notify** - Reports when ready
- **Watchdog** - Periodic health checks
- **Socket activation** - Optional

### Service Commands

```bash
sudo systemctl start scratchbird
sudo systemctl stop scratchbird
sudo systemctl restart scratchbird
sudo systemctl reload scratchbird  # Reload config
sudo systemctl status scratchbird
```

### View Logs

```bash
journalctl -u scratchbird -f
```

---

## File Locations

| File | Default Path |
|------|--------------|
| Configuration | `/etc/scratchbird/sb_server.conf` |
| Data directory | `/var/lib/scratchbird/` |
| PID file | `/var/run/scratchbird/sb_server.pid` |
| Log file | `/var/log/scratchbird/sb_server.log` |
| Unix socket | `/var/run/scratchbird/sb.sock` |

---

## Startup Sequence

1. Read configuration file
2. Initialize logging
3. Open data directory or database file
4. Bind to network ports
5. Start worker threads
6. Accept connections

---

## Shutdown Sequence

1. Stop accepting new connections
2. Wait for active queries to complete (timeout)
3. Close client connections
4. Flush dirty buffers
5. Close database files
6. Exit

---

## Resource Limits

Configure in systemd unit or shell:

```bash
# Max open files
ulimit -n 65536

# Max processes
ulimit -u 4096
```

Or in systemd unit:
```ini
[Service]
LimitNOFILE=65536
LimitNPROC=4096
```

---

## Troubleshooting

### Won't Start

```bash
# Check config
sb_server --config /etc/scratchbird/sb_server.conf --check

# Check logs
journalctl -u scratchbird -n 50

# Check port in use
ss -tlnp | grep 5432
```

### Performance Issues

```bash
# Check connections
sb_isql -c "SELECT COUNT(*) FROM pg_stat_activity"

# Check memory
ps aux | grep sb_server
```

---

## See Also

- [Configuration Reference](../configuration/sb_server.conf.md)
- [sb_isql](sb-isql.md)
- [Monitoring](../admin/monitoring.md)
