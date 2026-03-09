# Troubleshooting Connection Failures

[Troubleshooting README](../README.md) | [How-To Guide README](../../README.md)

## Quick Diagnosis

```bash
# Test basic connectivity
telnet localhost 3092

# Check if server is running
ps aux | grep scratchbird

# Check listening ports
ss -tlnp | grep 3092

# Test with sb_isql
sb_isql -h localhost -p 3092 -c "SELECT 1"
```

## Common Issues

### 1. Server Not Running

**Symptoms:**
- Connection refused
- No response from server

**Diagnosis:**
```bash
# Check process
pgrep -a scratchbird

# Check systemd status
systemctl status scratchbird

# Check logs
journalctl -u scratchbird -n 50
```

**Resolution:**
```bash
# Start server
systemctl start scratchbird

# Or manual start
sb_server -D /var/lib/scratchbird/data
```

### 2. Wrong Port

**Symptoms:**
- Connection refused on expected port
- Server running but not accessible

**Diagnosis:**
```bash
# Check actual listening port
ss -tlnp | grep scratchbird

# Check configuration
grep "^port" /etc/scratchbird/scratchbird.conf
```

**Resolution:**
```bash
# Update client connection string
sb_isql -h localhost -p 3093  # Use actual port

# Or reconfigure server
# Edit scratchbird.conf: port = 3092
systemctl restart scratchbird
```

### 3. Firewall Blocking

**Symptoms:**
- Connection timeout
- Works locally but not remotely

**Diagnosis:**
```bash
# Check firewall status
sudo iptables -L -n | grep 3092
sudo firewall-cmd --list-ports

# Test from remote host
telnet server-ip 3092
```

**Resolution:**
```bash
# Open port in firewall
sudo firewall-cmd --add-port=3092/tcp --permanent
sudo firewall-cmd --reload

# Or iptables
sudo iptables -A INPUT -p tcp --dport 3092 -j ACCEPT
```

### 4. Authentication Failure

**Symptoms:**
- Connection succeeds but login fails
- "authentication failed" error

**Diagnosis:**
```bash
# Check authentication settings
grep -E "^(auth|password)" /etc/scratchbird/scratchbird.conf

# Check user exists
sb_isql -c "SELECT rolname FROM pg_roles WHERE rolname = 'username'"

# Check pg_hba.conf
cat /etc/scratchbird/pg_hba.conf
```

**Resolution:**
```sql
-- Reset password
ALTER USER username WITH PASSWORD 'newpassword';

-- Check authentication method
-- In pg_hba.conf:
-- host all all 192.168.1.0/24 scram-sha-256
```

### 5. SSL/TLS Issues

**Symptoms:**
- SSL handshake failed
- Certificate verification error

**Diagnosis:**
```bash
# Test SSL connection
openssl s_client -connect localhost:3092

# Check certificate
openssl x509 -in /etc/scratchbird/server.crt -text -noout

# Verify certificate chain
openssl verify -CAfile /etc/scratchbird/ca.crt /etc/scratchbird/server.crt
```

**Resolution:**
```bash
# Regenerate certificates
sb_ssl_setup --generate-certs --ca

# Or disable SSL for testing (NOT for production)
# In scratchbird.conf:
ssl = off
```

### 6. Maximum Connections Reached

**Symptoms:**
- "sorry, too many clients already"
- Intermittent connection failures

**Diagnosis:**
```sql
-- Check current connections
SELECT count(*) FROM pg_stat_activity;

-- Check max connections
SHOW max_connections;
```

**Resolution:**
```ini
# Increase max connections
# In scratchbird.conf:
max_connections = 200

# Or use connection pooling
# pgbouncer or similar
```

## Connection String Debugging

### Test Connection String

```bash
# Native protocol
sb_isql "host=localhost port=3092 dbname=mydb user=myuser password=mypass"

# JDBC
jdbc:scratchbird://localhost:3092/mydb?user=myuser&password=mypass

# ODBC
driver=ScratchBird;server=localhost;port=3092;database=mydb;uid=myuser;pwd=mypass
```

### Enable Verbose Logging

```bash
# Server-side logging
# In scratchbird.conf:
log_connections = on
log_disconnections = on
log_line_prefix = '%t [%p]: [%l-1] user=%u,db=%d,app=%a,client=%h '
log_min_messages = debug2
```

## Network Diagnostics

### Check Network Path

```bash
# Traceroute
traceroute server-hostname

# Check DNS resolution
nslookup server-hostname
dig server-hostname

# Check latency
ping server-hostname
```

### Packet Capture

```bash
# Capture connection attempt
sudo tcpdump -i any -n port 3092 -w /tmp/scratchbird.pcap

# Analyze with Wireshark
# Look for:
# - TCP handshake (SYN, SYN-ACK, ACK)
# - SSL handshake (Client Hello, Server Hello)
# - Authentication messages
```

## Logs Analysis

### Common Log Messages

```
# Successful connection
LOG:  connection authorized: user=myuser database=mydb

# Failed authentication
LOG:  connection failed: user=myuser database=mydb error=authentication_failed

# Connection refused (server down)
ERROR:  could not connect to server: Connection refused

# Too many connections
FATAL:  sorry, too many clients already

# SSL error
LOG:  SSL error: tlsv1 alert unknown ca
```

### Log Location

```bash
# Default log location
/var/log/scratchbird/

# Or configured location
grep "^log_directory" /etc/scratchbird/scratchbird.conf

# View recent errors
tail -f /var/log/scratchbird/postgresql-$(date +%Y-%m-%d).log
```

## See Also

- [Authentication and Authorization Failures](02_authentication_and_authorization_failures.md)
- [Network Configuration](../../configuration_reference/server_parameters/01_listener_network_and_protocol_parameters.md)
