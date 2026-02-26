# Install Helpers

## `ensure-service-account.sh`

Creates and validates the `scratchbird` service group/user (or provided names), then enforces ownership and mode policy for:

- `/var/lib/scratchbird`
- `/var/log/scratchbird`
- `/var/run/scratchbird`

It also creates a one-time bootstrap token file at `/var/lib/scratchbird/bootstrap.token` (or `SCRATCHBIRD_BOOTSTRAP_TOKEN_FILE`) with mode `0600` when missing.

Run as root:

```bash
sudo tools/install/ensure-service-account.sh
```

## `seed-example-database.sh`

Creates (or refreshes) the static, seeded example database used by installer/test/demo workflows.
The script uses `scripts/example_db_manager.sh static-refresh` and writes connection profiles to:

- `<example-root>/profiles/runtime.env`
- `<example-root>/profiles/connections.json`

Defaults:

- service user/group: `scratchbird:scratchbird`
- static example root: `/var/lib/scratchbird/example`

Run as root after the service account setup:

```bash
sudo tools/install/seed-example-database.sh
```
