# ScratchBird Master Grammar Specification (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the core parsing model, reserved words, and statement dispatch for the
ScratchBird native dialect. Emulated dialect parsers are separate front-ends
and MUST NOT reuse this grammar.

## Parser Model

- ScratchBird parser is the primary parser and supports the full ScratchBird
  dialect.
- Emulated parsers (PostgreSQL, MySQL, Firebird) are separate front-ends that
  implement only their native syntax and map to SBLR.
- The engine executes only SBLR; it never parses SQL.

## Reserved Words (Gatekeepers)

These keywords are reserved only at the start of a statement and determine the
parsing subsystem.

- Control: CREATE, ALTER, DROP, TRUNCATE, COMMENT
- DML: SELECT, INSERT, UPDATE, DELETE, MERGE, WITH
- Transaction: START, COMMIT, ROLLBACK, SAVEPOINT, RELEASE
- Session: SET, RESET, SHOW, DESCRIBE
- Flow: CALL, BEGIN, END, IF, CASE, WHILE, LOOP, RETURN
- Logic: AND, OR, NOT, NULL, TRUE, FALSE
- Security: GRANT, REVOKE

All other words are contextual and treated as identifiers unless the parser is
in a state that expects the keyword.

## Statement Dispatch (Gatekeeper)

```
if (match(KW_SELECT)) return parseSelect();
if (match(KW_INSERT)) return parseInsert();
if (match(KW_SET))    return parseSet();
if (match(KW_CREATE)) return parseCreate();
...
```

If the first token is not a gatekeeper, the parser MUST treat it as an
identifier and attempt PSQL assignment or procedure call parsing.

## Schema Navigation (SET)

The `SET` subsystem provides recursive schema navigation in ScratchBird:

```
<set_statement> ::= SET <set_target>
<set_target> ::= CURRENT SCHEMA <schema_navigation>
               | SEARCH PATH <path_list>
               | TRANSACTION <transaction_options>
               | ROLE <identifier>
               | NAMES <charset>
```

## Related Specifications

- `docs/specifications/parser/v3/catalog/SCHEMA_PATH_RESOLUTION.md`
- `docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
