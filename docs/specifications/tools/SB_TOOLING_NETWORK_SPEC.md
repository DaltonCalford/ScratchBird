# Network Tooling Specification (sb_backup/sb_verify/sb_security/sb_charset_loader)

Version: 1.0
Status: Draft (Alpha IP layer)
Last Updated: January 2026

## Purpose

Define network-capable behavior for ScratchBird tooling binaries that
perform maintenance, verification, and security operations.

## Scope

Tools covered:
- sb_backup
- sb_verify
- sb_security
- sb_charset_loader

These tools must support remote operation via ScratchBird native protocol.
Some tools may optionally spawn a local helper process.

## Connection Options (Common)

```
-h, --host <host>       Server hostname (default: localhost)
-p, --port <port>       Server port (default: 3092)
-U, --user <user>       Username
-W, --password          Prompt for password
-d, --database <db>     Database name
-r, --role <role>       Role name
--sslmode <mode>        disable|prefer|required
--timeout <seconds>     Connection timeout
```

## Execution Mode

```
--mode <remote|local|auto>
```

- remote: connect to sb_server and request operation over the wire
- local: spawn local helper process (requires filesystem access)
- auto: if database file is local and config allows, run local; else remote

## Security Rules

- Remote mode requires engine authentication and authorization.
- Local mode still enforces permissions via engine APIs.
- All operations are audited.

## Tool-Specific Notes

### sb_backup
- Remote mode streams backup through the server.
- Local mode runs backup directly against the database file.

### sb_verify
- Remote mode requests verification through the server.
- Local mode runs verification locally.

### sb_security
- Remote mode manages users/roles through server APIs.
- Local mode is allowed only for system administrators.

### sb_charset_loader
- Remote mode uploads charset data through server APIs.
- Local mode updates charset catalog directly.

## Related Specs

- docs/specifications/BACKUP_AND_RESTORE.md
- docs/specifications/network/ENGINE_PARSER_IPC_CONTRACT.md
- docs/specifications/Security Design Specification/02_IDENTITY_AUTHENTICATION.md
