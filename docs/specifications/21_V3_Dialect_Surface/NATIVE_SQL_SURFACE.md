# Native SQL Surface (V3)

## Current code-backed truth
- The native V3 parser exists and exposes real statement entry points for DDL, DML, utility, grant or revoke, transaction control, set or show, PSQL body parsing, and selected extension surfaces.
- The native surface lowers to SBLR or v3 container structures through `v3_emitter` and `ast_sblr_lowerer`.
- Listener-path proof exists through `tests/conformance/v3_native_inet`.

## Capability-state matrix
- `supported_parser_surface`:
  - broad statement-family parse entry points in `parser_v3.h`
  - lexer gatekeeper keyword model in `lexer_v3.h`
  - AST and schema-path representation
- `supported_parser_and_lowering_surface`:
  - select
  - insert
  - update
  - delete
  - merge
  - copy
  - core DDL create or alter or drop routing
  - transaction, grant or revoke, set or show, utility, and PSQL lowering lanes
- `supported_listener_path`:
  - native inet conformance corpus
- `partial`:
  - exact clause-level coverage for many statement families
  - exact native listener execute parity for all accepted parser forms
  - exact extension-family runtime semantics
- `fail_closed`:
  - any claim that section `21` alone proves complete native SQL runtime parity

## Proven authority anchors
- `include/scratchbird/parser/parser_v3.h`
- `src/parser/parser_v3.cpp`
- `include/scratchbird/parser/lexer_v3.h`
- `include/scratchbird/parser/v3_emitter.h`
- `include/scratchbird/sblr/ast_sblr_lowerer.h`
- `tests/conformance/v3_native_inet/README.md`
- `tests/unit/test_parser_v3_udr_compile_emitter_contract.cpp`

## Fail-closed boundary
- This file does not prove end-to-end runtime support for every named SQL form.
- Statement-family presence in parser headers or AST definitions is stronger than prose-only claims, but still weaker than audited end-to-end listener or executor proof.
- Specialized families such as connector control, cluster fabric, storage relocation, diagnostics control, blob filters, and JDBC compatibility must stay bounded to their audited parser or runtime sources.
