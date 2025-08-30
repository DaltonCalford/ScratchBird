### Installation and Service

Systemd service (packaging/systemd/scratchbird.service):
- ExecStart: `/usr/bin/scratchbird`
- User/Group: `scratchbird`
- Restart policy: on-failure
- Limits: `LimitNOFILE=65536`
- Environment: `SCRATCHBIRD_LOG_LEVEL=info`

Packaging config (packaging/config/scratchbird.conf):
- BIND_ADDRESS, PORT, LOG_LEVEL, DATA_DIR, TLS_* (optional)

Typical steps:
1) Install binary as `/usr/bin/scratchbird` and create system user/group
2) Place `scratchbird.service` under `/etc/systemd/system/`
3) Place `scratchbird.conf` under `/etc/scratchbird/` (if used) and export env
4) `systemctl daemon-reload && systemctl enable --now scratchbird`

