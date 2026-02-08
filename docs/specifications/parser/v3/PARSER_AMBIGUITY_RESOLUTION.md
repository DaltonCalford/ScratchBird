# Parser Ambiguity Resolution (Deterministic)
Status: Authoritative (V3)
Last Updated: 2026-02-08

Purpose: define deterministic resolution rules for ambiguous grammar cases.
These rules are authoritative for the ScratchBird parser and emulated parsers.

---

## 1) Operator Precedence (Highest → Lowest)

1. Parentheses
2. Unary: `+`, `-`, `NOT`
3. Exponentiation `^` (if enabled)
4. Multiplicative: `*`, `/`, `%`
5. Additive: `+`, `-`
6. String concat: `||`
7. Comparison: `=`, `<>`, `<`, `<=`, `>`, `>=`
8. Special comparisons: `IS [NOT] NULL`, `IS [NOT] DISTINCT FROM`
9. Pattern: `LIKE`, `ILIKE`, `SIMILAR TO`, regex operators
10. `BETWEEN`, `IN`, `EXISTS`
11. `AND`
12. `OR`

All binary operators are left‑associative unless stated.

---

## 2) Set Operator Precedence

1. `INTERSECT` / `INTERSECT ALL`
2. `UNION` / `UNION ALL`
3. `EXCEPT` / `EXCEPT ALL`

Operators at the same level are left‑associative.

---

## 3) JOIN Binding

- JOINs are evaluated left‑associatively.
- `CROSS JOIN` has the lowest precedence among JOINs.
- `NATURAL` binds to the immediately following JOIN type.

---

## 4) Statement Disambiguation

### 4.1 CREATE TABLE vs CREATE TABLE AS
- If `AS SELECT` appears after the column definition block, parse as CTAS.
- If `AS` appears before any `(`, parse as CTAS only when followed by SELECT.

### 4.2 INSERT DEFAULT VALUES vs VALUES
- If `DEFAULT VALUES` appears, emit `source=DEFAULT`.
- Otherwise, `VALUES` emits `source=VALUES`.

### 4.3 CAST vs Type Name
- `CAST(expr AS type)` always parses as CAST.
- Bare `TYPE` keywords followed by `WITH TIME ZONE` parse as a single type.

### 4.4 WITH in DML vs WITH (options)
- `WITH` before a SELECT is always a CTE.
- `WITH (...)` after CREATE INDEX/TABLE is always options.

---

## 5) ORDER BY Numeric Positions

- `ORDER BY 1` resolves to the first select item expression.
- If the index is out of bounds, emit parser error.

---

## 6) Ambiguous NULL / DEFAULT

- `DEFAULT` in INSERT values emits `SBLR3_DEFAULT_VALUE`.
- `NULL` always emits `SBLR3_LITERAL_NULL`.
