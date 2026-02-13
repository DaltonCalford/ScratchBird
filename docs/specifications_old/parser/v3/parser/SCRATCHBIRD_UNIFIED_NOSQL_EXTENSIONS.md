# ScratchBird Unified NoSQL Extensions (V3)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the ScratchBird NoSQL extension surface within the ScratchBird dialect.
These extensions are part of the ScratchBird parser only; emulated dialects do
not accept NoSQL syntax unless explicitly mapped by their own specs.

## Scope

In scope:
- JSON/Document accessors
- Graph traversal clauses
- Key-value and document collection helpers

Out of scope:
- External NoSQL protocol emulation (defined in beta requirements)

## Grammar (High-Level)

```
<nosql_extension> ::=
    <json_path_expr>
  | <json_table_expr>
  | <graph_match_clause>
```

## Execution Model

- NoSQL extensions are parsed into AST nodes and emitted as SBLR nodes
  defined in `SBLR_V3_OPCODE_SPEC.md`.
- Execution must honor the same transaction, lock, and visibility rules
  as SQL statements.

## Related Specs

- `docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`
