# sb_isql - Command Identity

[sb_isql README](./README.md) | [CLI Guide README](../README.md)

## Synopsis

```
sb_isql [connection-options] [options] [database]
```

## Description

`sb_isql` is the native ScratchBird interactive SQL terminal. It provides a command-line interface for executing SQL commands and managing databases.

## Command Identity

| Attribute | Value |
|-----------|-------|
| Name | `sb_isql` |
| Purpose | Native SB interactive SQL shell |
| Protocol | SBWP v1.1 (native) |
| Emulation | None (native SB dialect) |
| Default Port | 3092 |

## Invocation Patterns

```bash
# Interactive mode
sb_isql -h localhost -U myuser -d mydb

# Single command
sb_isql -c "SELECT * FROM users" -d mydb

# File input
sb_isql -f script.sql -d mydb

# Piped input
cat script.sql | sb_isql -d mydb

# Environment variables
export SBHOST=localhost
export SBUSER=myuser
export SBDATABASE=mydb
sb_isql
```

## Connection String

```bash
# Full connection string
sb_isql "host=localhost port=3092 dbname=mydb user=myuser password=secret"

# With SSL
sb_isql "host=secure.example.com sslmode=require dbname=mydb"
```

## Options Summary

| Option | Description |
|--------|-------------|
| `-h, --host` | Server host |
| `-p, --port` | Server port |
| `-U, --username` | Username |
| `-d, --dbname` | Database name |
| `-c, --command` | Execute single command |
| `-f, --file` | Execute SQL from file |
| `-l, --list` | List databases |
| `-V, --version` | Version information |
| `-?, --help` | Help |

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Error in script |
| 2 | Connection failure |
| 3 | Authentication failure |

## Examples

```bash
# Connect to local database
sb_isql -d myapp

# Connect to remote server
sb_isql -h prod.db.internal -U admin -d production

# Execute command
sb_isql -c "SELECT COUNT(*) FROM users" -d myapp

# Run script
sb_isql -f setup.sql -d myapp

# List databases
sb_isql -l

# Verbose mode
sb_isql -v -d myapp
```

## See Also

- [Invocation and Syntax](02_invocation_and_syntax.md)
- [Options Reference](03_options_reference.md)
- [sb_pg_isql](../sb_pg_isql/README.md) - PostgreSQL mode
- [sb_my_isql](../sb_my_isql/README.md) - MySQL mode
