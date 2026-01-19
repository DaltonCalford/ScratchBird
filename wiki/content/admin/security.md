# Security Administration

**Status:** Alpha documentation
**Last Updated:** 2026-01-19

---

## Overview

Security is fundamental to protecting your data. This guide covers authentication, authorization, encryption, network security, and security best practices for ScratchBird deployments.

**Topics covered:**
- Authentication methods
- TLS/SSL encryption
- Network security
- Audit logging
- Security hardening

---

## Part 1: Authentication

### Authentication Methods

ScratchBird supports multiple authentication methods:

| Method | Description | Use Case |
|--------|-------------|----------|
| `password` | MD5 or SCRAM-SHA-256 hashed password | Default, most common |
| `scram-sha-256` | SCRAM-SHA-256 (recommended) | High security |
| `cert` | Client certificate | Certificate-based auth |
| `ldap` | LDAP/Active Directory | Enterprise SSO |
| `trust` | No authentication | Local development only |
| `reject` | Reject all connections | Blocking specific hosts |

### Password Authentication

**Configure password encryption** in `sb_server.conf`:
```ini
[authentication]
# Use SCRAM-SHA-256 (recommended)
password_encryption = scram-sha-256

# Or use MD5 (legacy compatibility)
# password_encryption = md5
```

**Set user password:**
```sql
-- Create user with password
CREATE USER app_user WITH PASSWORD 'SecureP@ssw0rd!';

-- Change existing password
ALTER USER app_user WITH PASSWORD 'NewSecureP@ssw0rd!';
```

### Host-Based Authentication (sb_hba.conf)

The `sb_hba.conf` file controls which hosts can connect and how they authenticate.

**File location:** `/etc/scratchbird/sb_hba.conf`

**Format:**
```
# TYPE    DATABASE    USER        ADDRESS           METHOD
local     all         all                           peer
host      all         all         127.0.0.1/32      scram-sha-256
host      all         all         ::1/128           scram-sha-256
host      all         all         192.168.1.0/24    scram-sha-256
hostssl   all         all         0.0.0.0/0         scram-sha-256
```

**Field descriptions:**

| Field | Options | Description |
|-------|---------|-------------|
| TYPE | local, host, hostssl, hostnossl | Connection type |
| DATABASE | all, db_name, @file | Database(s) to match |
| USER | all, user_name, +group, @file | User(s) to match |
| ADDRESS | IP/CIDR, hostname, all | Client address |
| METHOD | trust, reject, scram-sha-256, md5, cert, ldap | Auth method |

**Example configurations:**

```
# Local connections via Unix socket - use peer auth
local   all             all                                     peer

# IPv4 local connections - password required
host    all             all             127.0.0.1/32            scram-sha-256

# IPv6 local connections
host    all             all             ::1/128                 scram-sha-256

# Internal network - password required
host    all             all             10.0.0.0/8              scram-sha-256
host    all             all             192.168.0.0/16          scram-sha-256

# Specific database from specific subnet
host    production      app_user        10.1.5.0/24             scram-sha-256

# Replication connections
host    replication     repl_user       10.1.10.0/24            scram-sha-256

# SSL required for all external connections
hostssl all             all             0.0.0.0/0               scram-sha-256

# Reject all other connections
host    all             all             0.0.0.0/0               reject
```

### LDAP Authentication

**Configure LDAP** in `sb_hba.conf`:
```
host    all    all    0.0.0.0/0    ldap ldapserver=ldap.example.com ldapbasedn="dc=example,dc=com" ldapbinddn="cn=admin,dc=example,dc=com" ldapbindpasswd="secret" ldapsearchattribute=uid
```

**LDAP options:**

| Option | Description |
|--------|-------------|
| `ldapserver` | LDAP server hostname |
| `ldapport` | LDAP port (default: 389, 636 for SSL) |
| `ldaptls` | Use STARTTLS (1 = yes) |
| `ldapbasedn` | Base DN for searches |
| `ldapbinddn` | DN to bind for searches |
| `ldapbindpasswd` | Password for bind DN |
| `ldapsearchattribute` | Attribute to match username |

### Certificate Authentication

**Configure client certificate auth:**

1. **Generate certificates** (see TLS section)

2. **Configure sb_hba.conf:**
```
hostssl    all    all    0.0.0.0/0    cert clientcert=verify-full
```

3. **Map certificate CN to database user:**
```
# In sb_ident.conf
cert_map    /CN=app_user    app_user
cert_map    /CN=admin       admin
```

---

## Part 2: TLS/SSL Encryption

### Generate Certificates

**Create CA and server certificates:**

