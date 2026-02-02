# V2 vs Python Operator and Type Gap Analysis (PSQL Migration)
Status: Superseded (implementation verified)
Last Updated: 2026-02-02

Note: All gaps called out here are closed. Track any remaining work in
`docs/planning/TRACKER_OUTSTANDING_MASTER.md`.


## Scope and Sources
- V2 operator grammar: `ScratchBird/docs/specifications/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`.
- SBLR opcode registry (operator/function surface): `ScratchBird/include/scratchbird/sblr/opcodes.h`.
- Core type system: `ScratchBird/include/scratchbird/core/types.h`.
- Type system overview: `ScratchBird/docs/specifications/types/README.md`.
- Python baseline assumed: Python 3.x built-in operators and core types.

This review compares ScratchBird V2 operator/type surface to Python to identify gaps that keep
business logic in Python instead of moving into PSQL.

## Operator Comparison (V2 vs Python)

### Arithmetic and Numeric
- V2: `+ - * / % ^ **` in grammar; SBLR has ADD/SUB/MUL/DIV/MOD plus POWER function opcodes.
- Python: `+ - * / // % **` (unary `+`/`-`), plus `@` for matrix multiply.
- Gaps and notes:
  - Floor division (`//`) has no explicit operator; current path is `FLOOR(a / b)` if exposed.
  - Matrix multiply (`@`) has no SQL analog.
  - Python `int` is unbounded; V2 uses fixed widths (INT8..INT128/UINT128) and DECIMAL.

### Comparison and Membership
- V2: `= <> != < > <= >=`, `IS NULL`, `IS DISTINCT FROM`, `BETWEEN`, `IN`, `EXISTS`,
  `LIKE/ILIKE`, `REGEXP`, quantified comparisons (`ANY/ALL/SOME`).
- Python: `== != < > <= >=`, `in/not in`, `is/is not`, chained comparisons (`a < b < c`).
- Gaps and notes:
  - Python identity (`is`) has no SQL equivalent. `IS NULL` and `IS NOT DISTINCT FROM`
    cover the most common `None` checks, but semantics still differ.
  - Python `in` works on lists/tuples/strings/dicts/sets; SQL `IN` is list or subquery and
    uses three-valued logic with NULLs.
  - Chained comparisons are not a SQL operator; they must be expanded (`a < b AND b < c`).

### Boolean Logic and Truthiness
- V2: `AND`, `OR`, `NOT` with three-valued logic (TRUE/FALSE/UNKNOWN).
- Python: `and/or/not` short-circuit and return the last evaluated operand, not just bool.
- Gap: Python truthiness (empty list, zero, None) does not map cleanly to SQL NULL behavior.
  Migration usually needs explicit `IS NULL` or `COALESCE` patterns.

### String Operations
- V2 functions/operators: `||`/`CONCAT`, `SUBSTRING`, `LENGTH`, `CHAR_LENGTH`,
  `OCTET_LENGTH`, `TRIM`, `UPPER/LOWER`, `POSITION/STRPOS`, `LPAD/RPAD`, `REPEAT`,
  regex match/replace/split, `SPLIT_PART`, `STRING_TO_ARRAY`.
- Python: concatenation with `+`, repetition with `*`, indexing/slicing, `startswith`,
  `endswith`, `split`, `replace`, `format`, `join`.
- Gaps and notes:
  - No direct string indexing/slicing operators; must use `SUBSTRING` with 1-based indexing.
  - No opcode found for `REPLACE`, `STARTSWITH`, `ENDSWITH`, or formatting (`format`,
    `to_char`, `strftime`-style).
  - Python `+` for strings is not SQL; V2 uses `||` or `CONCAT`.

### Bitwise
- V2: `& | # ~ << >>` in grammar, plus BIT_* functions; logical right shift exists in SBLR.
- Python: `& | ^ ~ << >>`.
- Gap: Python uses `^` for XOR, but V2 uses `#` for XOR and `^` for exponent in grammar.
  This is a common migration footgun.

