[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# PostgreSQL - Security (DCL)

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Spec refs:
- `ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`

## CREATE ROLE
Description: Creates a role.

Syntax (actual, abbreviated):
```sql
CREATE ROLE <name> [WITH <options>]
```
Example:
```sql
CREATE ROLE app_read;
```
Status: Implemented (basic role creation).
Spec delta: Option handling is limited.

## CREATE USER
Description: PostgreSQL CREATE USER is parsed but bytecode payload does not
match executor expectations.

Status: Stubbed.

## GRANT / REVOKE
Description: Grants and revokes privileges.

Syntax (actual, abbreviated):
```sql
GRANT <priv_list> ON <object> TO <grantee>
REVOKE <priv_list> ON <object> FROM <grantee>
```
Example:
```sql
GRANT SELECT ON TABLE users TO app_read;
```
Status: Stubbed (executor expects EXT_GRANT_PRIVILEGE/EXT_REVOKE_PRIVILEGE).
