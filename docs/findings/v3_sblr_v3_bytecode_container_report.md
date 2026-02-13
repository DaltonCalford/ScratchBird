# SBLR V3 Bytecode Container Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_BYTECODE_CONTAINER.md`
Date: 2026-02-09
Status: Partially implemented

## Summary
Container encode/decode exists and matches basic layout, but validation rules are not fully enforced. `decodeContainer` does not check container_size, version_major, section alignment/ordering/overlap, or integrity verification. Flags for optional sections are not set in the emitter/compiler path. Unknown sections are ignored but not preserved on re-encode.

## Findings by Spec Item

### Encoding / Layout
- [~] Little-endian encoding, section table, and 8-byte alignment implemented in `encodeContainer`.
  - See `src/sblr/v3_container.cpp:191-260`.
- [~] Required sections (metadata/symbols/constants/bytecode) emitted.
  - `emitStatementToContainer` sets metadata and bytecode stream; symbols/constants may be empty but present. See `src/parser/v3_emitter.cpp:159-220`.
- [ ] Header flags (`has_debug/has_integrity/has_dependencies`) not set based on sections.
  - `header.flags` remains 0 in emitter; `encodeContainer` doesn’t auto-set flags.

### Decode/Validation Rules
- [ ] `version_major` must be 3 — not enforced.
  - `decodeContainer` parses version fields but does not validate them.
- [ ] `container_size` must match file length — not enforced.
- [ ] Section offsets must be aligned, ordered, non-overlapping — not enforced in `decodeContainer`.
- [~] Required sections are checked.
  - Missing required sections yields `missing required section`.
- [ ] `has_integrity` flag + INTEGRITY verification not implemented.
- [ ] Unknown sections should be preserved when rewriting — current decode drops unknown sections.

### Sections
- [~] MODULE_METADATA, SYMBOL_TABLE, CONSTANT_POOL, BYTECODE_STREAM encode/decode implemented.
- [ ] DEPENDENCIES/DEBUG/INTEGRITY validation not implemented.

## Notes
Container validation in `v3_validator.cpp` only checks section alignment (via section table offsets) and bytecode stream structure. It does not validate header version/size, flags, or integrity. Additional checks should be implemented per spec.
