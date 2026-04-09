# SBLR Opcode Families and Symbols

Status: current_authority

## Registry ownership

The opcode registry is closed by default. New opcode symbols require an explicit registry update, verifier update, feature-matrix update, and planner and executor integration review.

## Current family model

The current registry is organized by stable family intent:

- container and metadata opcodes
- literal and constant-pool reference opcodes
- identifier and object-reference opcodes
- scalar expression opcodes
- predicate and boolean evaluation opcodes
- statement-structure opcodes for query and mutation forms
- control and diagnostics opcodes used by current v3 compilation paths

## Symbol rules

- Each opcode symbol has exactly one canonical semantic meaning.
- Aliases are forbidden inside the canonical registry.
- A parser may use dialect-local syntax, but it must normalize to the canonical symbol set before emission.
- Renderer-specific mnemonics are presentation only and do not redefine canonical semantics.

## Compatibility rules

- Reusing an opcode numeric value for a different semantic meaning is forbidden.
- Removing a shipped opcode without an explicit container-version break is forbidden.
- Broad catch-all opcodes that force engine-side reinterpretation are forbidden.
