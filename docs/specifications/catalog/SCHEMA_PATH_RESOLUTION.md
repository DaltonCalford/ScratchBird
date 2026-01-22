# Schema Path Resolution Specification

Version: 1.1
Date: 2025-12-31
Status: Active

## Purpose
Define how ScratchBird resolves schema paths and object names for SQL statements, including
absolute paths, relative paths, and current/search schema behavior. This document is the
authoritative reference for parser, executor, and adapter behavior.

## Definitions
- Schema path: dot-separated path of schema segments (example: `users.alice.dev`).
- Object name: the final identifier (example: `tablename`).
- Qualified name: `schema_path.object_name`.
- Current schema: the active schema for the session (derived from role/user/group defaults).
- Search path: ordered list of schema paths used for name resolution.
- No-search prefix: `!:` prefix that disables search-path fallback for a name.

## Resolution Rules
1) Default schema
   - The session current schema is derived in this order:
     1) active role default schema (if set)
     2) user default schema (if set)
     3) group default schema (if set; deterministic tie-breaker by lexicographic group name)
     4) `users.public` schema
   - If the current schema cannot be resolved, resolution is an error (no silent fallback).

2) Unqualified object name (no schema path)
   - Resolve in the current schema.
   - If not found, resolve in each schema in search_path (in order).
   - If still not found, return NOT FOUND.

3) Leading dot (relative to current schema, no search_path fallback)
   - `.tablename` resolves to `<current_schema>.tablename`.
   - If not found, return NOT FOUND (no search_path fallback).

4) Leading dot with sub-schema (relative path)
   - `.dev.myproj.tablename` resolves to `<current_schema>.dev.myproj.tablename`.
   - If not found, return NOT FOUND.

5) Absolute path (no leading dot)
   - `users.alice.dev.tablename` resolves exactly as given.
   - If not found, return NOT FOUND.

6) No-search prefix (`!:`)
   - `!:` disables search-path fallback for the name.
   - All other resolution rules remain the same (CURRENT/PARENT/ABSOLUTE still apply).
   - If `!:` is used with an unqualified name and the current schema is missing, return NOT FOUND.

7) UUID is authoritative
   - Internally the engine resolves and operates on object UUIDs.
   - Names are user-facing; the executor should prefer UUIDs from the resolver.

8) DDL resolution (CREATE/ALTER/DROP)
   - DDL statements never use search_path fallback.
   - Unqualified names resolve only in the current schema.
   - Relative and absolute paths are honored as specified above.
   - If the current schema is missing, return NOT FOUND.

9) Object type awareness
   - Resolution is type-aware: the resolver is called with an expected object type.
   - A name that resolves to a different object type is an error (e.g., table vs trigger).

10) Identifier matching
   - Unquoted identifiers are case-insensitive.
   - Quoted identifiers are case-sensitive and must match exactly.
   - Case-insensitive collisions are not permitted within a single schema.

## Path Tokens
ScratchBird supports path tokens for internal resolution:
- PARENT
- CURRENT
- ABSOLUTE

These tokens are valid in the ScratchBird parser.
- Emulated parsers may use them internally for rewrites.
- Remote clients must not be exposed to these tokens.

## Emulated Engines
- Emulated sessions are scoped to an emulated database root schema path.
- Emulated parsers and adapters must ensure object resolution stays within the
  emulated root and that results presented to the client match the expected
  emulated structure.
- The emulating parser may use internal path tokens or `!:` during translation.
  Remote clients must not be exposed to these tokens.

## Schema Resolution
Schema objects are resolved using the same rules as other SQL objects.
For example, `SET SCHEMA name` or `SHOW SCHEMA name` uses the same path logic
as `SELECT * FROM name`.

## Search Path Semantics
- The search path is an ordered list of schema paths; order is authoritative.
- If the current schema is not included in the search path, it is not searched
  unless the user explicitly references it (e.g., `.name` or `!:`).
- Some deployments may define special path keywords (e.g., `current`, `home`,
  `public`, `shared`). Exact syntax and security gating are defined by the
  security subsystem.
- Optional security-gated behavior: relative paths (leading dot) may be
  resolved against each search-path entry as a base. Defaults and security
  gating are defined in `docs/specifications/catalog/SCHEMA_PATH_SECURITY_DEFAULTS.md`.

## Examples
Assume:
- Current schema: `users.alice`
- Search path: `current`, `home`, `public`, `shared`
- `current` resolves to `users.alice`
- `home` resolves to `users.alice`
- `public` resolves to `users.public`
- `shared` resolves to `shared`

Examples:
- `SELECT * FROM tablename`
  - Try `users.alice.tablename`, then `users.public.tablename`, then `shared.tablename`.
- `SELECT * FROM .tablename`
  - Resolve only `users.alice.tablename`; error if not found.
- `SELECT * FROM .dev.myproj.tablename`
  - Resolve `users.alice.dev.myproj.tablename`; error if not found.
- `SELECT * FROM users.alice.dev.tablename`
  - Resolve absolute path; error if not found.
- `SELECT * FROM .dev.myproj.tablename` (if relative search across search_path is enabled)
  - Try `users.alice.dev.myproj.tablename`, then `users.public.dev.myproj.tablename`,
    then `shared.dev.myproj.tablename`.
- `SELECT * FROM !:tablename`
  - Resolve only `users.alice.tablename`; do not search `users.public` or `shared`.
- `SELECT * FROM !:.dev.myproj.tablename`
  - Resolve `users.alice.dev.myproj.tablename`; `!:` is redundant but allowed.
- `SELECT * FROM !:users.alice.dev.tablename`
  - Resolve absolute path; `!:` prevents any search-path fallback (none applies).
- `CREATE TABLE tablename`
  - DDL: resolve only `users.alice.tablename`; no search_path fallback.
- `DROP TABLE tablename`
  - DDL: resolve only `users.alice.tablename`; no search_path fallback.
- `ALTER TABLE .dev.myproj.tablename RENAME TO newname`
  - DDL: resolve `users.alice.dev.myproj.tablename` only.
- `SELECT * FROM ..reports.tablename`
  - Resolve relative to parent schema of `users.alice`; error if no parent.
