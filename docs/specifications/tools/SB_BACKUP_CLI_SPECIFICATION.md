# sb_backup CLI Specification

Version: 1.0  
Status: Draft (Alpha networking)  
Last Updated: January 2026

## Purpose

Define the command-line interface for `sb_backup` with network-capable
backup/restore operations.

## Scope

- Command surface and options
- Remote vs local execution modes
- Output and error behavior

## Usage

```
sb_backup <command> [options] [args]
```

## Commands

- `create` (alias: `backup`)  
  Create a backup from a database to a backup file.
- `restore`  
  Restore a backup file into a database.
- `list`  
  List backups in a directory (client-side convenience).
- `verify`  
  Validate backup integrity.
- `info`  
  Show backup metadata.

## Options (Command-Specific)

**Create**
- `--compress`
- `--incremental`
- `--tables <t1,t2,...>`
- `--exclude <t1,t2,...>`
- `--schema-only`
- `--data-only`
- `--verbose`
- `--progress`

**Restore**
- `--overwrite`
- `--tables <t1,t2,...>`
- `--schema-only`
- `--data-only`
- `--no-owner`
- `--verbose`
- `--progress`

**Verify/Info/List**
- `--verbose`
- `--quiet`

## Connection Options

```
-H, --host <host>       Server hostname (default: localhost)
-p, --port <port>       Server port (default: 3092)
-U, --user <user>       Username
-P, --password <pass>   Password (prompted if omitted)
-d, --database <db>     Database name (optional if provided positionally)
--sslmode <mode>        disable|prefer|required
--timeout <seconds>     Connection timeout
--mode <remote|local|auto>
```

## Remote vs Local Semantics

- **Remote mode:** `sb_backup` connects to the native listener and requests
  backup/restore through the server. Backup data streams between client and
  server; backup files are local to the client.
- **Local mode:** operations run directly against local database files.
- **Auto mode:** prefer remote unless the database is local and config allows.

## Exit Codes

- `0`: success
- `1`: usage error
- `2`: operation failed

## Related Specs

- `docs/specifications/BACKUP_AND_RESTORE.md`
- `docs/specifications/tools/SB_TOOLING_NETWORK_SPEC.md`
- `docs/specifications/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md`
