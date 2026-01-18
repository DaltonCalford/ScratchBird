# sb_security

Security management utility.

[Back to CLI Tools](README.md) | [Back to Home](../Home.md)

---

## Synopsis

```
sb_security <command> [OPTIONS] [ARGS]
```

---

## Description

`sb_security` manages users, passwords, roles, and security settings. It provides command-line access to security administration tasks.

---

## Commands

| Command | Description |
|---------|-------------|
| `list-users` | List all users |
| `create-user` | Create new user |
| `drop-user` | Delete user |
| `password` | Change password |
| `lock-user` | Lock user account |
| `unlock-user` | Unlock user account |
| `list-roles` | List all roles |
| `grant` | Grant role to user |
| `revoke` | Revoke role from user |
| `audit` | View security audit log |

---

## Connection Options

| Option | Description |
|--------|-------------|
| `-H, --host HOST` | Server hostname |
| `-P, --port PORT` | Server port |
| `-U, --user USER` | Admin username |
| `-p, --password` | Prompt for password |

---

## User Management

### List Users

```bash
sb_security list-users
```

Output:
```
Users:
  admin       Superuser  Active   Created: 2024-01-01
  appuser     Login      Active   Created: 2024-01-15
  readonly    Login      Active   Created: 2024-01-20
  tempuser    Login      Locked   Created: 2024-02-01
```

### Create User

```bash
# Interactive password prompt
sb_security create-user newuser

# With options
sb_security create-user newuser --superuser
sb_security create-user newuser --createdb
sb_security create-user newuser --connection-limit 10
```

### Drop User

```bash
sb_security drop-user olduser

# Force drop (reassign objects first)
sb_security drop-user olduser --force
```

---

## Password Management

### Change Password

```bash
# Interactive prompt
sb_security password username

# Generate random password
sb_security password username --generate

# Set specific password (not recommended for scripts)
sb_security password username --new-password
```

### Password Policy

```bash
# View policy
sb_security password-policy

# Set policy
sb_security password-policy --min-length 16 --require-special
```

---

## Account Locking

### Lock User

```bash
sb_security lock-user baduser
```

Output:
```
User 'baduser' has been locked.
```

### Unlock User

```bash
sb_security unlock-user baduser
```

Output:
```
User 'baduser' has been unlocked.
```

### View Locked Users

```bash
sb_security list-users | grep Locked
```

---

## Role Management

### List Roles

```bash
sb_security list-roles
```

Output:
```
Roles:
  admin          Superuser
  developers     Login
  readonly       -
  data_analysts  Login
```

### Grant Role

```bash
# Grant role to user
sb_security grant developers myuser

# Grant database access
sb_security grant --database mydb --privilege ALL myuser
```

### Revoke Role

```bash
sb_security revoke developers myuser
```

---

## Audit Log

### View Audit Log

```bash
# Recent entries
sb_security audit --tail 50

# Filter by user
sb_security audit --user admin

# Filter by action
sb_security audit --action LOGIN_FAILED

# Date range
sb_security audit --since "2024-01-01" --until "2024-01-31"
```

Output:
```
2024-01-15 10:23:45  admin       LOGIN         192.168.1.100  Success
2024-01-15 10:25:12  unknown     LOGIN_FAILED  192.168.1.200  Invalid password
2024-01-15 11:00:00  admin       CREATE_USER   localhost      Created 'appuser'
```

### Audit Actions

| Action | Description |
|--------|-------------|
| `LOGIN` | Successful login |
| `LOGIN_FAILED` | Failed login attempt |
| `LOGOUT` | User disconnected |
| `CREATE_USER` | User created |
| `DROP_USER` | User deleted |
| `PASSWORD_CHANGE` | Password changed |
| `GRANT` | Privilege granted |
| `REVOKE` | Privilege revoked |

---

## Security Reports

### Failed Login Report

```bash
sb_security audit --action LOGIN_FAILED --since "24 hours ago" | \
    awk '{print $4}' | sort | uniq -c | sort -rn
```

Output:
```
  15  192.168.1.200
   3  10.0.0.50
   1  172.16.0.100
```

### User Activity

```bash
sb_security audit --user admin --tail 20
```

---

## Bulk Operations

### Import Users

```bash
# From CSV
cat users.csv | while IFS=, read user email; do
    sb_security create-user "$user"
done
```

### Export Users

```bash
sb_security list-users --format csv > users_backup.csv
```

---

## Integration Examples

### Password Rotation Script

```bash
#!/bin/bash
# Rotate service account passwords

ACCOUNTS="webapp api_service batch_worker"

for user in $ACCOUNTS; do
    NEW_PASS=$(sb_security password $user --generate --quiet)
    # Update application config
    echo "$user:$NEW_PASS" >> /secure/passwords.txt
done
```

### Monitor Failed Logins

```bash
#!/bin/bash
# Alert on excessive failed logins

COUNT=$(sb_security audit --action LOGIN_FAILED --since "1 hour ago" | wc -l)

if [ $COUNT -gt 10 ]; then
    echo "ALERT: $COUNT failed logins in last hour" | \
        mail -s "Security Alert" admin@example.com
fi
```

---

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | General error |
| 2 | Connection error |
| 3 | Authentication error |
| 4 | User not found |
| 5 | Permission denied |

---

## Security Considerations

1. **Run as admin** - Most commands require superuser
2. **Secure output** - Don't log passwords
3. **Audit trail** - All actions are logged
4. **Network security** - Use SSL for remote connections
5. **Password handling** - Use `--generate` in scripts

---

## Troubleshooting

### "Permission denied"

```bash
# Must connect as admin
sb_security -U admin -H localhost list-users
```

### "User not found"

```bash
# List users to verify name
sb_security list-users | grep partial_name
```

### "Cannot drop user"

```bash
# User has objects, use --force
sb_security drop-user olduser --force
```

---

## See Also

- [User Management](../admin/user-management.md)
- [Security Guide](../admin/security.md)
- [Authentication](../configuration/hba.conf.md)