```bash
#!/bin/bash
# Generate ScratchBird SSL certificates

CERT_DIR="/etc/scratchbird/certs"
mkdir -p "$CERT_DIR"
cd "$CERT_DIR"

# Generate CA private key
openssl genrsa -out ca.key 4096

# Generate CA certificate
openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 \
    -out ca.crt \
    -subj "/C=US/ST=State/L=City/O=Organization/CN=ScratchBird CA"

# Generate server private key
openssl genrsa -out server.key 2048

# Generate server CSR
openssl req -new -key server.key \
    -out server.csr \
    -subj "/C=US/ST=State/L=City/O=Organization/CN=db.example.com"

# Create server certificate extension file
cat > server.ext << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
subjectAltName = @alt_names

[alt_names]
DNS.1 = db.example.com
DNS.2 = localhost
IP.1 = 127.0.0.1
IP.2 = 192.168.1.10
EOF

# Sign server certificate
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
    -CAcreateserial -out server.crt -days 365 -sha256 \
    -extfile server.ext

# Set permissions
chmod 600 server.key ca.key
chmod 644 server.crt ca.crt
chown scratchbird:scratchbird server.key server.crt ca.crt

# Cleanup
rm server.csr server.ext

echo "Certificates generated in $CERT_DIR"
```

### Configure TLS

**Edit `sb_server.conf`:**
```ini
[ssl]
enabled = true
cert_file = /etc/scratchbird/certs/server.crt
key_file = /etc/scratchbird/certs/server.key
ca_file = /etc/scratchbird/certs/ca.crt

# Optional: Require client certificates
# client_cert = require

# TLS version (minimum)
min_protocol_version = TLSv1.2

# Cipher suites (example for high security)
ciphers = ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305
```

### Verify TLS Configuration

```bash
# Test TLS connection
openssl s_client -connect localhost:5432 -starttls postgres

# Check certificate details
openssl x509 -in /etc/scratchbird/certs/server.crt -text -noout

# Verify certificate chain
openssl verify -CAfile /etc/scratchbird/certs/ca.crt /etc/scratchbird/certs/server.crt
```

### Client TLS Connection

```bash
# Connect with SSL verification
sb_isql "host=db.example.com port=5432 dbname=mydb user=admin sslmode=verify-full sslrootcert=/path/to/ca.crt"

# With client certificate
sb_isql "host=db.example.com port=5432 dbname=mydb user=admin sslmode=verify-full sslcert=/path/to/client.crt sslkey=/path/to/client.key sslrootcert=/path/to/ca.crt"
```

**SSL modes:**

| Mode | Description |
|------|-------------|
| `disable` | No SSL |
| `allow` | Try SSL, fallback to non-SSL |
| `prefer` | Try SSL first (default) |
| `require` | Require SSL, no cert verification |
| `verify-ca` | Require SSL, verify CA |
| `verify-full` | Require SSL, verify CA and hostname |

---

## Part 3: Authorization

### Role-Based Access Control

**Create roles:**
```sql
-- Create roles
CREATE ROLE readonly;
CREATE ROLE readwrite;
CREATE ROLE admin_role;

-- Grant privileges to roles
GRANT CONNECT ON DATABASE mydb TO readonly;
GRANT USAGE ON SCHEMA public TO readonly;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO readonly;

GRANT CONNECT ON DATABASE mydb TO readwrite;
GRANT USAGE ON SCHEMA public TO readwrite;
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO readwrite;

-- Admin role
GRANT ALL PRIVILEGES ON DATABASE mydb TO admin_role;
GRANT ALL PRIVILEGES ON SCHEMA public TO admin_role;
```

**Assign roles to users:**
```sql
-- Create users and assign roles
CREATE USER report_user WITH PASSWORD 'password';
GRANT readonly TO report_user;

CREATE USER app_user WITH PASSWORD 'password';
GRANT readwrite TO app_user;

CREATE USER dba_user WITH PASSWORD 'password';
GRANT admin_role TO dba_user;
```

### Schema-Based Isolation

```sql
-- Create schemas for different applications
CREATE SCHEMA app1;
CREATE SCHEMA app2;

-- Create users with schema access
CREATE USER app1_user WITH PASSWORD 'password';
CREATE USER app2_user WITH PASSWORD 'password';

-- Grant schema-specific access
GRANT USAGE ON SCHEMA app1 TO app1_user;
GRANT ALL ON ALL TABLES IN SCHEMA app1 TO app1_user;

GRANT USAGE ON SCHEMA app2 TO app2_user;
GRANT ALL ON ALL TABLES IN SCHEMA app2 TO app2_user;

-- Set default search path
ALTER USER app1_user SET search_path TO app1, public;
ALTER USER app2_user SET search_path TO app2, public;
```

