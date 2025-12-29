# Schema Path Resolution Specification

Version: 1.0
Date: 2025-12-28
Status: Active

## Purpose
Define how ScratchBird resolves schema paths and object names for SQL statements, including
absolute paths, relative paths, and current/search schema behavior. This document is the
authoritative reference for parser, executor, and adapter behavior.

## Definitions
- Schema path: dot-separated path of schema segments (example: `users.alice.dev`).
- Object name: the final identifier (example: `tablename`).
- Qualified name: `schema_path.object_name`.
- Current schema: the active schema for the session (from user/role/group defaults).
- Search path: ordered list of schema paths used for name resolution.
- Root schema: the base schema path used if no current schema is set.

## Resolution Rules
1) Default schema
   - The session always has a default schema from user/role/group.
   - If it is missing, treat the root schema as current.

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

6) UUID is authoritative
   - Internally the engine resolves and operates on object UUIDs.
   - Names are user-facing; the executor should prefer UUIDs from the resolver.

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

## Examples
Assume:
- Current schema: `users.alice`
- Search path: `users.alice`, `public`

Examples:
- `SELECT * FROM tablename`
  - Try `users.alice.tablename`, then `public.tablename`.
- `SELECT * FROM .tablename`
  - Resolve only `users.alice.tablename`; error if not found.
- `SELECT * FROM .dev.myproj.tablename`
  - Resolve `users.alice.dev.myproj.tablename`; error if not found.
- `SELECT * FROM users.alice.dev.tablename`
  - Resolve absolute path; error if not found.
