# Migration Guide

## Scope
This driver speaks **ScratchBird native protocol only** (port 3092). For PostgreSQL/MySQL/Firebird compatibility, use those ecosystems' native drivers against the ScratchBird protocol listeners.

Wrapper types: JSONB uses `scratchbird.types.Jsonb`, RANGE uses `scratchbird.types.Range`, GEOMETRY uses `scratchbird.types.Geometry`. Use these wrappers when binding parameters or reading results.

## Migration steps
1. Update connection string to use `scratchbird://` and port 3092.
2. Switch placeholder style to `$1` or `:name` (rewrite `?` if present).
3. Validate type mappings for DECIMAL/MONEY/UUID and large integers.
4. Review transaction defaults (ScratchBird uses implicit transactions).
