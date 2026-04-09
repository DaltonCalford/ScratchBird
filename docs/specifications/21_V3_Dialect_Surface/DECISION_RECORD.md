# Decision Record - 21_V3_Dialect_Surface

## Scope
- parser and lexer front doors
- AST and schema-path construction
- SQL to SBLR lowering and emission boundary
- native versus emulated parser bridge surfaces
- listener-path parser conformance boundary

## Current decisions
- Section `21` is parser-layer authority first, not a blanket runtime-parity declaration.
- The V3 parser stack is real and implementation-backed.
- The gatekeeper lexer model is real and should remain the canonical keyword authority for the V3 surface.
- SQL text is parsed outside the engine core and lowered to SBLR or v3 containers before execution.
- Emulated dialect families are real parser families, but their full semantic parity must remain bounded to code-backed proof.
- Native listener-path conformance is stronger authority than prose-only surface claims.
- Normative checklist files remain implementation-planning worklists and must not be treated as proof that every listed feature is complete.

## Explicit rejections
- Reject treating section `21` as proof that every listed native DDL, DML, admin, infrastructure, listener, connector, or JDBC form executes end to end today.
- Reject using broad section `21` prose to overrule section-owned runtime boundaries from sections `17` to `20` and `22` to `28`.

## Primary code authority
- `include/scratchbird/parser/parser_v3.h`
- `include/scratchbird/parser/lexer_v3.h`
- `include/scratchbird/parser/v3_emitter.h`
- `include/scratchbird/sblr/ast_sblr_lowerer.h`
- `src/parser/parser_v3.cpp`
- `src/parser/lexer_v3.cpp`
- `src/parser/v3_emitter.cpp`
