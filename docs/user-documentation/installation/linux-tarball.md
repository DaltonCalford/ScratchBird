# Linux Installation (Tarball)
Last modified: 2026-02-21

Install ScratchBird 0.1.0 from a local tarball package.

[Back to Installation Index](index.md) | [Back to Documentation Index](../index.md)

---

## When to Use Tarball

Use tarball installation when you need:

- distro-independent deployment
- side-by-side versions
- controlled local release artifact installs

## Quick Install (0.1.0)

```bash
# Example local artifact path (adjust filename as needed)
cd ~/CliWork/ScratchBird/release/beta/runtime
sudo tar -xzf scratchbird-0.1.0-linux-amd64.tar.gz -C /opt

# Install service identity + directory ownership + bootstrap token
sudo /opt/scratchbird/tools/install/ensure-service-account.sh

# Install config and service files
sudo install -d -m 0755 /etc/scratchbird
sudo cp /opt/scratchbird/etc/scratchbird/sb_server.conf.example /etc/scratchbird/sb_server.conf
sudo cp /opt/scratchbird/etc/systemd/scratchbird.service /etc/systemd/system/scratchbird.service

# Enable/start
sudo systemctl daemon-reload
sudo systemctl enable scratchbird
sudo systemctl start scratchbird
```

## Identity and Directory Contract

Tarball installs must enforce:

- user/group: `scratchbird:scratchbird`
- state/log/run directory mode and ownership policy
- bootstrap token file creation when missing:
  - default path: `/var/lib/scratchbird/bootstrap.token`
  - mode: `0600`
  - override env: `SCRATCHBIRD_BOOTSTRAP_TOKEN_FILE`

## Required Config Keys

```ini
[server]
run_as_user = scratchbird
run_as_group = scratchbird

[network]
unix_socket = /var/run/scratchbird/sb.sock
```

## Verify Installation

```bash
id scratchbird
getent group scratchbird

stat -c '%U:%G %a %n' /var/lib/scratchbird /var/log/scratchbird /var/run/scratchbird
stat -c '%U:%G %a %n' /var/lib/scratchbird/bootstrap.token

sudo systemctl status scratchbird --no-pager
```

Expected:

1. service account exists
2. directories owned by `scratchbird:scratchbird`
3. bootstrap token mode `600`
4. service starts without permission errors
