# Default Test Engine Credentials (Development Window)

This file defines the standardized default credentials used by ScratchBird test database build scripts.

These credentials are for development/test environments only.

## Admin Credentials

| Engine Surface | Username | Password |
| --- | --- | --- |
| ScratchBird (native) | `SysArch` | `replaceme` |
| PostgreSQL emulation | `postgres` | `postgres` |
| MySQL emulation | `root` | `root` |
| Firebird emulation | `SYSDBA` | `masterkey` |

## Regular (Minimal/Public) Credentials

| Engine Surface | Username | Password | Access Intent |
| --- | --- | --- | --- |
| ScratchBird (native) | `sb_public` | `sb_public` | Public/minimal |
| PostgreSQL emulation | `pg_public` | `pg_public` | Public/minimal |
| MySQL emulation | `my_public` | `my_public` | Public/minimal |
| Firebird emulation | `fb_public` | `fb_public` | Public/minimal |

## Canonical Identity Mapping Contract

The example compatibility harness persists deterministic mapping metadata in:

- `compat_identity_user_map_contract`

Default canonical IDs used for test assertions:

- `u_sys_admin` for admin principals
- `u_public_user` for regular/public principals

Canonical user labels in the contract fixture are identity aliases, not engine login names:

- `u_sys_admin` uses canonical alias `sys_admin`
- `u_public_user` uses canonical alias `public_user`

## Source of Truth in Repository

- `resources/bootstrap/default_auth_manifest.json`
- `scripts/emulation/render_example_seed_sql.py`
- `scripts/example_db_manager.sh`

The example database harness renders runtime bootstrap/post-bootstrap SQL from
the manifest. The checked-in SQL files under
`tests/compatibility/scratchbird/example_sql/` are reference mirrors, not the
authoritative executable source.