### Row-Level Security (RLS)

```sql
-- Enable RLS on table
ALTER TABLE documents ENABLE ROW LEVEL SECURITY;

-- Create policy: users can only see their own documents
CREATE POLICY user_documents ON documents
    FOR ALL
    USING (owner_id = current_user_id())
    WITH CHECK (owner_id = current_user_id());

-- Create policy: admins can see all documents
CREATE POLICY admin_all_documents ON documents
    FOR ALL
    TO admin_role
    USING (true);

-- Force RLS for table owner too
ALTER TABLE documents FORCE ROW LEVEL SECURITY;
```

### Column-Level Permissions

```sql
-- Grant select on specific columns
GRANT SELECT (id, name, email) ON users TO readonly;

-- Revoke access to sensitive columns
REVOKE SELECT (password_hash, ssn) ON users FROM PUBLIC;
```

---

## Part 4: Network Security

### Firewall Configuration

**Linux (iptables):**
```bash
# Allow connections from internal network only
iptables -A INPUT -p tcp --dport 3092 -s 10.0.0.0/8 -j ACCEPT
iptables -A INPUT -p tcp --dport 3092 -s 192.168.0.0/16 -j ACCEPT
iptables -A INPUT -p tcp --dport 3092 -j DROP

# Allow PostgreSQL protocol
iptables -A INPUT -p tcp --dport 5432 -s 10.0.0.0/8 -j ACCEPT
iptables -A INPUT -p tcp --dport 5432 -j DROP

# Save rules
iptables-save > /etc/iptables/rules.v4
```

**Linux (ufw):**
```bash
# Allow from specific subnet
ufw allow from 10.0.0.0/8 to any port 3092
ufw allow from 10.0.0.0/8 to any port 5432

# Deny all other database connections
ufw deny 3092
ufw deny 5432
```

### Bind Address Configuration

**Restrict listening address** in `sb_server.conf`:
```ini
[network]
# Listen only on internal interface
bind_address = 10.0.1.5

# Or listen on multiple specific addresses
# bind_address = 10.0.1.5,192.168.1.5

# Never use 0.0.0.0 in production without firewall
# bind_address = 0.0.0.0  # DANGEROUS
```

### Connection Limits

```ini
[server]
# Maximum connections
max_connections = 100

# Connection timeout (seconds)
connection_timeout = 30

# Idle session timeout (0 = disabled)
idle_session_timeout = 3600
```

**Per-user connection limits:**
```sql
-- Limit connections per user
ALTER USER app_user CONNECTION LIMIT 20;

-- Limit connections per database
ALTER DATABASE mydb CONNECTION LIMIT 50;
```

---

## Part 5: Audit Logging

### Enable Audit Logging

**Configure in `sb_server.conf`:**
```ini
[logging]
level = info

# Log all connections
log_connections = true
log_disconnections = true

# Log DDL statements
log_statement = ddl

# Log all statements (use with caution - performance impact)
# log_statement = all

# Log slow queries
log_slow_queries = true
slow_query_threshold = 1000  # milliseconds

[audit]
enabled = true
log_file = /var/log/scratchbird/audit.log

# What to audit
audit_ddl = true
audit_dml = true
audit_dcl = true
audit_read = false  # Can be noisy

# Audit specific users
# audit_users = admin,app_user
```

### Audit Log Format

```json
{"timestamp":"2026-01-19T10:30:45.123Z","event":"LOGIN","user":"admin","database":"mydb","client_ip":"192.168.1.100","success":true}
{"timestamp":"2026-01-19T10:30:46.456Z","event":"DDL","user":"admin","database":"mydb","statement":"CREATE TABLE users (...)","success":true}
{"timestamp":"2026-01-19T10:30:47.789Z","event":"DML","user":"app_user","database":"mydb","statement":"INSERT INTO users VALUES (...)","rows_affected":1,"success":true}
{"timestamp":"2026-01-19T10:30:48.012Z","event":"LOGIN_FAILED","user":"hacker","database":"mydb","client_ip":"1.2.3.4","reason":"password authentication failed"}
```

### Audit Analysis Queries

