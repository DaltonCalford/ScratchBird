# Debug Guide

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

## Run in Foreground

```bash
sb_server --foreground
```

## Verify Ports

```bash
ss -ltn | rg '3092|5432|3306|3050'
```

## Check Configuration

- Config file: `../configuration/sb_server.conf.md`
- HBA rules: `../configuration/hba.conf.md`

## Capture Logs

If running as a service, check system logs:

```bash
journalctl -u scratchbird -n 200
```

## Report a Bug

Capture:
- exact command line
- full error output
- config files in use

Then file an issue: https://github.com/scratchbird/scratchbird/issues
