# Network Tooling Specification (sb_backup/sb_verify/sb_security/sb_charset_loader)

Version: 1.0
Status: Draft (Alpha IP layer)
Last Updated: January 2026

## Purpose

Define network-capable behavior for ScratchBird tooling binaries that
perform maintenance, verification, and security operations using the
listener/pool architecture.

## Scope

Tools covered:
- sb_backup
- sb_verify
- sb_security
- sb_charset_loader

These tools must support remote operation via the ScratchBird native listener.
Some tools may optionally spawn a local helper process.

## Connection Options (Common)

```
-H, --host <host>       Server hostname (default: localhost)
-p, --port <port>       Server port (default: 3092)
-U, --user <user>       Username
-P, --password <pass>   Password (prompted if omitted)
-d, --database <db>     Database name (optional, tool-specific)
-r, --role <role>       Role name (optional)
--sslmode <mode>        disable|prefer|required
--timeout <seconds>     Connection timeout
```

## Execution Mode

```
--mode <remote|local|auto>
```

- remote: connect to sb_listener_native and request operation over the wire
- local: spawn local helper process (requires filesystem access)
- auto: if database is local and config allows, run local; else remote

## Listener Integration

- Tools connect to the native listener managed by the server.
- If a shared listener is enabled, tools may target that port instead.
- If the native listener is disabled, tools must fail with a clear error.

## Database Targeting

- In remote mode, `<database>` is a ScratchBird database name/alias.
- In local mode, `<database>` may be a filesystem path.

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
- Current status: loader tool is deprecated in code and needs restoration or
  replacement before this network spec can be implemented.

## Related Specs

- docs/specifications/BACKUP_AND_RESTORE.md
- docs/specifications/network/ENGINE_PARSER_IPC_CONTRACT.md
- docs/specifications/Security Design Specification/02_IDENTITY_AUTHENTICATION.md
