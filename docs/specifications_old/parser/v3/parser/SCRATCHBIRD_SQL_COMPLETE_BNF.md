# ScratchBird SQL Complete BNF/EBNF Grammar (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Provide the authoritative grammar for the ScratchBird native SQL dialect. This
grammar is the canonical source for parser implementation. Emulated dialect
parsers (PostgreSQL/MySQL/Firebird) are defined separately and MUST NOT be
derived from this grammar.

## Notation

```ebnf
::=          Definition
[ ]          optional (0 or 1)
{ }          Repetition (0 or more)
( )          Grouping
|            Alternative
< >          Non-terminal
UPPERCASE    Terminal (keyword)
'literal'    Terminal (literal)
```

## Top-Level

```ebnf
<scratchbird_sql> ::=
    { <statement> [ ';' ] }

<statement> ::=
    <ddl_statement>
  | <dml_statement>
  | <dcl_statement>
  | <tcl_statement>
  | <psql_statement>
  | <utility_statement>
  | <execute_block>
```

## DDL

```ebnf
<ddl_statement> ::=
    <create_statement>
  | <alter_statement>
  | <drop_statement>
  | <truncate_statement>
  | <comment_statement>

<create_statement> ::=
    <create_database>
  | <create_schema>
  | <create_table>
  | <create_index>
  | <create_view>
  | <create_sequence>
  | <create_domain>
  | <create_type>
  | <create_role>
  | <create_user>
  | <create_tablespace>

<create_table> ::=
    CREATE [ <table_scope> ] TABLE [ IF NOT EXISTS ] <table_name>
    '(' <table_element> { ',' <table_element> } ')'
    [ <table_options> ]
    [ AS <select_statement> [ WITH [ NO ] DATA ] ]

<table_element> ::=
    <column_definition>
  | <table_constraint>
  | LIKE <table_name> [ <like_options> ]
```

## DML

```ebnf
<dml_statement> ::=
    <select_statement>
  | <insert_statement>
  | <update_statement>
  | <delete_statement>
  | <merge_statement>
  | <copy_statement>
```

## DCL

```ebnf
<dcl_statement> ::=
    <grant_statement>
  | <revoke_statement>
```

## TCL

```ebnf
<tcl_statement> ::=
    BEGIN
  | START TRANSACTION
  | COMMIT
  | ROLLBACK
  | SAVEPOINT <identifier>
  | RELEASE SAVEPOINT <identifier>
```

## PSQL

```ebnf
<psql_statement> ::=
    <create_procedure>
  | <create_function>
  | <create_trigger>
  | <execute_block>
```

## Utility

```ebnf
<utility_statement> ::=
    SET <set_target>
  | RESET <set_target>
  | SHOW <show_target>
  | DESCRIBE <describe_target>
  | EXPLAIN <explain_target>
```

## References

Detailed productions are defined in the following authoritative documents:
- `docs/specifications/parser/v3/parser/ScratchBird SQL Language Specification - Master Document.md`
- `docs/specifications/parser/v3/parser/ScratchBird Master Grammar Specification v2.0.md`
- `docs/specifications/parser/v3/parser/05_PSQL_PROCEDURAL_LANGUAGE.md`
