# Schema Path Security Defaults

**Status:** Draft (Alpha)
**Last Updated:** 2026-01-20
**Scope:** Default security behavior for schema search path resolution

---

## 1) Purpose

Define the default security rules and syntax for schema path resolution so
clients get consistent behavior and emulated parsers cannot escape their
assigned namespace.

---

## 2) Scope

**In scope:**
- Default schema path keywords
- Relative path behavior (leading dot)
- Emulation isolation rules
- Security gating knobs and defaults

**Out of scope:**
- Role-based path rewriting policies
- External directory mappings

---

## 3) Default Path Keywords

ScratchBird provides the following built-in keywords:

- `current` - current schema
- `home` - user home schema (defaults to current)
- `public` - shared public schema (`/users/public`)
- `shared` - shared internal schema for tooling

**Resolution order:** keywords are expanded into concrete schema names before
lookup. If a keyword resolves to a non-existent schema, it is skipped.

---

## 4) Relative Paths

Relative paths start with a leading dot (`.`) and are resolved against the
current schema only.

Examples (current schema = `users.alice`):
- `.widgets` -> `users.alice.widgets`
- `.dev.widgets` -> `users.alice.dev.widgets`

**Default behavior:**
- Relative paths are **not** expanded against every search-path entry.
- Relative paths only use the current schema as the base.

---

## 5) Emulation Isolation

Emulated sessions (PostgreSQL/MySQL/Firebird) are pinned to their emulated
root schema path. The resolver must reject any path that would escape the
emulated root.

Rules:
- All resolved schema names must be descendants of the emulated root.
- Internal tokens such as `!:` are never returned to clients.
- If an unqualified name cannot resolve within the emulated root, return
  the dialect-appropriate "not found" error.

---

## 6) Security Defaults (Alpha)

Configuration defaults (names are descriptive; exact config file placement is
implementation-specific):

- `schema_path.enable_keywords = true`
- `schema_path.allow_relative = true` (current-schema only)
- `schema_path.expand_relative = false` (no expansion across search path)
- `schema_path.allow_root_escape = false`
- `schema_path.emulation_isolation = strict`

If `allow_root_escape=false`, any path containing `..` or absolute root tokens
is rejected.

---

## 7) Error Handling

- Unauthorized path expansions return `ERR_SCHEMA_PATH_DENIED`.
- Emulation escape attempts return dialect-specific "permission denied" errors.
- Invalid tokens return `ERR_SCHEMA_PATH_SYNTAX`.

---

## 8) Related Specifications

- `docs/specifications/catalog/SCHEMA_PATH_RESOLUTION.md`
- `docs/specifications/Security Design Specification/01_SECURITY_ARCHITECTURE.md`
- `docs/specifications/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
