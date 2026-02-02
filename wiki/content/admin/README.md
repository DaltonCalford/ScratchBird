# Administration Guide

**Last Updated:** 2026-01-30

---

## Overview

This section covers database administration tasks for ScratchBird. Whether you're setting up a new installation, managing users, or troubleshooting production issues, these guides provide the information you need.

---

## Quick Links

| Guide | Description |
|-------|-------------|
| [Backup and Restore](backup-restore.md) | Backup strategies, sb_backup/sb_restore, disaster recovery |
| [Monitoring](monitoring.md) | Metrics, Prometheus, Grafana, alerting |
| [Security](security.md) | Authentication, TLS, authorization, auditing |
| [User Management](user-management.md) | Users, roles, permissions, DCL |
| [Troubleshooting](troubleshooting.md) | Common issues and solutions |

---

## Getting Started

### First-Time Setup

1. **[Install ScratchBird](../installation/)** - Platform-specific installation guides
2. **[Configure Security](security.md)** - Set up authentication and TLS
3. **[Create Users](user-management.md)** - Set up application and admin users
4. **[Configure Monitoring](monitoring.md)** - Set up metrics and alerting
5. **[Set Up Backups](backup-restore.md)** - Configure automated backups

### Essential Configuration

Edit `/etc/scratchbird/sb_server.conf`:

```ini
[server]
mode = multi-database
data_dir = /var/lib/scratchbird
max_connections = 100

[network]
bind_address = 0.0.0.0
native_port = 3092
pg_port = 5432

[ssl]
enabled = true
cert_file = /etc/scratchbird/certs/server.crt
key_file = /etc/scratchbird/certs/server.key

[logging]
level = info
log_connections = true
```

---

## Daily Operations

### Health Check

```sql
-- Quick health check
SELECT
    'Connections' AS metric,
    COUNT(*)::text AS value
FROM pg_stat_activity
WHERE backend_type = 'client backend'
UNION ALL
SELECT
    'Cache Hit Ratio',
    ROUND(blks_hit * 100.0 / NULLIF(blks_hit + blks_read, 0), 2)::text || '%'
FROM pg_stat_database
WHERE datname = current_database();
```

### Common Tasks

| Task | Command |
|------|---------|
| Check status | `systemctl status scratchbird` |
| View logs | `journalctl -u scratchbird -f` |
| Reload config | `systemctl reload scratchbird` |
| Create backup | `sb_backup -U admin -d mydb -o backup.sbk` |
| List connections | `SELECT * FROM pg_stat_activity` |

---

## Administration by Topic

### Backup and Recovery

- [Full and Incremental Backups](backup-restore.md#part-2-using-sb_backup)
- [Automated Backup Scripts](backup-restore.md#part-4-automated-backups)
- [Point-in-Time Recovery](backup-restore.md#part-5-point-in-time-recovery-pitr)
- [Disaster Recovery Planning](backup-restore.md#part-8-disaster-recovery)

### Security

- [Authentication Methods](security.md#part-1-authentication)
- [TLS Configuration](security.md#part-2-tlsssl-encryption)
- [Role-Based Access Control](security.md#part-3-authorization)
- [Audit Logging](security.md#part-5-audit-logging)
- [Security Hardening](security.md#part-7-security-hardening)

### User Management

- [Creating Users](user-management.md#part-2-creating-users)
- [Managing Roles](user-management.md#part-3-creating-roles)
- [Granting Permissions](user-management.md#part-4-permission-management)
- [Password Policies](user-management.md#part-7-password-management)

### Monitoring

- [Built-in Statistics](monitoring.md#part-1-built-in-monitoring)
- [Prometheus Integration](monitoring.md#part-2-prometheus-integration)
- [Grafana Dashboards](monitoring.md#part-4-grafana-dashboards)
- [Alerting](monitoring.md#part-3-alerting)

### Troubleshooting

- [Server Startup Issues](troubleshooting.md#part-1-server-startup-issues)
- [Connection Problems](troubleshooting.md#part-2-connection-issues)
- [Performance Issues](troubleshooting.md#part-3-performance-issues)
- [Storage Problems](troubleshooting.md#part-4-storage-issues)

---

## Key Locations

### File Paths

| Item | Default Location |
|------|------------------|
| Configuration | `/etc/scratchbird/sb_server.conf` |
| HBA Config | `/etc/scratchbird/sb_hba.conf` |
| Data Directory | `/var/lib/scratchbird/` |
| Log Files | `/var/log/scratchbird/` |
| Certificates | `/etc/scratchbird/certs/` |
| Backups | `/var/backups/scratchbird/` |

### Ports

| Protocol | Default Port |
|----------|--------------|
| Native | 3092 |
| PostgreSQL | 5432 |
| MySQL | 3306 |
| Firebird | 3050 |
| Metrics | 9090 |

---

## Emergency Procedures

### Server Won't Start

```bash
# Check status and logs
systemctl status scratchbird
journalctl -u scratchbird -n 50

# Validate configuration
sb_server --config /etc/scratchbird/sb_server.conf --check

# Check permissions
ls -la /var/lib/scratchbird/
```

### Database Unresponsive

```bash
# Check connections
sb_isql -U admin -c "SELECT COUNT(*) FROM pg_stat_activity"

# Kill long-running queries
sb_isql -U admin -c "
SELECT pg_terminate_backend(pid)
FROM pg_stat_activity
WHERE state = 'active'
AND query_start < NOW() - INTERVAL '30 minutes'
"
```

### Disk Full

```bash
# Check disk usage
df -h /var/lib/scratchbird

# Clear old logs
find /var/log/scratchbird -name "*.log" -mtime +7 -delete

# Vacuum to reclaim space
sb_isql -U admin -d mydb -c "VACUUM FULL"
```

### Restore from Backup

```bash
# Stop server
systemctl stop scratchbird

# Restore
sb_restore -U admin --create -d mydb /backups/latest.sbk

# Start server
systemctl start scratchbird
```

---

## Best Practices

### Security

- Use SCRAM-SHA-256 for password authentication
- Enable TLS for all external connections
- Follow the principle of least privilege
- Enable audit logging for compliance
- Regularly review user permissions

### Performance

- Monitor cache hit ratio (target > 99%)
- Set up automated VACUUM ANALYZE
- Create appropriate indexes
- Use connection pooling for applications
- Monitor slow query logs

### Reliability

- Implement automated backups (daily minimum)
- Test restore procedures quarterly
- Set up monitoring and alerting
- Document runbooks for common issues
- Keep configuration in version control

---

## Related Guides

- [Installation Guides](../installation/)
- [Getting Started](../getting-started/)
- [Tutorials](../tutorials/)
- [User Guides](../user-guides/)
- [Troubleshooting](../troubleshooting/)

---

## Getting Help

- **Documentation:** Browse the [wiki home](../Home.md)
- **CLI Help:** Run `sb_isql --help` or `sb_server --help`
- **Logs:** Check `/var/log/scratchbird/` for error messages
- **Community:** Visit the project repository for support