```sql
-- Create audit log table for analysis
CREATE TABLE sb_admin.audit_log (
    id SERIAL PRIMARY KEY,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL,
    event_type VARCHAR(20) NOT NULL,
    username VARCHAR(100),
    database_name VARCHAR(100),
    client_ip INET,
    statement TEXT,
    success BOOLEAN,
    details JSONB
);

-- Import audit logs (example)
COPY sb_admin.audit_log (timestamp, event_type, username, database_name, client_ip, statement, success, details)
FROM PROGRAM 'cat /var/log/scratchbird/audit.log | jq -r ''[.timestamp, .event, .user, .database, .client_ip, .statement, .success, .]|@csv'''
WITH (FORMAT csv);

-- Failed login attempts
SELECT
    client_ip,
    username,
    COUNT(*) AS attempts,
    MIN(timestamp) AS first_attempt,
    MAX(timestamp) AS last_attempt
FROM sb_admin.audit_log
WHERE event_type = 'LOGIN_FAILED'
AND timestamp > NOW() - INTERVAL '24 hours'
GROUP BY client_ip, username
ORDER BY attempts DESC;

-- DDL changes by user
SELECT
    username,
    DATE_TRUNC('day', timestamp) AS day,
    COUNT(*) AS ddl_count
FROM sb_admin.audit_log
WHERE event_type = 'DDL'
GROUP BY username, DATE_TRUNC('day', timestamp)
ORDER BY day DESC, ddl_count DESC;
```

---

## Part 6: Data Protection

### Encryption at Rest

**Full disk encryption:**
```bash
# Using LUKS for data directory
cryptsetup luksFormat /dev/sdb1
cryptsetup luksOpen /dev/sdb1 scratchbird_data
mkfs.ext4 /dev/mapper/scratchbird_data
mount /dev/mapper/scratchbird_data /var/lib/scratchbird
```

**Column-level encryption:**
```sql
-- Using pgcrypto extension
CREATE EXTENSION IF NOT EXISTS pgcrypto;

-- Encrypt sensitive data
INSERT INTO users (name, ssn_encrypted)
VALUES ('John Doe', pgp_sym_encrypt('123-45-6789', 'encryption_key'));

-- Decrypt when needed
SELECT name, pgp_sym_decrypt(ssn_encrypted::bytea, 'encryption_key') AS ssn
FROM users
WHERE id = 1;
```

### Data Masking

```sql
-- Create masking function
CREATE OR REPLACE FUNCTION mask_email(email TEXT)
RETURNS TEXT AS $$
BEGIN
    RETURN REGEXP_REPLACE(email, '(^[^@]{2})[^@]*(@.*)', '\1***\2');
END;
$$ LANGUAGE plpgsql IMMUTABLE;

-- Create masking function for SSN
CREATE OR REPLACE FUNCTION mask_ssn(ssn TEXT)
RETURNS TEXT AS $$
BEGIN
    RETURN 'XXX-XX-' || RIGHT(ssn, 4);
END;
$$ LANGUAGE plpgsql IMMUTABLE;

-- Create view with masked data for non-privileged users
CREATE VIEW users_masked AS
SELECT
    id,
    name,
    mask_email(email) AS email,
    mask_ssn(ssn) AS ssn,
    created_at
FROM users;

-- Grant access to masked view
GRANT SELECT ON users_masked TO readonly;
```

---

## Part 7: Security Hardening

### Hardening Checklist

**Authentication:**
- [ ] Use SCRAM-SHA-256 for password authentication
- [ ] Configure strict sb_hba.conf rules
- [ ] Disable trust authentication
- [ ] Implement password policies
- [ ] Enable account lockout after failed attempts

**Network:**
- [ ] Enable TLS for all connections
- [ ] Bind to specific interface (not 0.0.0.0)
- [ ] Configure firewall rules
- [ ] Use hostssl in sb_hba.conf
- [ ] Disable unnecessary protocols

**Authorization:**
- [ ] Follow principle of least privilege
- [ ] Use roles instead of direct grants
- [ ] Implement row-level security where needed
- [ ] Remove default/public grants
- [ ] Audit privilege assignments

**Monitoring:**
- [ ] Enable audit logging
- [ ] Monitor for failed login attempts
- [ ] Alert on suspicious activity
- [ ] Review logs regularly
- [ ] Monitor for unauthorized schema changes

### Remove Default Permissions

```sql
-- Revoke default public access
REVOKE ALL ON DATABASE mydb FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE CREATE ON SCHEMA public FROM PUBLIC;

-- Explicitly grant what's needed
GRANT CONNECT ON DATABASE mydb TO app_user;
GRANT USAGE ON SCHEMA public TO app_user;
```

### Secure Configuration

**`sb_server.conf` security settings:**
```ini
[server]
# Disable superuser remote connections
superuser_reserved_connections = 3

[authentication]
# Strong password hashing
password_encryption = scram-sha-256

# Password policy (if supported)
password_min_length = 12
password_require_uppercase = true
password_require_lowercase = true
password_require_digit = true
password_require_special = true

[ssl]
enabled = true
min_protocol_version = TLSv1.2

[logging]
log_connections = true
log_disconnections = true
log_statement = ddl

[security]
# Disable dangerous functions
allow_system_commands = false
```

