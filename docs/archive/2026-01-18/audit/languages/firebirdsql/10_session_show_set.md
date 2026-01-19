# FirebirdSQL - Session, SHOW, SET

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
