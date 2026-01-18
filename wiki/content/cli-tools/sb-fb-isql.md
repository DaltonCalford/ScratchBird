# sb_fb_isql

Firebird SQL interactive shell for ScratchBird.

[Back to CLI Tools](README.md) | [Back to Home](../Home.md)

---

## Synopsis

```
sb_fb_isql <database_path> [options]
```

---

## Description

`sb_fb_isql` is a Firebird-compatible SQL shell that uses the Firebird SQL
parser and compiler to generate SBLR and execute against the ScratchBird engine.
It is intended for Firebird emulation testing and Firebird SQL compatibility.

**What actually happens:**
- SQL is parsed by the FirebirdQueryCompiler and compiled to SBLR.
- SBLR is executed by the ScratchBird engine.
- Emulated metadata is written to the Firebird emulation catalog; no separate
  Firebird database files are created.

---

## Options

| Option | Description |
|--------|-------------|
| `-c, --command <sql>` | Execute a single command and exit |
| `-f, --file <file>` | Execute commands from file and exit |
| `-q, --quiet` | Quiet mode (no welcome message) |
| `-s, --dialect <n>` | SQL dialect (1, 2, or 3; default: 3) |
| `--stats` | Show compilation/execution statistics |
| `-h, --help` | Show help |
| `--version` | Show version |

---

## Examples

```bash
# Open a database file
sb_fb_isql /var/lib/scratchbird/mydb.sbdb

# Execute a command and exit
sb_fb_isql /var/lib/scratchbird/mydb.sbdb -c "SELECT FIRST 10 * FROM rdb$relations"

# Run a script
sb_fb_isql /var/lib/scratchbird/mydb.sbdb -f setup.sql

# Use dialect 1
sb_fb_isql /var/lib/scratchbird/mydb.sbdb -s 1
```
