# Native Parser Feature Families

## Current code-backed family inventory

| Family cluster | Current state | Primary proof | Boundary |
| --- | --- | --- | --- |
| `DDL` | `supported_parser_surface` | `parser_v3.h` | exact clause completeness still partial |
| `DML` | `supported_parser_and_lowering_surface` | `parser_v3.h`, `v3_emitter.h`, `ast_sblr_lowerer.h` | advanced compatibility forms still partial |
| `TXN` | `supported_parser_surface` | `parser_v3.h` | full alias parity still partial |
| `SESSION` | `supported_parser_surface` | `parser_v3.h` | exact set or show inventory still partial |
| `SECURITY` | `supported_parser_surface` | `parser_v3.h` | runtime authorization remains external |
| `PSQL` | `supported_parser_surface` | `parsePsqlBody()`, AST support | full control-flow parity still partial |
| `UTILITY` | `supported_parser_surface` | parser utility entry points | exact per-statement proof still partial |
| `EXTENSION` | `partial` | parser extension entry points plus selected public-beta tests | parser presence stronger than runtime proof |

## Source authority
- `include/scratchbird/parser/parser_v3.h`
- `src/parser/parser_v3.cpp`
- emulated-family mirrors in `include/src parser mysql|postgresql|firebird`

## Boundary
- Family presence is code-backed.
- Exact per-feature parity inside each family remains partially audited.
- Any finer-grained feature-key matrix still requires contradiction decomposition and proof tightening.
