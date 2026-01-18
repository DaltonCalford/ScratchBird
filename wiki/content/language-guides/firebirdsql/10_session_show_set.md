[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# FirebirdSQL - Session, SHOW, SET

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Spec refs:
- `ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md`
- `ScratchBird/docs/audit/22_firebird_parser_correction_plan_checklist.md`

## SHOW commands
Description: Firebird ISQL supports SHOW TABLE/INDEX/etc, but Firebird parser
currently does not parse these.

Status: Missing.

## SET commands
Description: Firebird SET commands (SET TRANSACTION is handled as TCL; other SET
variants like SET SQL DIALECT are not parsed in Firebird parser).

Status: Missing (except SET TRANSACTION handled as TCL).

Notes:
- `sb_fb_isql` implements a small subset of client-side SET commands, but these
  do not go through the SQL parser.
- Firebird ISQL uses `SHOW DATABASE` (current database info), but this parser
  does not implement SHOW DATABASE/SHOW SCHEMA surfaces.