### Collections and JSON
- V2: ARRAY types with `ARRAY_*`, `UNNEST`, array overlap/contain operators; JSON/JSONB
  extraction and update opcodes exist (`->`, `->>`, `#>`, `jsonb_set`, etc).
- Python: list/tuple/dict/set with rich methods, slicing, and comprehensions.
- Gaps and notes:
  - Array slicing and list-like methods (append/pop/extend/sort) are not SQL operators.
  - Dict-style access and mutation patterns need JSON path support and key-existence operators
    beyond the basic JSON ops listed.
  - Set semantics are not native; can be emulated with arrays plus DISTINCT.

### Date/Time
- V2: DATE/TIME/TIMESTAMP/INTERVAL, DATE_ADD/SUB/DIFF, NOW/CURRENT_DATE, AT TIME ZONE,
  EXTRACT, AGE.
- Python: datetime/date/time/timedelta, timezone-aware arithmetic, strftime/strptime parsing.
- Gaps and notes:
  - No opcode found for formatting/parsing helpers (`to_char`, `to_timestamp`, `strftime`).
  - Timezone handling exists, but Python's tzinfo and library ecosystem is much richer.

## Data Type Comparison (V2 vs Python)

### Numeric
- V2: fixed-width ints (INT8..INT128/UINT128), FLOAT32/64, DECIMAL, MONEY.
- Python: unbounded `int`, `float` (64-bit), `decimal.Decimal`, `fractions.Fraction`.
- Gaps: unbounded integers and fractions do not map directly. DECIMAL can cover many cases
  but needs consistent precision/scale and overflow rules for migration.

### Text and Binary
- V2: CHAR/VARCHAR/TEXT with charset/collation, BINARY/VARBINARY/BYTEA/BLOB.
- Python: `str` (Unicode), `bytes`, `bytearray`, `memoryview`.
- Gaps: Python indexing/slicing semantics; mutable `bytearray` and `memoryview` have no
  direct SQL analog. Collation differences can change comparison results.

### Boolean/Null
- V2: BOOLEAN plus NULL (three-valued logic).
- Python: `bool` and `None` with truthiness rules.
- Gap: `None == None` is True in Python; `NULL = NULL` yields NULL in SQL.

### Temporal
- V2: DATE/TIME/TIMESTAMP (+ optional timezone flag), INTERVAL.
- Python: datetime/date/time/timedelta (timezone-aware).
- Gap: Python timezone libraries and formatting/parsing helpers are richer than current spec.

### Collections and Complex Types
- V2: ARRAY, COMPOSITE, JSON/JSONB, RANGE, spatial types, INET/CIDR/MAC.
- Python: list/tuple/dict/set/range, complex numbers.
- Gaps: no native set/frozenset, no complex numbers, no lazy range type.
  Arrays and JSON can cover many use cases, but lack Python methods and slicing sugar.

## Migration-Blocking Gaps (Candidate Additions)

### P0 (highest impact on moving Python logic into PSQL)
- Array slicing and indexing semantics plus `ARRAY_LENGTH`/`ARRAY_POSITION` coverage in parser
  and executor (opcodes exist, need full wiring).
- JSON key-existence and path query operators to match dict access patterns.
- NULL-safe equality and explicit three-valued helpers in PSQL (`IS [NOT] DISTINCT FROM`,
  `COALESCE`, `NULLIF`) with consistent engine behavior.
- Looping over arrays and query results (FOREACH/UNNEST patterns) with clear syntax in PSQL.

### P1 (common Python idioms)
- Floor division (`//`) or `DIV` operator for integer division parity.
- String helpers: `REPLACE`, `STARTS_WITH`, `ENDS_WITH`, and formatting/parsing helpers
  (`TO_CHAR`, `TO_DATE`, `TO_TIMESTAMP` equivalents).
- Scalar `LEAST/GREATEST` to mirror Python `min/max` outside aggregates.

### P2 (quality-of-life parity)
- Set-like helpers for arrays/JSON (union/intersection/difference) to reduce client-side set logic.
- Range and slice helper functions for windowing and paging scripts.
- Optional helper functions that mimic Python built-ins (`len`, `round`, `abs`, `pow`)
  where not already covered by opcodes and parser wiring.

