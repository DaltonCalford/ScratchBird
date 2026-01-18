# AppImage Installation

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

Use the AppImage when you want a single-file Linux build without system-wide
installation.

## Steps

1) Download the latest AppImage from the releases page.
2) Make it executable.
3) Run it.

```bash
# Download (example)
curl -LO https://github.com/scratchbird/scratchbird/releases/latest/download/scratchbird.AppImage

# Make executable
chmod +x scratchbird.AppImage

# Run (starts sb_server)
./scratchbird.AppImage
```

## Notes

- Default native port: `3092`
- Config file: [sb_server.conf](../configuration/sb_server.conf.md)
- For PostgreSQL/MySQL/Firebird emulation ports, see [sb_server](../cli-tools/sb-server.md).

## Troubleshooting

If the AppImage fails to run, confirm FUSE is available:

```bash
sudo modprobe fuse
```

See [Installation Issues](../troubleshooting/Installation-Issues.md) for more.
