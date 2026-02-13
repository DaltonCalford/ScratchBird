# V3 EXTRACT/ALTER_ELEMENT Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/EXTRACT_AND_ALTER_ELEMENT.md`

## Summary
- The document labels itself non-authoritative but also claims **Status: Authoritative (V3)**; this is internally inconsistent.
- V3 parser and emitter support `EXTRACT(...)` and `ALTER_ELEMENT(...)` syntax, but **V3 executor does not handle SBLR3 opcodes** for these expressions.
- Runtime semantics (element catalog, type-specific behavior, temporal normalization, error codes) appear **implemented only in legacy EXT_EXTRACT/EXT_ALTER_ELEMENT** paths, not in V3.

## Authoritative Status Check
[~] Document is explicitly marked non-authoritative at the top but includes `Status: Authoritative (V3)` inside the file. This conflict needs resolution.

## Implementation Check

### Parsing / AST
[*] Parser supports `EXTRACT(<selector> FROM <expr>)` and `ALTER_ELEMENT(<selector> IN <expr> TO <expr>)`.
[*] Element selectors support identifiers, string literals, or integer expressions with optional argument list.

### Emission
[~] V3 emitter outputs `SBLR3_EXTRACT` / `SBLR3_ALTER_ELEMENT` with selector+args and source/new_value expressions.

### Execution
[ ] No V3 executor handling for `SBLR3_EXTRACT` or `SBLR3_ALTER_ELEMENT`.
[~] Legacy executor implements `EXT_EXTRACT` / `EXT_ALTER_ELEMENT` with extensive element semantics.

### Spec Semantics
[ ] Element catalog, type conversions, temporal normalization, and error mapping are not enforced in V3 path.

## Key References
- Parser EXTRACT/ALTER_ELEMENT: `src/parser/parser_v3.cpp:9151-9173`
- Element selector AST: `include/scratchbird/parser/ast_v3.h:3070-3110`
- V3 emitter opcodes: `src/parser/v3_emitter.cpp:3429-3453`
- V3 executor lacks handlers; legacy EXT_* exists: `src/sblr/executor.cpp:37403-37409`, `src/sblr/executor.cpp:68025-68090`
- Legacy element semantics: `include/scratchbird/sblr/extract_element_ops.h`
