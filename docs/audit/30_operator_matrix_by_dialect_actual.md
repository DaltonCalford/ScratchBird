# Operator Matrix by Dialect (Actual)

Purpose: Per-dialect view of operator support across V2, Firebird, PostgreSQL, and MySQL parsers, including known mismatches with the executor.

Status: static code review snapshot; no runtime execution performed.

Legend:
- Y = parsed and executed correctly
- P = parsed but incorrect/partial semantics
- N = not parsed in that dialect
- U = parsed/emitted but executor missing

## Arithmetic and numeric operators
| Operator | V2 | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `+` | Y | Y | Y | Y | |
| `-` | Y | Y | Y | Y | |
| `*` | Y | Y | Y | Y | |
| `/` | Y | Y | Y | Y | |
| `%` | Y | N | Y | Y | Firebird parser does not accept `%` |
| `DIV` | N | N | N | P | MySQL parses DIV but emits modulo opcode |
| unary `+` | N | Y | Y | Y | V2 does not parse unary plus |
| unary `-` | Y | Y | Y | Y | |
| `^` (power) | N | N | N | N | Use POWER()/POW() instead |

## String and pattern operators
| Operator | V2 | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `||` (concat) | P | P | P | N | V2/Firebird emit wrong opcode; PG uses EXT_ARRAY_CAT; MySQL treats `||` as OR |
| `LIKE` | Y | Y | Y | Y | |
| `ILIKE` | Y | N | Y | N | |
| `SIMILAR TO` | Y | Y | Y | N | Uses regex opcodes |
| `CONTAINING` | N | P | N | N | Firebird parsed but not encoded by V2 generator |
| `STARTING WITH` | N | P | N | N | Firebird parsed but not encoded by V2 generator |
| `REGEXP`/`RLIKE` | N | N | N | Y | MySQL uses EXT_REGEX_MATCH |
| `~`, `~*`, `!~`, `!~*` | Y | N | N | N | V2 supports regex operators; PG parser does not |

## Comparison and NULL-safe operators
| Operator | V2 | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `=`, `<>`/`!=`, `<`, `<=`, `>`, `>=` | Y | Y | Y | Y | |
| `IS NULL` / `IS NOT NULL` | Y | Y | Y | Y | |
| `IS TRUE` / `IS FALSE` | Y | N | Y | Y | Firebird parser only handles IS NULL |
| `IS DISTINCT FROM` | P | N | Y | N | V2 emits EQ/NE; PG uses EXT_NULL_SAFE_EQ |
| `<=>` (null-safe EQ) | N | N | N | Y | MySQL only |

## Logical operators
| Operator | V2 | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `AND` / `OR` | Y | Y | Y | Y | MySQL also accepts `&&`/`||` as AND/OR |
| `NOT` | P | P | P | P | V2 uses `= 0`; PG/MySQL emit EXT_BIT_NOT |
| `XOR` | N | N | N | P | MySQL maps XOR to bitwise XOR opcode |

## JSON operators
| Operator | V2 | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `->` | Y | N | Y | N | |
| `->>` | Y | N | Y | N | |
| `#>` | Y | N | Y | N | |
| `#>>` | Y | N | Y | N | |
| `?`, `?|`, `?&` | N | N | Y | N | PG parser maps to JSON/array opcodes |

## Array operators
| Operator | V2 | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `ARRAY[...]` literal | Y | Y | Y | N | Stored as JSON strings in executor |
| `[]` subscript | N | N | U | N | PG emits EXT_ARRAY_SUBSCRIPT; executor lacks handler |
| `@>`, `<@`, `&&` | N | N | Y | N | PG emits EXT_ARRAY_*; executor supports |

## Bitwise operators
| Operator | V2 | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `&`, `|`, `^` | N | N | Y | Y | PG/MySQL emit EXT_BIT_* |
| `~` (bitwise NOT) | N | N | Y | Y | PG/MySQL emit EXT_BIT_NOT |
| `<<`, `>>` | N | N | Y | Y | PG/MySQL emit EXT_BIT_SHIFT_* |

## Cast and other operators
| Operator | V2 | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `::` (cast) | N | N | Y | N | V2 supports CAST() only |
| `IN` / `NOT IN` | Y | Y | Y | P | MySQL does not parse NOT IN explicitly |
| `BETWEEN` / `NOT BETWEEN` | Y | Y | Y | Y | V2 NOT BETWEEN uses equality inversion |
| `@@` (text search match) | N | N | Y | N | PG emits EXT_TSMATCH |
