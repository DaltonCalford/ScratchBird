# V3 Parser: Joins and Table References

Status: Authoritative (V3)

Purpose: define parsing, SBLR emission, and executor semantics for JOINs and
FROM clause table references.

---

## 1) Grammar (Authoritative)

```
from_clause := FROM table_ref (join_clause | ',' table_ref)*

join_clause := [NATURAL] join_type JOIN table_ref join_condition
join_type := INNER | LEFT | RIGHT | FULL | CROSS
join_condition := ON expr | USING '(' ident_list ')'

table_ref :=
    schema_path [AS] alias [ '(' ident_list ')' ]
  | LATERAL '(' select_stmt ')'
  | '(' select_stmt ')' [AS] alias
  | schema_path '(' arg_list ')' [AS] alias   // table function
```

Notes:
- Comma joins are normalized to `CROSS JOIN`.
- `NATURAL` is allowed with `INNER/LEFT/RIGHT/FULL` and standalone `NATURAL JOIN`.
- `LATERAL` applies only to subquery table refs.

---

## 2) AST Schema (Logical Fields)

### 2.1 TableRef

- `kind`: `TABLE | SUBQUERY | LATERAL_SUBQUERY | FUNCTION`
- `path`: schema path (for TABLE/FUNCTION)
- `alias`: optional alias
- `alias_columns`: optional list of column aliases
- `subquery`: optional SELECT AST (for SUBQUERY/LATERAL_SUBQUERY)
- `func_args`: optional list of expressions (for FUNCTION)

### 2.2 JoinNode

- `left`: TableRef or JoinNode
- `right`: TableRef
- `join_type`: `INNER | LEFT | RIGHT | FULL | CROSS`
- `is_natural`: bool
- `using_columns`: optional list of identifiers
- `on_expr`: optional expression

---

## 3) SBLR Emission (Normative)

### 3.1 Core Opcodes

- `SBLR3_TABLE_REF`
- `SBLR3_JOIN_TYPE`
- `SBLR3_JOIN_CONDITION`
- `SBLR3_JOIN_USING`
- Join algorithm nodes:
  - `SBLR3_HASH_JOIN`
  - `SBLR3_NESTED_LOOP_JOIN`

### 3.2 Emission Rules

1. Each table reference emits `SBLR3_TABLE_REF` with:
   - `kind`, `schema_path`, `alias`, `alias_columns`, and `subquery` if present.
2. Each join emits:
   - `SBLR3_JOIN_TYPE` with join type + `is_natural`.
   - If `USING`, emit `SBLR3_JOIN_USING` with sorted column list.
   - If `ON`, emit `SBLR3_JOIN_CONDITION` with the expression.
3. Physical algorithm selection:
   - Parser emits logical join nodes only.
   - Executor/optimizer selects `SBLR3_HASH_JOIN` or `SBLR3_NESTED_LOOP_JOIN`.

---

## 4) Executor Semantics (Normative)

### 4.1 Join Type Semantics

- **INNER**: output rows where predicate matches.
- **LEFT**: unmatched left rows output with NULL-extended right.
- **RIGHT**: unmatched right rows output with NULL-extended left.
- **FULL**: unmatched rows on both sides output with NULL extension.
- **CROSS**: cartesian product.

### 4.2 NATURAL / USING

- `NATURAL` expands to equality on all common column names.
- `USING (c1, c2)` expands to equality predicates for listed columns.
- Output columns for USING/NATURAL are coalesced to a single column per name.

### 4.3 LATERAL

- LATERAL subquery is evaluated per left-side row.
- LATERAL references are allowed to access columns from the left input.

### 4.4 Locking

- Joins inherit locking from SELECT/UPDATE context.
- If `FOR UPDATE/SHARE`, row locks are acquired in primary key order.

---

## 5) Error Codes / SQLSTATE

- `42601` syntax_error
- `42703` undefined_column (USING/NATURAL column missing)
- `42883` undefined_function (table function not found)
- `42P01` undefined_table

---

## 6) Legacy Parsing References (Non‑Authoritative)

- `src/parser/parser_v2.cpp:6036`
- `src/parser/parser_v2.cpp:6071`
- `src/parser/parser_v2.cpp:6160`
- `src/parser/parser_v2.cpp:6188`
