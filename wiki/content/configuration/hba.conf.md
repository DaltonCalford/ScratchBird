# Host-Based Authentication (hba.conf)

**Last Updated:** 2026-02-03


Configure connection security rules.


---

## Overview

Host-Based Authentication (HBA) controls which users can connect from which hosts using which authentication methods. Rules are evaluated in order; the first matching rule is used.

---

## File Location

| Installation | Path |
|--------------|------|
| DEB/RPM | `/etc/scratchbird/hba.conf` |
| Tarball | `/opt/scratchbird/conf/hba.conf` |
| Windows | `C:\ProgramData\ScratchBird\hba.conf` |

---

## Syntax

```
# TYPE  DATABASE  USER  ADDRESS        METHOD  [OPTIONS]
local   all       all                  peer
host    all       all   127.0.0.1/32   scram-sha-256
host    all       all   ::1/128        scram-sha-256
```

Each line defines a rule with these fields:

| Field | Description |
|-------|-------------|
| TYPE | Connection type (local, host, hostssl, hostnossl) |
| DATABASE | Database name, "all", or comma-separated list |
| USER | Username, "all", or comma-separated list |
| ADDRESS | IP address/netmask (for host types) |
| METHOD | Authentication method |
| OPTIONS | Method-specific options |

---

## Connection Types

### local

Unix domain socket connections (Linux/macOS):

```
local   all   all   peer
```

### host

TCP/IP connections (with or without SSL):

```
host    all   all   0.0.0.0/0   scram-sha-256
```

### hostssl

TCP/IP connections requiring SSL:

```
hostssl   all   all   0.0.0.0/0   scram-sha-256
```

### hostnossl

TCP/IP connections without SSL:

```
hostnossl   all   all   192.168.0.0/16   scram-sha-256
```

---

## Database Field

| Value | Meaning |
|-------|---------|
| `all` | All databases |
| `sameuser` | Database matching username |
| `mydb` | Specific database |
| `db1,db2` | Multiple databases |
| `@file` | List from file |

Examples:

```
host    all         all   0.0.0.0/0   scram-sha-256
host    production  all   0.0.0.0/0   scram-sha-256
host    dev,test    all   0.0.0.0/0   trust
```

---

## User Field

| Value | Meaning |
|-------|---------|
| `all` | All users |
| `admin` | Specific user |
| `user1,user2` | Multiple users |
| `+group` | Group members |
| `@file` | List from file |

Examples:

```
host    all   all          0.0.0.0/0   scram-sha-256
host    all   admin        0.0.0.0/0   scram-sha-256
host    all   +developers  10.0.0.0/8  scram-sha-256
```

---

## Address Field

CIDR notation for IP ranges:

| Format | Meaning |
|--------|---------|
| `127.0.0.1/32` | Single IPv4 address |
| `192.168.0.0/24` | IPv4 subnet (256 addresses) |
| `10.0.0.0/8` | Large IPv4 range |
| `0.0.0.0/0` | All IPv4 addresses |
| `::1/128` | Single IPv6 address |
| `::/0` | All IPv6 addresses |

Common patterns:

```
# Localhost only
host    all   all   127.0.0.1/32   scram-sha-256
host    all   all   ::1/128        scram-sha-256

# Local network
host    all   all   192.168.0.0/16   scram-sha-256
host    all   all   10.0.0.0/8       scram-sha-256

# Anywhere (use with SSL!)
hostssl all   all   0.0.0.0/0        scram-sha-256
```

---

## Authentication Methods

### scram-sha-256 (Recommended)

Secure password authentication using SCRAM-SHA-256:

```
host    all   all   0.0.0.0/0   scram-sha-256
```

### scram-sha-512

Even more secure variant:

```
host    all   all   0.0.0.0/0   scram-sha-512
```

### md5 (Legacy)

MD5 password hashing (not recommended):

```
host    all   all   0.0.0.0/0   md5
```

### peer (Local Only)

Use OS username for local connections:

```
local   all   all   peer
```

The connecting OS user must match the database username.

### trust (Dangerous!)

No authentication - for development only:

```
# WARNING: Never use in production!
local   all   all   trust
host    all   all   127.0.0.1/32   trust
```

### reject

Explicitly deny connections:

```
host    all   baduser   0.0.0.0/0   reject
host    all   all       10.0.0.0/8  reject
```

### cert

Require SSL client certificate:

```
hostssl   all   all   0.0.0.0/0   cert
```

---

## Rule Order

Rules are evaluated top to bottom. First match wins.

```
# Order matters!

# 1. Reject blocked user from anywhere
host    all   blocked   0.0.0.0/0        reject

# 2. Allow admin from office network
host    all   admin     192.168.1.0/24   scram-sha-256

# 3. Require SSL from external networks
hostssl all   all       0.0.0.0/0        scram-sha-256

# 4. Allow local connections
local   all   all                        peer
host    all   all       127.0.0.1/32     scram-sha-256
```

---

## Common Configurations

### Development (Permissive)

```
# Local connections - no password
local   all   all   trust

# Localhost - no password
host    all   all   127.0.0.1/32   trust
host    all   all   ::1/128        trust
```

### Production (Secure)

```
# Reject superuser remote login
host    all   postgres  0.0.0.0/0        reject
host    all   admin     0.0.0.0/0        reject

# Local connections use peer auth
local   all   all                        peer

# Localhost with password
host    all   all   127.0.0.1/32        scram-sha-256
host    all   all   ::1/128             scram-sha-256

# Require SSL from anywhere else
hostssl all   all   0.0.0.0/0           scram-sha-256
```

### Locked Down

```
# Only allow from specific IPs
host    all   all   192.168.1.100/32   scram-sha-256
host    all   all   192.168.1.101/32   scram-sha-256

# Reject everything else
host    all   all   0.0.0.0/0          reject
```

---

## Applying Changes

After editing hba.conf:

```bash
# Reload configuration
sudo systemctl reload scratchbird

# Or send SIGHUP
sudo kill -HUP $(cat /var/run/scratchbird/sb_server.pid)
```

---

## Troubleshooting

### "No matching rule"

Connection doesn't match any rule:

```
FATAL: no pg_hba.conf entry for host "192.168.1.50", user "myuser", database "mydb"
```

**Solution:** Add a matching rule to hba.conf.

### "Authentication failed"

Rule matched but authentication failed:

```
FATAL: password authentication failed for user "myuser"
```

**Solution:** Check password or authentication method.

### Test from Command Line

```bash
# Test connection
sb_isql -H 192.168.1.100 -P 3092 -U testuser -d testdb
```

### Check Applied Rules

```sql
-- View current connections and their auth
SELECT usename, client_addr, ssl FROM pg_stat_activity;
```

---

## Security Best Practices

1. **Use SCRAM-SHA-256** - Most secure password method
2. **Require SSL** for remote connections
3. **Block superuser** remote login
4. **Use specific IPs** not 0.0.0.0/0 when possible
5. **Never use trust** in production
6. **Regular audits** of hba.conf

---

## Next Steps

- [Enable SSL/TLS](ssl-setup.md)
- [Security best practices](../admin/security.md)
- [User management](../admin/user-management.md)
