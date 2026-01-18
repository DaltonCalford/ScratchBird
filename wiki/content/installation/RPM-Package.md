# RPM Package Installation

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

Install ScratchBird on Fedora/RHEL/CentOS using the `.rpm` package.

## Install

1) Download the latest `.rpm` from releases.
2) Install with `dnf` or `yum`.

```bash
curl -LO https://github.com/scratchbird/scratchbird/releases/latest/download/scratchbird.rpm
sudo dnf install ./scratchbird.rpm
# or
sudo yum install ./scratchbird.rpm
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

## Troubleshooting

See [Installation Issues](../troubleshooting/Installation-Issues.md) for common issues.
