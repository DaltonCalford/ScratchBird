# Migration Overview

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-09

ScratchBird supports Firebird-style MGA and emulates PostgreSQL/MySQL/Firebird
SQL dialects at the protocol/parsing layer.

**Key point:** Emulated databases are metadata-only schemas; no physical files
are created for emulated dialects.

See:
- `docs/user-documentation/migration/`
- `docs/specifications/` for engine behavior
