# sb_isql

Interactive SQL shell for ScratchBird.

[Back to Tools Index](index.md) | [Back to Documentation Index](../index.md)

---

## Synopsis

```
sb_isql [OPTIONS] [DATABASE]
```

---

## Description

`sb_isql` is an interactive command-line tool for executing SQL queries against ScratchBird. It provides features like command history, tab completion, and formatted output.

---

## Connection Options

| Option | Description |
|--------|-------------|
| `-H, --host HOST` | Server hostname (default: localhost) |
| `-p, --port PORT` | Server port (default: 5433) |
| `-U, --user USER` | Username |
| `-P, --password` | Prompt for password |
| `-d, --database DB` | Database name |

---

## Output Options

| Option | Description |
|--------|-------------|
| `-t, --tuples-only` | Print data only, no headers |
| `-q, --quiet` | Suppress welcome message |
| `-x, --expanded` | Expanded table format |
| `-F, --field-separator SEP` | Field separator (default: \|) |
| `-H, --html` | HTML output format |

---

## Execution Options

| Option | Description |
|--------|-------------|
| `-c, --command SQL` | Execute SQL and exit |
| `-f, --file FILE` | Execute SQL from file |
| `-i, --input FILE` | Read input from file |
| `-o, --output FILE` | Write output to file |

---

## Schema Extraction

| Option | Description |
|--------|-------------|
| `-a` | Extract all schema (EXTRACT_ALL mode) |
| `-x` | Extract schema excluding system tables |
| `-ex` | Extended extraction mode |

---

## Other Options

| Option | Description |
|--------|-------------|
| `--sql-dialect N` | SQL dialect (1, 2, or 3) |
| `--bail` | Exit on first error |
| `--echo` | Echo commands before execution |
| `--help` | Show help |
| `--version` | Show version |

---

## Usage Examples

### Interactive Session

```bash
sb_isql -H localhost -U admin mydb
```

### Single Command

```bash
sb_isql -H localhost -c "SELECT * FROM users"
```

### Execute File

```bash
sb_isql -H localhost -f schema.sql mydb
```

### Quiet Output

```bash
sb_isql -H localhost -tq -c "SELECT COUNT(*) FROM users"
```

### Save Output

```bash
sb_isql -H localhost -c "SELECT * FROM orders" -o results.txt
```

---

## Meta-Commands

### Help and Exit

| Command | Description |
|---------|-------------|
| `\?` | Show help |
| `\q` | Quit |
| `\h [COMMAND]` | SQL help |

### Database Information

| Command | Description |
|---------|-------------|
| `\l` | List databases |
| `\c DATABASE` | Connect to database |
| `\d` | List tables |
| `\d TABLE` | Describe table |
| `\dt` | List tables with details |
| `\di` | List indexes |
| `\dv` | List views |
| `\du` | List users |
| `\ds` | List sequences |
| `\df` | List functions |

### Input/Output

| Command | Description |
|---------|-------------|
| `\i FILE` | Execute file |
| `\o FILE` | Send output to file |
| `\o` | Stop sending to file |
| `\! COMMAND` | Execute shell command |
| `\e` | Edit query in $EDITOR |

### Display Settings

| Command | Description |
|---------|-------------|
| `\x` | Toggle expanded display |
| `\timing [on|off]` | Toggle timing display |
| `\pset OPTION VALUE` | Set output option |

---

## SET Commands

### Query Behavior

| Command | Description |
|---------|-------------|
| `SET BAIL ON/OFF` | Stop on error |
| `SET ECHO ON/OFF` | Echo commands |
| `SET COUNT ON/OFF` | Show row count |
| `SET STATS ON/OFF` | Show statistics |
| `SET PLAN ON/OFF` | Show query plan |
| `SET PLANONLY ON/OFF` | Plan only, don't execute |

### Output Format

