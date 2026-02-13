# PSQL Procedural Language (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the PSQL procedural language for ScratchBird. PSQL is compiled to SBLR
bytecode and executed by the SBLR executor. The engine MUST NOT parse SQL.

## Core Principles

- Strong typing with full V3 type system and domains.
- Deterministic control-flow semantics.
- No implicit SQL parsing in the engine.

## Program Structure

```
[ DECLARE
    <variable_decl>;
    ...
]
BEGIN
    <statement_list>
[ EXCEPTION
    WHEN <condition> THEN <statement_list>
]
END
```

## Variable Declarations

```
<variable_decl> ::= <ident> [CONSTANT] <type_spec> [DEFAULT <expr>]
```

Rules:
- Variables MUST be declared before use.
- DEFAULT expressions are evaluated at block entry.

## Assignment

```
SET <ident> = <expr>
SELECT <expr_list> INTO <ident_list> FROM <query>
```

## Control Flow

### IF / ELSE

```
IF <condition> THEN <statement_list>
[ELSIF <condition> THEN <statement_list>]*
[ELSE <statement_list>]
END IF
```

### CASE (Statement)

```
CASE <expr>
    WHEN <expr> THEN <statement_list>
    [WHEN <expr> THEN <statement_list>]*
    [ELSE <statement_list>]
END CASE

CASE
    WHEN <condition> THEN <statement_list>
    [WHEN <condition> THEN <statement_list>]*
    [ELSE <statement_list>]
END CASE
```

### LOOP / WHILE / FOR

```
LOOP
    <statement_list>
    EXIT WHEN <condition>
END LOOP

WHILE <condition> LOOP
    <statement_list>
END LOOP

FOR <ident> IN <int_range> LOOP
    <statement_list>
END LOOP

FOR <record_var> IN <select_statement> LOOP
    <statement_list>
END LOOP
```

## Cursors

- DECLARE cursor over a query or set-valued source.
- OPEN / FETCH / CLOSE follow the rules in `PSQL_CURSOR_HANDLES.md`.
- `WHERE CURRENT OF` is valid in UPDATE/DELETE.

## Error Handling

```
EXCEPTION
    WHEN <condition> THEN <statement_list>
```

- RAISE and RAISE NOTICE map to SBLR exception opcodes.
- Exception propagation follows `PSQL_RUNTIME_V3.md`.

## Related Specs

- `docs/specifications/parser/v3/PSQL_RUNTIME_V3.md`
- `docs/specifications/parser/v3/PSQL_STATEMENTS.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`
