# Bootstrap Auth Manifest

This directory contains installable development/test authentication defaults.

`default_auth_manifest.json` is the repository source of truth for the
standardized default usernames/passwords used by:

- native ScratchBird bootstrap
- example/test database seed flows
- emulation compatibility fixtures

Only ScratchBird entries with `"seed_on_database_bootstrap": true` are created
automatically during core database initialization. Other entries remain in the
manifest so test/example build scripts can use the same credential contract
without changing engine bootstrap state.

The example database harness consumes this manifest through:

- `scripts/emulation/render_example_seed_sql.py`
- `scripts/example_db_manager.sh`
