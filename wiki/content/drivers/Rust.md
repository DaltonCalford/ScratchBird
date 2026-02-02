[Back to Drivers](Driver-Comparison.md) | [Back to Home](../Home.md)

# Rust Driver Guide

**Status:** Complete
**Last Updated:** 2026-01-30

---

## Overview

Rust applications can connect to ScratchBird through multiple protocols:

| Protocol | Port | Library | Best For |
|----------|------|---------|----------|
| Native | 3092 | scratchbird (SBWP v1.1) | Full ScratchBird feature set |
| PostgreSQL | 5432 | tokio-postgres | Ecosystem compatibility |
| MySQL | 3306 | mysql_async | MySQL migrations |
| Firebird | 3050 | Firebird drivers/ODBC | Firebird migrations |

**Recommendation:** Use the **ScratchBird native driver** for full SBWP v1.1 feature coverage. Use PostgreSQL/MySQL/Firebird drivers only when you need emulation compatibility.

---

## ScratchBird Native Driver (SBWP v1.1)

### Install

Add to `Cargo.toml`:

```toml
scratchbird = "0.1.0"
```

If the crate is not yet published, use a path dependency:

```toml
scratchbird = { path = "../ScratchBird-driver/rust" }
```

Packaging will be handled by the installation utility once distribution is finalized.

### Install via sb_setup (Installer Utility)

If you installed ScratchBird with the installer, you can add the native driver pack later:

```bash
sb_setup --interactive
```

Select `scratchbird-driver-rust` or the `scratchbird-drivers-all` meta package. On Linux, run with `sudo`.

### Quick Start

```rust
use scratchbird::{Client, Config};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut client = Client::new(Config::from_dsn(
        "scratchbird://user:pass@localhost:3092/mydb",
    )?);
    client.connect().await?;
    let result = client.query("SELECT 1").await?;
    println!("{:?}", result.rows[0][0]);
    client.close().await;
    Ok(())
}
```

The native driver uses SBWP v1.1 with server-side prepare/bind and binary-only parameters.
Wrapper types for JSONB/RANGE/GEOMETRY are exposed by the driver API.

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

- `SCRATCHBIRD_RUST_URL`
