# Command-Line Tools

ScratchBird command-line utilities reference.

**Last Updated:** 2026-02-03

[Back to Home](../Home.md)

---

## Status

CLI tools are distributed separately. This wiki documents engine behavior.


---

## Common Options

Most tools share these options:

| Option | Description |
|--------|-------------|
| `-H, --host` | Server hostname |
| `-P, --port` | Server port |
| `-U, --user` | Username |
| `-p, --password` | Password (or prompt) |
| `-d, --database` | Database name |
| `-v, --verbose` | Verbose output |
| `-q, --quiet` | Minimal output |
| `--help` | Show help |
| `--version` | Show version |

---

## Environment Variables

Tools read these environment variables:

| Variable | Description |
|----------|-------------|
| `SBHOST` | Default host |
| `SBPORT` | Default port |
| `SBUSER` | Default username |
| `SBDATABASE` | Default database |

PostgreSQL-compatible variables also work:
- `PGHOST`, `PGPORT`, `PGUSER`, `PGDATABASE`
