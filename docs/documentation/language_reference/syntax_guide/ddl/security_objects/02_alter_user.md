# ALTER USER

[Prev](./01_create_user.md) | [Next](./03_drop_user.md) | [Topic README](./README.md)

## Synopsis

Modifies an existing database user.

## Syntax

```sql
ALTER USER [ environment_path ] user_name [ WITH ] option [ ... ]

where option can be:
    SUPERUSER | NOSUPERUSER
    | CREATEDB | NOCREATEDB
    | CREATEROLE | NOCREATEROLE
    | INHERIT | NOINHERIT
    | LOGIN | NOLOGIN
    | REPLICATION | NOREPLICATION
    | BYPASSRLS | NOBYPASSRLS
    | CONNECTION LIMIT connlimit
    | [ ENCRYPTED ] PASSWORD 'password' | PASSWORD NULL
    | VALID UNTIL 'timestamp'
    | PROFILE profile_name
    | AUTH plugin_name [ USING 'auth_options' ]
```

## Description

ALTER USER changes the attributes of an existing user. Any option not mentioned remains unchanged.

## Examples

### Change Password

```sql
ALTER USER john WITH PASSWORD 'new_secure_password';
```

### Grant Privileges

```sql
-- Grant CREATEDB privilege
ALTER USER app_user WITH CREATEDB;

-- Make superuser
ALTER USER admin WITH SUPERUSER;

-- Grant replication
ALTER USER replicator WITH REPLICATION;
```

### Restrict Account

```sql
-- Disable login
ALTER USER former_employee WITH NOLOGIN;

-- Set connection limit
ALTER USER app_user WITH CONNECTION LIMIT 50;

-- Set password expiration
ALTER USER temp_user WITH VALID UNTIL '2024-12-31';
```

### Change Authentication

```sql
-- Switch to LDAP
ALTER USER corp_user WITH AUTH ldap USING 'cn=newcn,dc=company';
```

### Rename User

```sql
ALTER USER old_name RENAME TO new_name;
```

## Notes

- Only superusers can alter other superusers
- Users can change their own passwords
- Changes take effect immediately for new connections
- Existing connections retain previous settings

## See Also

- [CREATE USER](01_create_user.md)
- [DROP USER](03_drop_user.md)