| Command | Description |
|---------|-------------|
| `SET HEADING ON/OFF` | Show column headers |
| `SET LIST ON/OFF` | List format output |
| `SET NULL 'text'` | NULL display string |
| `SET WIDTH N` | Column width |
| `SET MAXROWS N` | Max rows to display |

### Session Settings

| Command | Description |
|---------|-------------|
| `SET SQL DIALECT N` | SQL dialect (1, 2, 3) |
| `SET AUTODDL ON/OFF` | Auto-commit DDL |
| `SET LOCAL_TIMEOUT N` | Statement timeout |
| `SET NAMES charset` | Character set |

### View Settings

```sql
-- Show all settings
\set

-- Show specific
SHOW sql_dialect;
```

---

## Interactive Features

### Command History

- **Up/Down arrows** - Navigate history
- **Ctrl+R** - Reverse search
- History saved to `~/.sb_isql_history`
- Maximum 1000 entries

### Line Editing

- **Ctrl+A** - Beginning of line
- **Ctrl+E** - End of line
- **Ctrl+K** - Delete to end
- **Ctrl+U** - Delete to beginning
- **Ctrl+W** - Delete word
- **Tab** - Auto-complete

### Multi-line Input

Statements continue until semicolon:

```sql
sb_isql> SELECT
sb_isql>   id,
sb_isql>   name
sb_isql> FROM users
sb_isql> WHERE active = true;
```

---

## Output Formats

### Standard (Table)

```
 id | name  | email
----+-------+-------------------
  1 | Alice | alice@example.com
  2 | Bob   | bob@example.com
```

### Expanded (`\x`)

```
-[ RECORD 1 ]--------------------
id    | 1
name  | Alice
email | alice@example.com
-[ RECORD 2 ]--------------------
id    | 2
name  | Bob
email | bob@example.com
```

### Tuples Only (`-t`)

```
1|Alice|alice@example.com
2|Bob|bob@example.com
```

### HTML (`--html`)

```html
<table>
<tr><th>id</th><th>name</th><th>email</th></tr>
<tr><td>1</td><td>Alice</td><td>alice@example.com</td></tr>
</table>
```

---

## Scripting

### Non-Interactive Script

```bash
#!/bin/bash
sb_isql -H localhost -U admin -tq mydb << 'EOF'
SELECT COUNT(*) FROM orders WHERE status = 'pending';
EOF
```

### With Variables

```bash
#!/bin/bash
DATE=$(date +%Y-%m-%d)
sb_isql -H localhost -U admin mydb -c \
    "SELECT * FROM orders WHERE created_at >= '${DATE}'"
```

### Batch Processing

```bash
# Process multiple files
for f in migrations/*.sql; do
    echo "Running $f..."
    sb_isql -H localhost -d mydb -f "$f"
done
```

---

## Connection String

Alternative to separate options:

```bash
# Using environment variables
export SBHOST=localhost
export SBPORT=3092
export SBUSER=admin
sb_isql mydb

# PostgreSQL-style also works
export PGHOST=localhost
export PGPORT=5432
export PGUSER=admin
sb_isql mydb
```

---

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | SQL error (with --bail) |
| 2 | Connection error |
| 3 | File not found |

---

## Configuration File

Create `~/.sb_isqlrc` for defaults:

```sql
-- Default settings
SET TIMING ON
SET HEADING ON
\pset null '(null)'
```

---

## Tips

### Quick Row Count

```bash
sb_isql -H localhost -tAc "SELECT COUNT(*) FROM large_table"
```

### CSV Export

```bash
sb_isql -H localhost -tF',' -c "SELECT * FROM users" > users.csv
```

### Table Definition

```bash
sb_isql -H localhost -c "\d users"
```

### Performance Check

```sql
\timing on
SELECT * FROM slow_query;
```

---

## See Also

- [First Connection](../getting-started/first-connection.md)
- [Basic SQL](../getting-started/basic-sql.md)
- [psql Compatibility](../connectivity/postgresql-clients.md)
