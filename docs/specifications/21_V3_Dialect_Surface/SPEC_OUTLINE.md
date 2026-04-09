# Spec Outline - 21_V3_Dialect_Surface

## Purpose
Define the code-backed V3 parser surface, its lexer and AST model, the lowering boundary into SBLR, and the bounded native and emulated dialect front doors currently proven by source and tests.

## Authority split
- `supported_parser_surface` authority:
  - `parser_v3.h`
  - `lexer_v3.h`
  - `parser_state_v3.h`
  - `ast_v3.h`
  - `parser_v3.cpp`
  - `lexer_v3.cpp`
- `supported_parser_and_lowering_surface` authority:
  - `v3_emitter.h/.cpp`
  - `ast_sblr_lowerer.h`
- `supported_listener_path` authority:
  - `tests/conformance/v3_native_inet`
- `partial` authority:
  - public beta extension suites
  - JDBC promotion matrices
  - many clause-level and runtime-parity matrices

## Current code-backed subsections
1. Gatekeeper lexer model and reserved-keyword contract.
2. Statement-family capability authority across DDL, DML, transaction, session, security, utility, PSQL, and extension families.
3. AST, schema-path, naming, quoting, and system-column parser boundary.
4. SQL to SBLR v3 container emission and lowering boundary.
5. Listener-path versus direct-parse proof split.
6. Emulated parser and builtin scaffold inventory.
7. JDBC compatibility and promotion boundary.
8. Extension front-door and bounded runtime ownership split.

## Fail-closed boundary
- Section `21` does not prove full runtime parity for every named SQL form.
- Checklist files are target-state implementation worklists, not proof that the entire surface is complete.
- Specialized runtime surfaces defer to sections `17`, `18`, `19`, `20`, `22`, `24`, `26`, and `28`.

## Active contradiction set
- `CCAW-015-D01`: native parser statement-family capability authority
- `CCAW-015-D02`: gatekeeper keyword, naming, and normalization or rejection closure
- `CCAW-015-D03`: listener-path, direct-parse, and lowering authority split
- `CCAW-015-D04`: emulated parser family and builtin scaffold capability authority
- `CCAW-015-D05`: JDBC compatibility and promotion boundary
- `CCAW-015-D06`: extension front-door and bounded runtime ownership closure
