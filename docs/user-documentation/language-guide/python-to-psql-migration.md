# Python to PSQL Migration Guide (Draft)

This guide maps common Python client-side idioms to ScratchBird PSQL equivalents. The goal is
functionality parity using Firebird/V2-style operators and functions (not Python syntax).

Status: Draft. Items marked **Planned** are specified but not yet implemented.

---

## Core Differences to Keep in Mind

- **NULL vs None**: SQL NULL is three-valued; `NULL = NULL` yields NULL, not TRUE.
- **Truthiness**: SQL predicates are boolean; Python truthy/falsy shortcuts do not apply.
- **Integer size**: Python `int` is unbounded; ScratchBird uses fixed-width integers.
- **Integer division**: Use `DIV` for explicit integer division (Planned).

---

## Arithmetic and Math

| Python | PSQL (V2) | Status/Notes |
|---|---|---|
| `a + b` | `a + b` | Implemented |
| `a - b` | `a - b` | Implemented |
| `a * b` | `a * b` | Implemented |
| `a / b` | `a / b` | Implemented (integer division for int inputs) |
| `a // b` | `a DIV b` | **Planned** (integer division operator) |
| `a % b` | `a % b` or `MOD(a,b)` | Implemented |
| `a ** b` | `POWER(a,b)` | Implemented |
| `abs(x)` | `ABS(x)` | Implemented |
| `round(x, n)` | `ROUND(x, n)` | Implemented |
| `min(a,b,...)` | `LEAST(a,b,...)` | **Planned** |
| `max(a,b,...)` | `GREATEST(a,b,...)` | **Planned** |

---

## Strings

| Python | PSQL (V2) | Status/Notes |
|---|---|---|
| `s + t` | `s || t` or `CONCAT(s,t)` | Implemented |
| `s * n` | `REPEAT(s, n)` | Implemented |
| `s[i:j]` | `SUBSTRING(s FROM i+1 FOR j-i)` | Implemented (1-based) |
| `len(s)` | `CHAR_LENGTH(s)` | Implemented |
| `s.find(x)` | `POSITION(x IN s)` or `STRPOS(s,x)` | Implemented |
| `s.replace(a,b)` | `REPLACE(s, a, b)` | **Planned** |
| `s.startswith(p)` | `s STARTING WITH p` | **Planned** (predicate) |
| `s.endswith(p)` | `ENDS_WITH(s, p)` | **Planned** (function) |
| `x in s` | `s CONTAINING x` | **Planned** (predicate, case-insensitive) |

---

## Lists, Arrays, and JSON (Dicts)

| Python | PSQL (V2) | Status/Notes |
|---|---|---|
| `[1,2,3]` | `ARRAY[1,2,3]` | Implemented |
| `lst[i]` | `arr[i]` | Implemented (1-based) |
| `lst[i:j]` | `arr[i:j]` or `ARRAY_SLICE(arr,i,j)` | **Planned** (slice opcode) |
| `len(lst)` | `ARRAY_LENGTH(arr, 1)` or `CARDINALITY(arr)` | Implemented |
| `lst.append(x)` | `ARRAY_APPEND(arr, x)` | Implemented |
| `x in lst` | `x = ANY(arr)` or `x IN (...)` | Implemented |
| `lst.index(x)` | `ARRAY_POSITION(arr, x)` | **Planned** |
| `for x in lst` | `SELECT ... FROM UNNEST(arr)` | **Planned** (native UNNEST) |
| `d['k']` | `json -> 'k'` or `JSON_EXTRACT(json,'$.k')` | Implemented |
| `'k' in d` | `JSON_HAS_KEY(json, 'k')` | **Planned** |
| `path in d` | `JSON_EXISTS(json, '$.path')` | **Planned** |

---

## Dates and Times

| Python | PSQL (V2) | Status/Notes |
|---|---|---|
| `datetime.now()` | `NOW()` | Implemented |
| `date.today()` | `CURRENT_DATE` | Implemented |
| `dt + timedelta(days=n)` | `DATE_ADD(dt, n)` | Implemented |
| `dt - dt2` | `DATE_DIFF(dt, dt2)` | Implemented |
| `dt.strftime(fmt)` | `TO_CHAR(dt, fmt)` | **Planned** |
| `datetime.strptime(s, fmt)` | `TO_TIMESTAMP(s, fmt)` | **Planned** |
| `date.fromisoformat(s)` | `TO_DATE(s, fmt)` | **Planned** |

---

## Control Flow and NULL Semantics

| Python | PSQL (V2) | Status/Notes |
|---|---|---|
| `if cond: ...` | `IF ... THEN ...` (PSQL) | Implemented |
| `x if cond else y` | `CASE WHEN cond THEN x ELSE y END` | Implemented |
| `a == None` | `a IS NULL` | Implemented |
| `a != None` | `a IS NOT NULL` | Implemented |
| `a == b` (NULL-safe) | `a IS NOT DISTINCT FROM b` | Implemented |

---

## Tips for Migration

- Prefer **predicates** (`STARTING WITH`, `CONTAINING`) over `LIKE` when possible.
- Use **JSON functions** for dict-like data and **ARRAY** for list-like data.
- Be explicit with **casts** when Python is relying on implicit coercion.

---

## Next Steps

- Once the planned items are implemented, replace “Planned” with “Implemented” and
  add examples for each category.
