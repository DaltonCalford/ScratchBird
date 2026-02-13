# SBLR V3 Bytecode Canonicalization Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_BYTECODE_CANONICALIZATION.md`
Date: 2026-02-09
Status: Partially implemented

## Summary
Canonicalization utilities exist for symbols, constants, and certain unordered lists, but they are not integrated into validation or emission. Identifier folding/NFC normalization is not enforced in the parser/emitter. The validator does not check canonicalization rules or emit the specified V3E-0100..0105 errors.

## Findings by Spec Item

### 1) Identifier Folding
- [ ] Unquoted identifiers folded to lowercase ASCII and NFC-normalized.
  - Lexer interns identifiers as-is; emitter emits raw strings without folding. See `src/parser/lexer_v3.cpp:446-490` and `src/parser/v3_emitter.cpp:3953-3959`.
- [~] Quoted identifiers preserve exact UTF-8 bytes.
  - `scanQuotedIdentifier` preserves bytes, but no NFC normalization. See `src/parser/lexer_v3.cpp:680-719`.

### 2) Symbol Table Canonicalization
- [~] Canonicalization routine exists (`canonicalizeSymbols`).
  - `src/sblr/v3_canonicalization.cpp:83-104`.
- [ ] Not applied during emission or validation.
  - No call sites for `canonicalizeSymbols` were found.

### 3) Constant Pool Canonicalization
- [~] Canonicalization routine exists (`canonicalizeConstants`).
  - `src/sblr/v3_canonicalization.cpp:106-153`.
- [ ] Not applied during emission or validation.
  - No call sites for `canonicalizeConstants` were found.
- [ ] Decimal normalization not verified.

### 4) Unordered List Canonicalization
- [~] `canonicalizePayload` sorts OPTION_KV, privileges, and `columns` fields.
  - `src/sblr/v3_canonicalization.cpp:165-197`.
- [ ] Not applied during emission or validation.
  - No call sites for `canonicalizePayload` were found.

### 5) Whitespace and Formatting
- [~] Emitted strings are UTF-8; whitespace not preserved (by design).
  - No explicit canonicalization step for NFC or BOM removal.

### 6) Canonicalization Verifier
- [ ] `validateContainer` does not enforce canonicalization rules or V3E-0100..0105 errors.
  - `src/sblr/v3_validator.cpp` only checks structural validity and basic opcode/payload sanity.

## Notes
Canonicalization utilities exist but are unused. The enforcement requirement in `SBLR_V3_VALIDATION_RULES.md` is not met in the current validator.
