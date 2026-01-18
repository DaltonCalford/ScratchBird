# Command-Line Tools

ScratchBird command-line utilities reference.

[Back to Home](../Home.md)

---

## Tool Overview

| Tool | Description |
|------|-------------|
| [sb_server](sb-server.md) | Database server daemon |
| [sb_isql](sb-isql.md) | Interactive SQL shell |
| [sb_fb_isql](sb-fb-isql.md) | Firebird SQL shell (emulation) |
| [sb_pg_isql](sb-pg-isql.md) | PostgreSQL wire protocol shell (emulation) |
| [sb_my_isql](sb-my-isql.md) | MySQL wire protocol shell (emulation) |
| [sb_backup](sb-backup.md) | Backup and restore utility |
| [sb_verify](sb-verify.md) | Database verification |
| [sb_security](sb-security.md) | Security management |
| [sb_admin](sb-admin.md) | Server administration |

---

## Quick Reference

### Start Server

```bash
# Foreground
sb_server --database /var/lib/scratchbird

# As service
sudo systemctl start scratchbird
```

### Connect

```bash
sb_isql -H localhost -p 3092 -U admin mydb
```

### Backup

```bash
sb_backup create mydb /backup/mydb.sbdb
```

### Restore

```bash
sb_backup restore /backup/mydb.sbdb restored_db
```

### Verify

```bash
sb_verify mydb --all
```

### User Management

```bash
sb_security create-user newuser
sb_security password newuser
```

---

## Common Options

Most tools share these options:

| Option | Description |
|--------|-------------|
| `-H, --host` | Server hostname |
| `-P, --port` | Server port |
| `-U, --user` | Username |
| `-p, --password` | Password (or prompt) |
| `-d, --database` | Database name |
| `-v, --verbose` | Verbose output |
| `-q, --quiet` | Minimal output |
| `--help` | Show help |
| `--version` | Show version |

---

## Environment Variables

Tools read these environment variables:

| Variable | Description |
|----------|-------------|
| `SBHOST` | Default host |
| `SBPORT` | Default port |
| `SBUSER` | Default username |
| `SBDATABASE` | Default database |

PostgreSQL-compatible variables also work:
- `PGHOST`, `PGPORT`, `PGUSER`, `PGDATABASE`

---

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | General error |
| 2 | Connection error |
| 3 | Authentication error |
| 4 | Permission error |
| 5 | Not found |

---

## Getting Help

```bash
# Show help for any tool
sb_isql --help
sb_backup --help
sb_verify --help
```
