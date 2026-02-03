# sb_admin

Server administration utility for scheduler and metrics queries.

**Status:** Available with supported distributions.
**Last Updated:** 2026-02-03

[Back to CLI Tools](README.md) | [Back to Home](../Home.md)

---

## Synopsis

```
sb_admin <database> job list [--like <pattern>] [OPTIONS]
sb_admin <database> job runs <job_name> [OPTIONS]
sb_admin <database> metrics [OPTIONS]
```

---

## Description

`sb_admin` connects to the configured listener and issues scheduler/metrics queries
using admin credentials.

---

## Commands

### Scheduler

| Command | Description |
|---------|-------------|
| `job list` | List jobs (optional LIKE filter) |
| `job runs <job_name>` | Show runs for a named job |

### Metrics

| Command | Description |
|---------|-------------|
| `metrics` | Emit `SHOW METRICS` output |

---

## Connection Options

| Option | Description |
|--------|-------------|
| `-U, --user USER` | Admin username |
| `-P, --password PASS` | Admin password |
| `-p, --port PORT` | TCP port (default: 3092) |
| `--database NAME` | Database name (if not supplied positionally) |
| `-q, --quiet` | Only show errors |

---

## Examples

```bash
# List jobs
sb_admin mydb job list -U SYSARCH -P ScratchBirdBeta1!

# Filter jobs by name
sb_admin mydb job list --like "daily%" -U SYSARCH -P ScratchBirdBeta1!

# Show runs for a job
sb_admin mydb job runs daily_sweep -U SYSARCH -P ScratchBirdBeta1!

# Emit metrics
sb_admin mydb metrics -U SYSARCH -P ScratchBirdBeta1!
```
