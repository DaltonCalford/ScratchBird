# Linux Installation (DEB)
Last modified: 2026-02-21

Install ScratchBird on Debian/Ubuntu-based distributions.

[Back to Installation Index](index.md) | [Back to Documentation Index](../index.md)

---

## Quick Install (0.1.0)

```bash
# Example local package path
cd ~/CliWork/ScratchBird/release/beta/runtime
sudo dpkg -i ./scratchbird_0.1.0_amd64.deb
sudo apt-get install -f

sudo systemctl enable scratchbird
sudo systemctl start scratchbird
```

## Post-Install Identity Contract

DEB install flow must create/validate:

- user/group: `scratchbird:scratchbird`
- `/var/lib/scratchbird`
- `/var/log/scratchbird`
- `/var/run/scratchbird`
- `/var/lib/scratchbird/bootstrap.token` (mode `0600`)

The package install hooks should use the same logic as:

- `tools/install/ensure-service-account.sh`

## Verify

```bash
id scratchbird
getent group scratchbird
stat -c '%U:%G %a %n' /var/lib/scratchbird /var/log/scratchbird /var/run/scratchbird
stat -c '%U:%G %a %n' /var/lib/scratchbird/bootstrap.token
sudo systemctl status scratchbird --no-pager
```

## Configuration

```ini
[server]
run_as_user = scratchbird
run_as_group = scratchbird
```

If either configured identity is missing, startup fails before listeners are opened.
