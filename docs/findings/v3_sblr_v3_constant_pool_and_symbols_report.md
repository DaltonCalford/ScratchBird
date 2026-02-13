# SBLR V3 Constant Pool and Symbol Table Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`
Date: 2026-02-09
Status: Partially implemented

## Summary
Container support for symbol table and constant pool exists, but V3 emission currently inlines strings in payloads rather than using `string_id` references. Constant pooling is not used in V3 emitter, and canonicalization utilities are unused. Required pooling rules, forbidden pooling rules, and deterministic ordering are not enforced.

## Findings by Spec Item

### Symbol Table Requirements
- [ ] Required pooling of identifiers, names, labels, etc. not implemented.
  - V3 emitter emits strings inline via `toIdent` and literal encoding; no symbol table construction. See `src/parser/v3_emitter.cpp:3953-3965`.
- [ ] Symbol ordering/canonicalization not enforced.
  - `canonicalizeSymbols` exists but unused. See `src/sblr/v3_canonicalization.cpp:83-104`.
- [ ] Identifier NFC normalization not enforced; unquoted folding not enforced.
  - Lexer interns identifiers as-is. See `src/parser/lexer_v3.cpp:446-490`.

### Constant Pool Requirements
- [ ] Required constant pooling not implemented.
  - V3 emitter inlines literal payloads; container constants are empty. See `src/parser/v3_emitter.cpp` emission paths.
- [ ] Constant ordering/canonicalization not enforced.
  - `canonicalizeConstants` exists but unused. See `src/sblr/v3_canonicalization.cpp:106-153`.
- [ ] UUID catalog ID pooling not enforced.
  - No evidence of UUID constants being pooled and referenced by `const_id`.

### Deterministic Hashing
- [ ] No module hash construction enforcing build_id/source_hash zeroing was found.

## Notes
The current V3 container path uses the symbol table and constant pool sections structurally, but the parser/emitter does not populate them. This violates the spec’s pooling and determinism requirements.
