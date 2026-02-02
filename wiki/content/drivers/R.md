[Back to Drivers](Driver-Comparison.md) | [Back to Home](../Home.md)

# R Driver Guide

**Status:** Complete
**Last Updated:** 2026-01-30

---

## Overview

R applications can connect to ScratchBird through multiple protocols:

| Protocol | Port | Library | Best For |
|----------|------|---------|----------|
| Native | 3092 | scratchbird (SBWP v1.1) | Full ScratchBird feature set |
| PostgreSQL | 5432 | RPostgres | Ecosystem compatibility |
| MySQL | 3306 | RMariaDB | MySQL migrations |
| Firebird | 3050 | Firebird drivers/ODBC | Firebird migrations |

**Recommendation:** Use the **ScratchBird native driver** for full SBWP v1.1 feature coverage. Use PostgreSQL/MySQL/Firebird drivers only when you need emulation compatibility.

---

## ScratchBird Native Driver (SBWP v1.1)

### Install

From the ScratchBird-driver repo:

```r
# Run from the repo root
install.packages("r", repos = NULL, type = "source")
```

Packaging will be handled by the installation utility once distribution is finalized.

### Install via sb_setup (Installer Utility)

If you installed ScratchBird with the installer, you can add the native driver pack later:

```bash
sb_setup --interactive
```

Select `scratchbird-driver-r` or the `scratchbird-drivers-all` meta package. On Linux, run with `sudo`.

### Quick Start

```r
library(DBI)
library(scratchbird)

con <- dbConnect(Scratchbird(), "scratchbird://user:pass@localhost:3092/mydb")
res <- dbGetQuery(con, "SELECT 1")
dbDisconnect(con)
```

The native driver uses SBWP v1.1 with server-side prepare/bind and binary-only parameters.
Wrapper types for JSONB/RANGE/GEOMETRY are exposed via `sb_jsonb`, `sb_range`, and `sb_geometry`.

## Connection Strings

URI:

```
scratchbird://user:password@host:3092/database?sslmode=require
```

Key-value:

```
host=localhost port=3092 dbname=mydb user=myuser password=mypass
```

See [DSN and config standard](../specifications/DRIVER_DSN_AND_CONFIG_STANDARD.md).

## TLS

TLS 1.3 is required. `sslmode=disable` is rejected.

## Tests

Integration tests are gated by:

- `SCRATCHBIRD_R_URL`
