# Troubleshooting Authentication and Authorization

[Troubleshooting README](../README.md)

## Authentication Failures

### "password authentication failed"

**Symptoms:**
- Login denied with correct credentials
- Error: `password authentication failed for user "xxx"`

**Diagnosis:**
```sql
-- Check if user exists
SELECT rolname FROM pg_roles WHERE rolname = 'username';

-- Check authentication method
-- Review pg_hba.conf
SHOW hba_file;

-- Check password
-- (Try resetting)
```

**Resolution:**
```sql
-- Reset password
ALTER USER username WITH PASSWORD 'new_password';

-- Check pg_hba.conf has correct method
-- host all all 127.0.0.1/32 scram-sha-256
```

### "role does not exist"

**Symptoms:**
- Error: `role "xxx" does not exist`

**Resolution:**
```sql
-- Create user
CREATE USER username WITH PASSWORD 'password';

-- Or create role
CREATE ROLE username LOGIN PASSWORD 'password';
```

### SSL/TLS Issues

**Symptoms:**
- `SSL connection has been closed unexpectedly`
- `certificate verify failed`

**Diagnosis:**
```bash
# Check certificate validity
openssl x509 -in /path/to/server.crt -text -noout

# Verify date
openssl x509 -in /path/to/server.crt -noout -dates

# Test connection
openssl s_client -connect localhost:3092
```

**Resolution:**
```bash
# Regenerate certificates
sb_ssl_setup --generate-certs

# Or disable SSL temporarily for testing
# In scratchbird.conf:
ssl = off
```

## Authorization Failures

### "permission denied for table"

**Symptoms:**
- `ERROR: permission denied for table xxx`

**Diagnosis:**
```sql
-- Check current user
SELECT current_user;

-- Check table owner
SELECT tableowner FROM pg_tables WHERE tablename = 'xxx';

-- Check privileges
SELECT * FROM information_schema.table_privileges
WHERE table_name = 'xxx' AND grantee = current_user;
```

**Resolution:**
```sql
-- As table owner or superuser:
GRANT SELECT ON table_name TO username;
GRANT ALL ON table_name TO username;

-- Grant schema usage
GRANT USAGE ON SCHEMA public TO username;
```

### RLS Policy Blocking Access

**Symptoms:**
- No rows returned when rows exist
- Unexpected empty result sets

**Diagnosis:**
```sql
-- Check if RLS is enabled
SELECT relname, relrowsecurity 
FROM pg_class 
WHERE relname = 'mytable';

-- Check policies
SELECT * FROM pg_policies WHERE tablename = 'mytable';

-- Test as different user
SET ROLE test_user;
SELECT * FROM mytable;
RESET ROLE;
```

**Resolution:**
```sql
-- Check policy definition
-- May need to adjust USING clause

-- Disable RLS (caution!)
ALTER TABLE mytable DISABLE ROW LEVEL SECURITY;

-- Or create appropriate policy
CREATE POLICY allow_all ON mytable FOR ALL TO PUBLIC USING (true);
```

### Column-Level Security

**Symptoms:**
- `ERROR: permission denied for column xxx`

**Resolution:**
```sql
-- Grant column privilege
GRANT SELECT (col1, col2) ON table_name TO username;

-- Or grant all columns
GRANT SELECT ON table_name TO username;
```

## Debugging Authentication Flow

```bash
# Enable detailed logging
# In scratchbird.conf:
log_connections = on
log_disconnections = on
log_line_prefix = '%t [%p]: [%l-1] user=%u,db=%d,app=%a,client=%h '
log_min_messages = debug1

# Reload config
sb_ctl reload
```

## Common Scenarios

### Application Cannot Connect

1. Verify user exists
2. Check password
3. Verify pg_hba.conf allows connection
4. Check network/firewall
5. Verify SSL settings match

### User Can Connect But Cannot Query

1. Check schema usage privilege
2. Verify table privileges
3. Check RLS policies
4. Verify column-level security

### Emulated Database User Issues

```sql
-- Check environment-scoped user
SELECT * FROM pg_roles WHERE rolname LIKE '!:prod.emulated_pg%';

-- Verify master UUID mapping
-- Check environment authentication policy
```

## See Also

- [Connection Failures](01_connection_failures.md)
- [Security Hardening](../../../security_hardening_and_compliance/identity_authentication_and_plugins/README.md)
