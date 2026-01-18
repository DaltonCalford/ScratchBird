# sb_my_isql

MySQL wire protocol shell for ScratchBird emulation.

[Back to CLI Tools](README.md) | [Back to Home](../Home.md)

---

## Synopsis

```
sb_my_isql [OPTIONS] [database]
```

---

## Description

`sb_my_isql` is a minimal mysql-compatible client used to test the MySQL
wire protocol against ScratchBird's MySQL listener.

**What actually happens:**
- The client speaks MySQL protocol to the ScratchBird listener.
- The MySQL parser translates SQL into SBLR.
- The engine executes SBLR and the parser formats MySQL-compatible results.

---

## Options

| Option | Description |
|--------|-------------|
| `-h, --host <host>` | Server host (default: localhost) |
| `-P, --port <port>` | Server port (default: 3306) |
| `-u, --user <user>` | Username (default: root) |
| `-p, --password[=pass]` | Password (prompt if not provided) |
| `-D, --database <db>` | Database name |
| `-f, --file <file>` | Execute commands from file |
| `-e, --execute <sql>` | Execute a single command |
| `-o, --output <file>` | Write output to file |
| `-t, --tuples-only` | Print rows only (no headers) |
| `-A, --no-align` | Unaligned output mode |
| `-F, --field-separator <s>` | Field separator for unaligned output |
| `-q, --silent` | Quiet mode |
| `--help` | Show help |

---

## Examples

```bash
# Connect to MySQL listener
sb_my_isql -h localhost -P 3306 -u root -D app_db

# Execute a command
sb_my_isql -h localhost -P 3306 -u root -D app_db -e "SELECT VERSION()"

# Run a script
sb_my_isql -h localhost -P 3306 -u root -D app_db -f schema.sql
```
