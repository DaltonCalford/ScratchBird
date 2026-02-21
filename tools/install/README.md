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
