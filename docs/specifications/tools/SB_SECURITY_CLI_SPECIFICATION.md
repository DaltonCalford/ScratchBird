# sb_security CLI Specification

Version: 1.0  
Status: Draft (Alpha networking)  
Last Updated: January 2026

## Purpose

Define the command-line interface for `sb_security` with network-capable
security administration (users, roles, grants, audit).

## Scope

- Security command surface and options
- Remote vs local execution modes
- Output and error behavior

## Usage

```
sb_security <command> [options] [args]
```

## Commands

User management:
- `user list` (alias: `list-users`)
- `user create <username>` (alias: `create-user`)
- `user delete <username>` (alias: `drop-user`)
- `user password <username>` (alias: `password`)
- `user lock <username>` (alias: `lock-user`)
- `user unlock <username>` (alias: `unlock-user`)

Role management:
- `role list` (alias: `list-roles`)
- `role create <rolename>`
- `role delete <rolename>`
- `role grant <role> <user>`
- `role revoke <role> <user>`

Privileges:
- `grant <privilege> on <object> to <user|role>`
- `revoke <privilege> on <object> from <user|role>`
- `show grants for <user|role>`
- `show grants on <object>`

Audit:
- `audit status`
- `audit enable [category]`
- `audit disable [category]`
- `audit log [filter]`

## Options

```
-H, --host <host>       Server hostname (default: localhost)
-p, --port <port>       Server port (default: 3092)
-U, --user <user>       Admin username
-P, --password <pass>   Password (prompted if omitted)
--sslmode <mode>        disable|prefer|required
--timeout <seconds>     Connection timeout
--mode <remote|local|auto>
-v, --verbose           Verbose output
-q, --quiet             Errors only
--json                  JSON output format
```

## Remote vs Local Semantics

- **Remote mode:** commands execute through the server and follow RBAC rules.
- **Local mode:** allowed only for system administrators with local access.
- **Auto mode:** prefer remote unless local-only policy is configured.

## Exit Codes

- `0`: success
- `1`: usage error
- `2`: command failed or permission denied

## Related Specs

- `docs/specifications/Security Design Specification/03_AUTHORIZATION_MODEL.md`
- `docs/specifications/Security Design Specification/08_AUDIT_COMPLIANCE.md`
- `docs/specifications/tools/SB_TOOLING_NETWORK_SPEC.md`
