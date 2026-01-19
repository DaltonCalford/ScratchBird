# Operator Inventory (Actual Implementation)

Purpose: Code-verified inventory of expression operators (math, string, comparison, logical, pattern, JSON, array, bitwise) and their implementation status across the V2 pipeline and executor.

Status: static code review snapshot; no runtime execution performed.

## If you only remember 5 things
- V2 implements + - * / % and comparison ops, AND/OR, LIKE/ILIKE/SIMILAR, regex ~ operators, and JSON ->/->>/#>/#>>.
- The string concatenation operator `||` is parsed and type-checked but not encoded; it falls through to EXPR_ADD and fails.
- Bitwise ops (& | ^ << >> ~), array ops (@> <@ &&), cast ::, and JSON existence operators (? ?| ?&) are tokenized but not parsed in V2.
- Executor supports EXT_BIT_* and EXT_ARRAY_* operators, but they are only reachable through the PostgreSQL/MySQL parsers today.
- IS DISTINCT FROM uses EQ/NE (not null-safe), so NULL semantics are wrong when either operand is NULL.

## Scope and sources
- V2 parser/AST: `ScratchBird/src/parser/parser_v2.cpp`, `ScratchBird/include/scratchbird/parser/ast_v2.h`
- Bytecode: `ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- Executor: `ScratchBird/src/sblr/executor.cpp`
- Opcodes: `ScratchBird/include/scratchbird/sblr/opcodes.h`
- Lexer tokens: `ScratchBird/include/scratchbird/parser/lexer_v2.h`
- Emulated parsers: `ScratchBird/src/parser/postgresql/pg_parser_expr.cpp`, `ScratchBird/src/parser/mysql/mysql_parser.cpp`, `ScratchBird/src/parser/firebird/firebird_parser.cpp`

## V2 operator support (actual)

Legend: Y = implemented, P = partial or incorrect semantics, N = not implemented.

### Arithmetic and numeric
| Operator | Parser | Bytecode | Executor | Notes |
| --- | --- | --- | --- | --- |
| `+` | Y | EXPR_ADD | Y | Numeric only; no string concat |
| `-` | Y | EXPR_SUBTRACT | Y | Numeric only |
| `*` | Y | EXPR_MULTIPLY | Y | Numeric only |
| `/` | Y | EXPR_DIVIDE | Y | Integer division for ints; divide-by-zero error |
| `%` | Y | EXPR_MODULO | Y | Integer modulo |
| unary `-` | Y | EXPR_SUBTRACT (0 - x) | Y | Numeric only |
| unary `+` | N | - | - | Not parsed in V2 |
| `^` (power) | N | - | - | Use POWER()/POW() function (EXT_FUNC_POWER) |

### String and pattern
| Operator | Parser | Bytecode | Executor | Notes |
| --- | --- | --- | --- | --- |
| `||` | Y | P | N | Parsed as CONCAT, but generator falls through to EXPR_ADD (bug) |
| `LIKE` | Y | EXPR_LIKE / EXT_LIKE_ESCAPE | Y | ESCAPE uses EXT_LIKE_ESCAPE |
| `ILIKE` | Y | EXPR_ILIKE / EXT_ILIKE_ESCAPE | Y | Case-insensitive |
| `SIMILAR TO` | Y | EXT_REGEX_MATCH/NOT | Y | ESCAPE ignored; regex semantics |
| `~`, `~*`, `!~`, `!~*` | Y | EXT_REGEX_* | Y | Regex match operators |

### Comparison and boolean
| Operator | Parser | Bytecode | Executor | Notes |
| --- | --- | --- | --- | --- |
| `=`, `!=`/`<>`, `<`, `<=`, `>`, `>=` | Y | EXPR_EQ/NE/LT/LE/GT/GE | Y | String compare uses collation-aware compare |
| `IS NULL` / `IS NOT NULL` | Y | EXT_NULL_SAFE_EQ + EXPR_EQ invert | Y | Null-safe equality used |
| `IS TRUE` / `IS FALSE` | Y | EXPR_EQ vs literal | Y | NULL yields NULL via EXPR_EQ |
| `IS DISTINCT FROM` | Y | EXPR_NE | P | Null-safe semantics not implemented |
| `IS NOT DISTINCT FROM` | Y | EXPR_EQ | P | Null-safe semantics not implemented |
| `AND`, `OR` | Y | EXPR_AND/OR | Y | Boolean ops; NULL propagates |
| unary `NOT` | Y | EXPR_EQ vs 0 | P | Not 3VL-safe; uses numeric compare |

### Predicate operators
| Operator | Parser | Bytecode | Executor | Notes |
| --- | --- | --- | --- | --- |
| `IN` / `NOT IN` | Y | EXT_IN_LIST / EXT_SUBQUERY_IN/NOT_IN | Y | List and subquery supported |
| `BETWEEN` / `NOT BETWEEN` | Y | >= and <= + AND (+ NOT via EXPR_EQ) | P | NOT uses numeric compare to 0; NULL handling is fragile |

### JSON operators
| Operator | Parser | Bytecode | Executor | Notes |
| --- | --- | --- | --- | --- |
| `->` | Y | JSON_ARROW | Y | JSON extract (string-based JSON) |
| `->>` | Y | JSON_DOUBLE_ARROW | Y | JSON extract text |
| `#>` | Y | JSON_HASH_ARROW | Y | JSON path extract |
| `#>>` | Y | JSON_HASH_DOUBLE_ARROW | Y | JSON path extract text |
| `?`, `?|`, `?&` | N | - | - | Tokenized but not parsed in V2 |

### Array operators
| Operator | Parser | Bytecode | Executor | Notes |
| --- | --- | --- | --- | --- |
| ARRAY literal `ARRAY[...]` | Y | EXT_ARRAY_CONSTRUCT | Y | Stored as JSON strings in executor |
| `@>`, `<@`, `&&` | N | - | - | Tokenized but not parsed in V2 |
| array subscript `[]` | N | - | - | Tokenized but not parsed in V2 |

### Cast and assignment
| Operator | Parser | Bytecode | Executor | Notes |
| --- | --- | --- | --- | --- |
| `::` (cast) | N | - | - | Tokenized but not parsed in V2 |
| `:=` (assignment) | N | - | - | Tokenized; not used in V2 expressions |
| `=>` (named arg) | N | - | - | Tokenized; not used in V2 expressions |

## Operators supported by executor but unreachable from V2
These operators are emitted by PostgreSQL/MySQL parsers and executed by the core executor, but V2 does not parse them:
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>` via EXT_BIT_*.
- Array: `@>`, `<@`, `&&`, array subscript `[]` via EXT_ARRAY_*.
- JSON existence: `?`, `?|`, `?&` (PostgreSQL parser maps to JSON/array opcodes).
- Text search match: `@@` via EXT_TSMATCH.
- MySQL null-safe equality: `<=>` via EXT_NULL_SAFE_EQ.

## Missing/incorrect operator implementations (action list)
- `||` concatenation: parsed but not encoded; add opcode or map to an existing concat implementation.
- `IS DISTINCT FROM`: should emit EXT_NULL_SAFE_EQ (or equivalent) to be null-safe.
- unary `NOT`: current encoding is not three-valued-logic safe; should use boolean NOT opcode.
- Decide V2 support for bitwise, array, and JSON existence operators; lexer already tokenizes them.
- Decide whether to support `::` cast operator or keep CAST() only.
