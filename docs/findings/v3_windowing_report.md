# WINDOWING.md - Implementation Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/WINDOWING.md` (Authoritative)

Summary:
- Parser supports window function parsing, `OVER (...)`, and window frame syntax.
- V3 emitter does not emit window function opcodes (`SBLR3_WIN_*`) or window spec opcodes (`SBLR3_WINDOW_SPEC`, `SBLR3_WINDOW_ORDER_BY`, `SBLR3_FRAME_*`).
- V3 executor window support appears to be wired to legacy/extended opcodes (`Opcode::WINDOW`, `Opcode::WINDOW_SPEC`) rather than V3 schema-based payloads.

Key findings:

## Parsing
[~] Parser recognizes window functions and requires `OVER` clause for window functions (`src/parser/parser_v3.cpp:9075`).
[~] Window spec parsing supports PARTITION BY, ORDER BY, and ROWS/RANGE/GROUPS frames (`src/parser/parser_v3.cpp:6680`).
[ ] Unsupported clauses (EXCLUDE, named windows) are not explicitly rejected in parser.

## SBLR Emission
[ ] Spec requires emission of `SBLR3_WIN_*` opcodes with `SCHEMA_WINDOW_CALL` and separate window spec opcodes. V3 emitter has no window emission logic (`src/parser/v3_emitter.cpp` has no WindowSpec handling).

## Execution Semantics
[ ] V3 executor appears to parse legacy window opcodes (`Opcode::WINDOW`, `Opcode::WINDOW_SPEC`) not the V3 window schema (`src/sblr/executor.cpp:24092`), so V3 window emission is not wired.

## Errors
[ ] Spec-required error codes (e.g., `42883`, `0A000`, `22003`) are not surfaced by parser/emitter or executor in V3 paths.

