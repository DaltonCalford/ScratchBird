Status: reconstructed_required_with_current_substrate

# PRINCIPAL ADMIN SQL AND USER MAPPING CURRENT SURFACE MODEL

## Purpose

This file defines the current parser-backed SQL surface for:

- `USER`
- `ROLE`
- `GROUP`
- `USER MAPPING`
- grant inspection surfaces that intersect principal administration

This file is intentionally strict about the difference between:

- current parser-backed authority
- required reconstructed admin surface that still belongs in the overall security rebuild

## Parser ownership rule

Per canonical parser isolation rules, this surface belongs to the local parser
implementation only. No other parser is permitted to lower or normalize this dialect path on
its behalf.

## Current parser-backed create surface

### CREATE USER

Current recovered `v3` parser support accepts:

- `CREATE USER <name>`
- optional `WITH`
- `PASSWORD '<string>'`
- `SUPERUSER`
- `NOSUPERUSER`

Recovered behavior is option-loop based and emits a dedicated `CreateUserStmt`.

### CREATE ROLE

Current recovered `v3` parser support accepts:

- `CREATE ROLE <name>`

Recovered behavior emits `CreateRoleStmt` with no further role-option grammar in the
current parser path.

### CREATE GROUP

Current recovered `v3` parser support accepts:

- `CREATE GROUP <name>`

Recovered behavior emits `CreateGroupStmt` with no further group-option grammar in the
current parser path.

## Current parser-backed alter surface

### ALTER USER

Current recovered parser support exists only for `ALTER USER`.

Recovered supported forms are:

- `ALTER USER <user> RENAME TO <new_name>`
- `ALTER USER <user> SET SCHEMA <schema_path>`
- `ALTER USER <user> [WITH] PASSWORD '<string>'`
- `ALTER USER <user> [WITH] SUPERUSER`
- `ALTER USER <user> [WITH] NOSUPERUSER`

Important current implementation detail:

- rename and set-schema emit dedicated DDL nodes
- option mutation emits `AlterSystemStmt`
- the `AlterSystemStmt.name` format is `security.user.alter.<schema_path>`
- the value payload is a semicolon-delimited string of option assignments

This mixed lowering path is current authority and must be preserved until a fuller principal
admin executor contract replaces it.

### Missing alter surfaces

No current recovered parser authority was found for:

- `ALTER ROLE`
- `ALTER GROUP`

These are therefore required reconstructed surfaces, not current parser-backed ones.

## Current parser-backed drop surface

### DROP ROLE

Recovered support accepts:

- `DROP ROLE [IF EXISTS] <role_path> [, ...] [CASCADE]`

### DROP USER

Recovered support accepts:

- `DROP USER [IF EXISTS] <user_path> [, ...] [CASCADE|RESTRICT]`

### DROP GROUP

Recovered support accepts:

- `DROP GROUP [IF EXISTS] <group_path> [, ...] [CASCADE]`

## Current parser-backed user mapping surface

Recovered parser support includes:

- `CREATE USER MAPPING FOR ... SERVER ... [OPTIONS (...)]`
- `DROP USER MAPPING [IF EXISTS] FOR ... SERVER ...`

Recovered target forms include:

- `CURRENT_USER`
- `SESSION_USER`
- `PUBLIC`
- `USER <name>`
- implicit bare `<name>`

Recovered behavior emits dedicated user-mapping AST nodes rather than reusing the generic
principal-admin path.

## Current grant and inspection intersections

Recovered parser support includes:

- object `GRANT`
- object `REVOKE`
- `SHOW ROLE [name]`
- `SHOW ROLES [name]`
- `SHOW GRANTS [FOR name]`

No current recovered parser authority was found in this pass for:

- `SHOW USERS`
- `SHOW GROUPS`
- `GRANT ROLE ... TO ...`
- `REVOKE ROLE ... FROM ...`

The catalog layer already exposes role and group membership APIs, so the absence here is
current parser drift, not product intent.

## Required reconstructed admin surface

The rebuilt security specification requires the parser family to grow toward a complete,
deterministic principal-admin SQL surface that includes:

- create, alter, drop, and inspect user
- create, alter, drop, and inspect role
- create, alter, drop, and inspect group
- role membership grant and revoke
- group membership add and remove
- external group mapping administration
- shared-rights inspection surfaces

That is required reconstructed behavior. It is not yet claimed here as current parser
authority unless explicitly recovered from code.

## Fail-closed boundary

The parser must not:

- silently reinterpret missing role or group admin grammar through another parser
- borrow another dialect parser's lowering path
- fabricate role-membership syntax that current parser code does not support

If a principal-admin SQL form is not in the current parser surface, it must remain either:

- a rejected statement
- a separate rebuilt target-state item captured in canon and planning

It must not exist as an undocumented implicit surface.
