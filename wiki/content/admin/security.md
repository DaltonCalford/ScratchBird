# Security

**Last Updated:** 2026-02-03


Secure your ScratchBird installation.


---

## Security Checklist

### Initial Setup

- [ ] Change default admin password
- [ ] Configure host-based authentication
- [ ] Enable SSL/TLS
- [ ] Set up firewall rules
- [ ] Create application-specific users
- [ ] Review default privileges

### Ongoing

- [ ] Regular password rotation
- [ ] Audit user access
- [ ] Monitor for suspicious activity
- [ ] Keep software updated
- [ ] Test backup restoration
- [ ] Review logs

---

## Authentication Security

### Strong Passwords

Configure minimum requirements:

```ini
# sb_server.conf
[authentication]
password_min_length = 12
password_hash = argon2id
max_failed_attempts = 5
lockout_duration = 300
```

### Use SCRAM-SHA-256

Never use MD5 or trust in production:

```ini
[authentication]
methods = scram-sha-256
```

### Host-Based Authentication

Restrict connections by IP:

```
# hba.conf
# Reject admin from remote
host    all   admin     0.0.0.0/0        reject

# Localhost only for superuser
local   all   postgres                   peer
host    all   postgres  127.0.0.1/32     scram-sha-256

# Application from specific subnet
host    mydb  appuser   10.0.0.0/8       scram-sha-256

# SSL required from elsewhere
hostssl all   all       0.0.0.0/0        scram-sha-256
```

### Disable Remote Superuser

```ini
# sb_server.conf
[authentication]
allow_superuser_remote = false
```

---

## Encryption

### Enable TLS

```ini
# sb_server.conf
[ssl]
enabled = true
cert_file = /etc/scratchbird/ssl/server.crt
key_file = /etc/scratchbird/ssl/server.key
min_protocol = TLSv1.2
```

### Require SSL for Remote

```
# hba.conf
# Non-SSL connections only from localhost
hostnossl   all   all   127.0.0.1/32   scram-sha-256

# All others require SSL
hostssl     all   all   0.0.0.0/0      scram-sha-256
```

### Encrypt Backups

```bash
# Encrypt backup
sb_backup create mydb - | \
    gpg --encrypt -r admin@example.com > backup.sbdb.gpg
```

### Data at Rest

Consider full-disk encryption for production:

```bash
# Linux LUKS
cryptsetup luksFormat /dev/sdb
cryptsetup open /dev/sdb scratchbird_data
mkfs.ext4 /dev/mapper/scratchbird_data
mount /dev/mapper/scratchbird_data /var/lib/scratchbird
```

---

## Network Security

### Firewall Configuration

**Linux (UFW):**
```bash
# Allow only specific IPs
sudo ufw allow from 10.0.0.0/8 to any port 5432
sudo ufw deny 5432
```

**Linux (firewalld):**
```bash
sudo firewall-cmd --add-rich-rule='rule family="ipv4" source address="10.0.0.0/8" port protocol="tcp" port="5432" accept' --permanent
sudo firewall-cmd --reload
```

### Bind to Specific Interface

```ini
# sb_server.conf
[network]
# Only listen on internal interface
bind_address = 10.0.1.5

# Or localhost only
bind_address = 127.0.0.1
```

### Disable Unused Protocols

```ini
# sb_server.conf
[network]
native_port = 3092
pg_port = 5432
mysql_port = 0    # Disabled
fb_port = 0       # Disabled
```

---

## Access Control

### Principle of Least Privilege

Create specific users with minimal permissions:

```sql
-- Read-only user
CREATE USER reporter WITH PASSWORD 'secure_pass';
GRANT CONNECT ON DATABASE analytics TO reporter;
GRANT USAGE ON SCHEMA public TO reporter;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO reporter;

-- Application user (CRUD only)
CREATE USER webapp WITH PASSWORD 'secure_pass';
GRANT CONNECT ON DATABASE myapp TO webapp;
GRANT USAGE ON SCHEMA public TO webapp;
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO webapp;
```

### Row-Level Security

Restrict data access at row level:

```sql
-- Enable RLS
ALTER TABLE customers ENABLE ROW LEVEL SECURITY;

-- Policy: Users see only their own data
CREATE POLICY customer_isolation ON customers
    USING (owner_id = current_user_id());
```

### Column-Level Permissions

Hide sensitive columns:

```sql
-- Grant access to non-sensitive columns
GRANT SELECT (id, name, email) ON users TO analyst;
-- Analyst cannot see: password_hash, ssn, etc.
```

---

## Audit Logging

### Enable Connection Logging

```ini
# sb_server.conf
[logging]
log_connections = true
log_disconnections = true
```

### Enable Statement Logging