### Security Testing

**Test authentication:**
```bash
# Test without credentials (should fail)
sb_isql -H localhost -U nobody -d mydb
# Expected: authentication failed

# Test with wrong password (should fail)
sb_isql -H localhost -U admin -d mydb -W wrongpassword
# Expected: password authentication failed

# Test from blocked network (should fail)
sb_isql -H db.example.com -U admin -d mydb
# Expected: connection refused (if not in sb_hba.conf)
```

**Test TLS:**
```bash
# Verify TLS is required
sb_isql "host=db.example.com dbname=mydb user=admin sslmode=disable"
# Expected: SSL required

# Verify certificate validation
sb_isql "host=db.example.com dbname=mydb user=admin sslmode=verify-full sslrootcert=/wrong/ca.crt"
# Expected: certificate verification failed
```

---

## Part 8: Incident Response

### Detecting Breaches

**Monitor for signs of compromise:**

```sql
-- Unusual login patterns
SELECT
    usename,
    client_addr,
    COUNT(*) AS logins,
    MIN(backend_start) AS first_login,
    MAX(backend_start) AS last_login
FROM pg_stat_activity
WHERE backend_start > NOW() - INTERVAL '1 hour'
GROUP BY usename, client_addr
ORDER BY logins DESC;

-- Check for new superusers
SELECT usename, usecreatedb, usesuper
FROM pg_user
WHERE usesuper = true;

-- Check for new database objects
SELECT
    schemaname,
    tablename,
    tableowner
FROM pg_tables
WHERE schemaname NOT IN ('pg_catalog', 'information_schema')
ORDER BY tablename;

-- Check for privilege escalation
SELECT
    grantee,
    privilege_type,
    table_schema,
    table_name
FROM information_schema.table_privileges
WHERE grantee NOT IN ('postgres', 'admin')
AND privilege_type IN ('INSERT', 'UPDATE', 'DELETE', 'TRUNCATE');
```

### Response Procedures

**Immediate response:**
```bash
# 1. Block suspicious IP
iptables -I INPUT -s 1.2.3.4 -j DROP

# 2. Terminate suspicious connections
sb_isql -U admin -c "SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE client_addr = '1.2.3.4'"

# 3. Lock compromised account
sb_isql -U admin -c "ALTER USER compromised_user NOLOGIN"

# 4. Capture evidence
cp /var/log/scratchbird/audit.log /evidence/audit_$(date +%Y%m%d_%H%M%S).log
sb_isql -U admin -c "SELECT * FROM pg_stat_activity" > /evidence/connections.txt
```

**Post-incident:**
```sql
-- Change all passwords
ALTER USER admin WITH PASSWORD 'new_secure_password';
ALTER USER app_user WITH PASSWORD 'new_secure_password';

-- Review and revoke suspicious grants
REVOKE ALL PRIVILEGES ON ALL TABLES IN SCHEMA public FROM suspicious_user;
DROP USER suspicious_user;

-- Check for backdoors
SELECT proname, prosrc FROM pg_proc WHERE proname LIKE '%backdoor%' OR prosrc LIKE '%/bin/%';
```

---

## Quick Reference

### Common Security Commands

```sql
-- Create user with password
CREATE USER app_user WITH PASSWORD 'SecureP@ssw0rd!';

-- Grant role
GRANT readonly TO app_user;

-- Revoke privileges
REVOKE ALL ON DATABASE mydb FROM PUBLIC;

-- Enable RLS
ALTER TABLE sensitive_data ENABLE ROW LEVEL SECURITY;

-- Check user privileges
SELECT * FROM information_schema.table_privileges WHERE grantee = 'app_user';
```

### sb_hba.conf Quick Reference

```
# Local Unix socket
local   all    all                    peer

# Localhost
host    all    all    127.0.0.1/32    scram-sha-256

# Internal network (SSL required)
hostssl all    all    10.0.0.0/8      scram-sha-256

# Reject everything else
host    all    all    0.0.0.0/0       reject
```

### SSL Modes

| Mode | Encryption | Server Cert | Hostname |
|------|------------|-------------|----------|
| disable | No | No | No |
| require | Yes | No | No |
| verify-ca | Yes | Yes | No |
| verify-full | Yes | Yes | Yes |

---

## See Also

- [User Management](user-management.md)
- [Monitoring](monitoring.md)
- [Troubleshooting](troubleshooting.md)
- [Backup and Restore](backup-restore.md)