## Implementation Audit (Code Truth)

Status legend: **Implemented** = executor handling present; **Partial** = placeholder or limited
semantics; **Missing** = no opcode/executor handling found.

### Implemented (executor wiring present)
- Core arithmetic/comparison/logical ops (`EXPR_*`, `LIKE/ILIKE`): `ScratchBird/src/sblr/executor.cpp:31419`.
- NULL-safe equality + NOT/IS NULL helpers: `ScratchBird/src/sblr/executor.cpp:24760`.
- COALESCE/NULLIF/CASE: `ScratchBird/src/sblr/executor.cpp:24464`.
- IN list, IN/NOT IN subquery, EXISTS: `ScratchBird/src/sblr/executor.cpp:25878` and
  `ScratchBird/src/sblr/executor.cpp:28050`.
- Arrays: append/prepend/cat/remove/replace, overlap/contains/contained_by, subscript,
  length/dims/upper/lower: `ScratchBird/src/sblr/executor.cpp:25275`.
- Regex match/replace/split: `ScratchBird/src/sblr/executor.cpp:27985`.
- String helpers (SPLIT_PART, STRPOS, POSITION, OVERLAY, QUOTE_*): `ScratchBird/src/sblr/executor.cpp:28345`.
- Math helpers (ABS/ROUND/FLOOR/TRUNC/POWER/etc.): `ScratchBird/src/sblr/executor.cpp:29507`.
- Temporal helpers (DATE_ADD/SUB/DIFF, NOW, AT TIME ZONE, CURRENT_DATE): `ScratchBird/src/sblr/executor.cpp:23680`.
- JSON/JSONB extraction and mutation (`->`, `->>`, `#>`, JSON_SET/INSERT/REMOVE): 
  `ScratchBird/src/sblr/executor.cpp:24005`.

### Partial (placeholder or limited semantics)
- Table-returning text helpers return arrays for now:
  - REGEXP_SPLIT_TO_TABLE: `ScratchBird/src/sblr/executor.cpp:28228`.
  - STRING_TO_TABLE: `ScratchBird/src/sblr/executor.cpp:28280`.
  - UNNEST_TEXT: `ScratchBird/src/sblr/executor.cpp:28310`.
- Array dimensional support is 1D only (`ARRAY_LENGTH` ignores dimension arg):
  `ScratchBird/src/sblr/executor.cpp:25667`.
- JSON path array `#>` parsing is simplified (comma-split, not full PG array semantics):
  `ScratchBird/src/sblr/executor.cpp:24092`.

### Missing (no opcode/executor handling located)
- Array slicing and `ARRAY_POSITION` (no opcode in `ScratchBird/include/scratchbird/sblr/opcodes.h`).
- JSON key-existence operators and path predicates (no opcode in
  `ScratchBird/include/scratchbird/sblr/opcodes.h`).
- Floor-division operator (`DIV` or `//`) and scalar `LEAST/GREATEST`
  (no opcode in `ScratchBird/include/scratchbird/sblr/opcodes.h`).
- String helpers for `REPLACE`, `STARTS_WITH`, `ENDS_WITH`, and formatting/parsing
  (`TO_CHAR`, `TO_DATE`, `TO_TIMESTAMP`) (no opcode in
  `ScratchBird/include/scratchbird/sblr/opcodes.h`).
- Set-style helpers for arrays/JSON (union/intersection/difference) beyond overlap/contain
  (no opcode in `ScratchBird/include/scratchbird/sblr/opcodes.h`).
- Native `UNNEST` opcode handling is not in the executor (no `Opcode::UNNEST` case in
  `ScratchBird/src/sblr/executor.cpp`).

## Notes for Follow-up
- The opcode registry is rich, but parser wiring may lag. The next step is to verify parser
  coverage for the implemented opcodes and schedule the missing items above.
- Python portability issues are more about semantics (NULL and truthiness) than raw operators.
  Documenting these differences in the PSQL guide will reduce migration surprises.
