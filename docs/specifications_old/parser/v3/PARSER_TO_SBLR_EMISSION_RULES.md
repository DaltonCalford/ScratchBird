# Parser → SBLR Emission Rules (Edge Cases)

Status: Authoritative (V3)
Last Updated: 2026-02-08

Purpose: define deterministic parse-to-SBLR emission rules for **all edge
cases** across DDL, DML, and PSQL. This document is authoritative and overrides
any ambiguous or implied behavior elsewhere.

Normative language: MUST / MUST NOT / SHALL.

---

## 1) Identifier Canonicalization

- The parser MUST normalize identifiers to UTF-8.
- Identifiers are case-insensitive unless quoted.
- Unquoted identifiers MUST be folded to lowercase before emitting `ident` bytes.
- Quoted identifiers MUST preserve exact byte sequence (UTF-8).
- Any identifier length > 128 bytes MUST be rejected (`V3E-0091`).

---

## 2) CREATE TABLE Edge Cases

### 2.1 Column Ordering
- Columns MUST be emitted in source order.
- Constraints referencing columns MUST refer to canonicalized names.

### 2.2 Column Defaults
- Default expressions MUST be emitted as nested expression instructions.
- If default expression references a column in the same row, emit the reference
  as `SBLR3_COLUMN_REF` with empty table path (self-reference).

### 2.3 Identity vs. Default
- If both `DEFAULT` and `IDENTITY` are present, IDENTITY takes precedence and
  the default MUST be ignored (explicitly recorded in docs as ignored).

### 2.4 Generated Columns
- `GENERATED ALWAYS AS` MUST emit `generated_expr` and set `GENERATED_STORED`
  or `GENERATED_VIRTUAL`.
- `generated_expr` MUST NOT include any non-deterministic functions.

### 2.5 Temporary/Unlogged Flags
- Emit `TEMPORARY` and `UNLOGGED` as `flags` bits only (no extra bytes).

---

## 3) CREATE INDEX Edge Cases

### 3.1 Expression Indexes
- Expression indexes MUST emit `INDEX_KEY.kind = EXPRESSION` and embed the
  exact expression bytecode.
- Expression MUST be evaluated against the row version on insert/update.

### 3.2 Column Indexes
- Column index keys MUST emit `COLUMN_REF` with empty table path and column name.

### 3.3 INCLUDE Clause
- `include:list<ident>` MUST follow key list order and preserve source order.

### 3.4 Predicate (Partial Index)
- Predicate MUST be emitted as `predicate:opt<expr>`; any references to excluded
  columns are invalid and must be rejected.

---

## 4) ALTER TABLE Edge Cases

### 4.1 RENAME + TYPE (MySQL CHANGE COLUMN)
- Must emit **two** `SBLR3_ALTER_TABLE` statements:
  1. `RENAME_COLUMN` (action `15`)
  2. `ALTER_COLUMN_TYPE` (action `5`)

### 4.2 ALTER COLUMN USING
- Must emit action `29` with full `TYPE_SPEC` + nested `expr`.
- `USING` expression may reference old column; must be emitted before type update.

### 4.3 SET/DROP DEFAULT
- Must use actions `7`/`8` only.
- `SET DEFAULT NULL` must emit a literal NULL.

### 4.4 SET/DROP NOT NULL
- Must use actions `9`/`10` only.
- `SET NOT NULL` must verify column is not nullable in catalog before commit.

---

## 5) SELECT Edge Cases

### 5.1 DISTINCT + ALL
- DISTINCT and ALL are mutually exclusive; emit only one flag.
- If both appear, parser MUST reject (`V3E-0070`).

### 5.2 SELECT *
- Must expand `*` into explicit column refs during emission for all tables in FROM.
- Expansion order: tables in FROM order, then JOIN order.
- JOIN USING: coalesce columns into a single output entry.

### 5.3 ORDER BY References
- ORDER BY numeric positions MUST be resolved to select-item expressions at emit time.
- ORDER BY alias must resolve to the correct select item expression.

### 5.4 LIMIT/OFFSET/FETCH
- LIMIT/OFFSET/FETCH must be normalized into the canonical fields in `SCHEMA_SELECT`.
- `FETCH FIRST/NEXT` with `ONLY` must map to `FETCH_SPEC.kind`.

---

## 6) INSERT Edge Cases

### 6.1 DEFAULT VALUES
- `INSERT INTO t DEFAULT VALUES` MUST emit `source=DEFAULT` and empty `values`.

### 6.2 DEFAULT in VALUES
- Each DEFAULT token MUST emit `SBLR3_DEFAULT_VALUE`.

### 6.3 ON CONFLICT DO NOTHING/UPDATE
- `DO NOTHING` MUST emit `action=NOTHING`.
- `DO UPDATE` MUST emit `action=UPDATE` and populate `set_items` + `action_where`.

### 6.4 INSERT IGNORE (MySQL)
- MUST emit `on_conflict.action=NOTHING`.

---

## 7) UPDATE Edge Cases

### 7.1 UPDATE FROM
- `from:opt<stmt>` MUST be emitted as `SCHEMA_TABLE_REF` with `type=TABLE`.

### 7.2 Target Alias
- Alias must be emitted in `alias:opt<ident>`. No alias bytes elsewhere.

---

## 8) DELETE Edge Cases

### 8.1 DELETE USING
- USING must be emitted as `using:opt<stmt>` plus `using_joins` list.

---

## 9) MERGE Edge Cases

- Must emit `SBLR3_MERGE` only; EXT_MERGE_* opcodes are forbidden.
- `WHEN NOT MATCHED` INSERT must preserve column order of source clause.
- `WHEN MATCHED` UPDATE must emit `set_items` in source order.

---

## 10) DDL Constraints

### 10.1 CHECK
- MUST emit as `TABLE_CONSTRAINT` entry with `type=CHECK` + `check_expr`.

### 10.2 DEFERRABLE
- DEFERRABLE flags MUST be preserved in `TABLE_CONSTRAINT.deferrable`.

---

## 11) PSQL Edge Cases

### 11.1 Variable Shadowing
- Inner scope variables MUST shadow outer scope variables of the same name.
- Emitted symbol table MUST store each scope frame separately.

### 11.2 Exception Handling
- `WHEN ... DO` blocks must emit in source order and be last in block.

---

## 12) Dialect Separation

- ScratchBird parser MUST NOT accept PostgreSQL/MySQL syntax.
- Emulated parsers MUST emit only canonical ScratchBird SBLR opcodes.