```ini
[logging]
log_statement = all  # none, ddl, mod, all
```

### Audit Sensitive Operations

```sql
-- Create audit table
CREATE TABLE audit_log (
    id SERIAL PRIMARY KEY,
    event_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    user_name TEXT,
    operation TEXT,
    table_name TEXT,
    old_data JSONB,
    new_data JSONB
);

-- Create audit trigger
CREATE OR REPLACE FUNCTION audit_trigger()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO audit_log (user_name, operation, table_name, old_data, new_data)
    VALUES (
        current_user,
        TG_OP,
        TG_TABLE_NAME,
        CASE WHEN TG_OP = 'DELETE' THEN row_to_json(OLD)::jsonb END,
        CASE WHEN TG_OP IN ('INSERT', 'UPDATE') THEN row_to_json(NEW)::jsonb END
    );
    RETURN COALESCE(NEW, OLD);
END;
$$ LANGUAGE plpgsql;

-- Apply to sensitive tables
CREATE TRIGGER audit_users
    AFTER INSERT OR UPDATE OR DELETE ON users
    FOR EACH ROW EXECUTE FUNCTION audit_trigger();
```

---

## SQL Injection Prevention

### Use Parameterized Queries

**Good:**
```python
cursor.execute("SELECT * FROM users WHERE id = %s", (user_id,))
```

**Bad:**
```python
cursor.execute(f"SELECT * FROM users WHERE id = {user_id}")
```

### Validate Input

```python
# Validate expected format
if not user_id.isdigit():
    raise ValueError("Invalid user ID")
```

### Limit Database Permissions

Application users should not have DDL rights:

```sql
-- No CREATE, DROP, ALTER
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO webapp;
```

---

## Security Monitoring

### Failed Login Monitoring

```bash
# Check for brute force attempts
grep "authentication failed" /var/log/scratchbird/sb_server.log | \
    awk '{print $NF}' | sort | uniq -c | sort -rn
```

### Long-Running Queries

```sql
-- Potential attack indicator
SELECT pid, usename, query_start, query
FROM pg_stat_activity
WHERE state = 'active'
  AND NOW() - query_start > INTERVAL '10 minutes';
```

### Unusual Patterns

Monitor for:
- Login from new IPs
- After-hours activity
- Unusual query patterns
- Bulk data access

---

## Security Hardening

### File Permissions

```bash
# Config files
chmod 600 /etc/scratchbird/sb_server.conf
chmod 600 /etc/scratchbird/hba.conf
chown scratchbird:scratchbird /etc/scratchbird/*

# SSL keys
chmod 600 /etc/scratchbird/ssl/server.key
chmod 644 /etc/scratchbird/ssl/server.crt

# Data directory
chmod 700 /var/lib/scratchbird
chown -R scratchbird:scratchbird /var/lib/scratchbird
```

### Systemd Hardening

The default service file includes:

```ini
[Service]
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
MemoryDenyWriteExecute=true
```

### Kernel Security

```bash
# /etc/sysctl.d/99-scratchbird.conf
# Prevent IP spoofing
net.ipv4.conf.all.rp_filter = 1

# SYN flood protection
net.ipv4.tcp_syncookies = 1

# Disable ICMP redirects
net.ipv4.conf.all.accept_redirects = 0
```

---

## Incident Response

### Suspicious Activity Detected

1. **Capture evidence:**
   ```bash
   # Snapshot connections
   psql -c "SELECT * FROM pg_stat_activity" > incident_$(date +%s).log
   ```

2. **Isolate if necessary:**
   ```bash
   # Block suspicious IP
   sudo ufw deny from 1.2.3.4
   ```

3. **Kill suspicious sessions:**
   ```sql
   SELECT pg_terminate_backend(pid)
   FROM pg_stat_activity
   WHERE client_addr = '1.2.3.4';
   ```

4. **Review audit logs**

5. **Document and report**

### Data Breach Response

1. Contain the breach
2. Assess scope
3. Notify affected parties
4. Preserve evidence
5. Remediate vulnerabilities
6. Review and improve

---

## Compliance

### Password Policies

| Requirement | Configuration |
|-------------|---------------|
| Minimum length | `password_min_length = 12` |
| Complexity | Application-enforced |
| Rotation | Policy + user education |
| History | Not reusing recent passwords |

### Access Logging

Maintain logs for required retention period:

```bash
# Log rotation with retention
/var/log/scratchbird/*.log {
    daily
    rotate 365
    compress
    delaycompress
    notifempty
}
```

---

## Next Steps

- [Configure SSL/TLS](../configuration/ssl-setup.md)
- [Set up authentication](../configuration/hba.conf.md)
- [User management](user-management.md)
- [Monitoring](monitoring.md)
