### Installation and Service

What it is
- Instructions to run ScratchBird as a systemd service and configure its environment.

Why it matters
- Standardized deployment ensures consistent behavior across environments and easier operations.

How to use it
- Install the binary and unit file, configure environment, enable the service.

Systemd service (packaging/systemd/scratchbird.service):
- ExecStart: `/usr/bin/scratchbird`
- User/Group: `scratchbird`
- Restart policy: on-failure
- Limits: `LimitNOFILE=65536`
- Environment: `SCRATCHBIRD_LOG_LEVEL=info`

Packaging config (packaging/config/scratchbird.conf):
- BIND_ADDRESS, PORT, LOG_LEVEL, DATA_DIR, TLS_* (optional)

Typical steps:

See also
- [Configuration](./configuration.md) · [CLI tools](./cli-tools.md)
1) Install binary as `/usr/bin/scratchbird` and create system user/group
2) Place `scratchbird.service` under `/etc/systemd/system/`
3) Place `scratchbird.conf` under `/etc/scratchbird/` (if used) and export env
4) `systemctl daemon-reload && systemctl enable --now scratchbird`

