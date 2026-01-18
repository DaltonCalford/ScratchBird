# DEB Package Installation

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

Install ScratchBird on Debian/Ubuntu using the `.deb` package.

## Install

1) Download the latest `.deb` from releases.
2) Install with `dpkg` and resolve dependencies.

```bash
curl -LO https://github.com/scratchbird/scratchbird/releases/latest/download/scratchbird.deb
sudo dpkg -i scratchbird.deb
sudo apt-get -f install
```

## Service Management

```bash
sudo systemctl status scratchbird
sudo systemctl start scratchbird
sudo systemctl stop scratchbird
```

## Notes

- Default native port: `3092`
- Config: [sb_server.conf](../configuration/sb_server.conf.md)
- Logs: `/var/log/scratchbird/` (if package installs logging)

## Troubleshooting

See [Installation Issues](../troubleshooting/Installation-Issues.md) for common issues.
