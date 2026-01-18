# sb_pg_isql

PostgreSQL wire protocol shell for ScratchBird emulation.

[Back to CLI Tools](README.md) | [Back to Home](../Home.md)

---

## Synopsis

```
sb_pg_isql [OPTION]... [DBNAME [USERNAME]]
```

---

## Description

`sb_pg_isql` is a minimal psql-compatible client used to test the PostgreSQL
wire protocol against ScratchBird's PostgreSQL listener.

**What actually happens:**
- The client speaks PostgreSQL FE/BE protocol to the ScratchBird listener.
- The PostgreSQL parser translates SQL into SBLR.
- The engine executes SBLR and the parser formats PostgreSQL-compatible results.

---

## Options

| Option | Description |
|--------|-------------|
| `-h, --host <host>` | Server host (default: localhost) |
| `-p, --port <port>` | Server port (default: 5432) |
| `-U, --user <user>` | Username |
| `-d, --dbname <db>` | Database name |
| `-f, --file <file>` | Execute commands from file |
| `-c, --command <sql>` | Execute a single command |
| `-o, --output <file>` | Write output to file |
| `-t, --tuples-only` | Print rows only (no headers) |
| `-A, --no-align` | Unaligned output mode |
| `-F, --field-separator <s>` | Field separator for unaligned output |
| `-q, --quiet` | Quiet mode |
| `-W, --password` | Force password prompt |
| `-?, --help` | Show help |

---

## Examples

```bash
# Connect to PostgreSQL listener
sb_pg_isql -h localhost -p 5432 -U app_user -d app_db

# Run a script
sb_pg_isql -h localhost -p 5432 -U app_user -d app_db -f schema.sql

# Execute a command
sb_pg_isql -h localhost -p 5432 -U app_user -d app_db -c "SELECT version()"
```
