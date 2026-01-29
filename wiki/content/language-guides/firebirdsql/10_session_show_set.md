[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# FirebirdSQL - Session, SHOW, SET

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Spec refs:
- `ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md`
- `ScratchBird/docs/audit/22_firebird_parser_correction_plan_checklist.md`

## SHOW commands
Description: Firebird ISQL-style SHOW commands for inspecting database objects.

Status: **Implemented** - `parseShowStatement()` handles Firebird-style SHOW commands.

Supported SHOW types:
- `SHOW TABLE [name]`
- `SHOW INDEX [name]`
- `SHOW TRIGGER [name]`
- `SHOW VIEW [name]`
- `SHOW PROCEDURE [name]`
- `SHOW FUNCTION [name]`
- `SHOW DOMAIN [name]`
- `SHOW GENERATOR [name]` / `SHOW SEQUENCE [name]`
- `SHOW SCHEMA [name]`
- `SHOW ROLE [name]`
- `SHOW GRANTS [FOR name]`
- `SHOW CHECKS [name]`
- `SHOW COLLATIONS`
- `SHOW COMMENTS [name]`
- `SHOW DEPENDENCIES [name]`
- `SHOW PACKAGE [name]`
- `SHOW DATABASE` (current database info)
- `SHOW SQL DIALECT`
- `SHOW VERSION`
- `SHOW SYSTEM`

---

## SET commands
Description: Firebird SET commands for session configuration.

Status: **Implemented** - `parseSetStatement()` handles SET commands.

Supported SET types:
- `SET TRANSACTION ...` (handled as TCL)
- `SET AUTOCOMMIT {ON|OFF|1|0}`
- `SET SQL DIALECT <n>`
- `SET NAMES <charset>`
- `SET LOCAL_TIMEOUT <seconds>`
- `SET ROLE <role>`
- `SET SESSION AUTHORIZATION <user>`
- `SET TIME ZONE <value>`
- `SET SCHEMA <name>` / `SET SEARCH_PATH TO <name> [, ...]`
- `SET <var> TO/= <expr>`

Notes:
- `sb_fb_isql` may also implement client-side SET commands that do not go
  through the SQL parser.
