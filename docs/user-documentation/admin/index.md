# Administration Guide

Server administration and maintenance tasks.

[Back to Documentation Index](../index.md)

---

## Administration Topics

| Guide | Description |
|-------|-------------|
| [User Management](user-management.md) | Users, roles, and permissions |
| [Backup & Restore](backup-restore.md) | Database backup and recovery |
| [Monitoring](monitoring.md) | Prometheus metrics and logging |
| [Security](security.md) | Security best practices |
| [Performance Tuning](performance-tuning.md) | Optimization techniques |
| [Troubleshooting](troubleshooting.md) | Common issues and solutions |

---

## Quick Reference

### Server Control

```bash
# Start/stop/restart
sudo systemctl start scratchbird
sudo systemctl stop scratchbird
sudo systemctl restart scratchbird

# Reload configuration (no downtime)
sudo systemctl reload scratchbird

# Check status
sudo systemctl status scratchbird
```

### View Logs

```bash
# Systemd journal
journalctl -u scratchbird -f

# Log file (if configured)
tail -f /var/log/scratchbird/sb_server.log
```

### Check Connections

```sql
SELECT
    usename,
    client_addr,
    state,
    query_start
FROM pg_stat_activity;
```

### Database Size

```sql
SELECT
    datname,
    pg_size_pretty(pg_database_size(datname)) AS size
FROM pg_database;
```

---

## Common Tasks

### Create Database

```sql
CREATE DATABASE mydb;
```

### Create User

```sql
CREATE USER myuser WITH PASSWORD 'secure_password';
GRANT ALL ON DATABASE mydb TO myuser;
```

### Take Backup

```bash
sb_backup create mydb /backup/mydb_$(date +%Y%m%d).sbdb
```

### Restore Backup

```bash
sb_backup restore /backup/mydb_20240115.sbdb restored_db
```

### Check Database Health

```bash
sb_verify mydb --all
```

---

## Administrative Tools

| Tool | Purpose |
|------|---------|
| `sb_server` | Database server daemon |
| `sb_isql` | Interactive SQL shell |
| `sb_backup` | Backup and restore |
| `sb_verify` | Database verification |
| `sb_security` | Security management |

---

## Maintenance Schedule

### Daily

- Monitor disk space
- Check error logs
- Verify backup completion

### Weekly

- Review slow query logs
- Check connection patterns
- Update statistics (`ANALYZE`)

### Monthly

- Review user access
- Test backup restoration
- Update server if patches available

---

## Getting Help

- [Troubleshooting guide](troubleshooting.md)
- [FAQ](../faq/index.md)
- [GitHub Issues](https://github.com/daltoncs/scratchbird/issues)
