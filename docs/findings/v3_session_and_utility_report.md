# SESSION_AND_UTILITY.md - Implementation Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SESSION_AND_UTILITY.md` (Authoritative, updated 2026-02-08)

Summary:
- Parser/emitter supports most SET/SHOW/RESET/EXPLAIN/ANALYZE/CONNECT/DISCONNECT/COMMENT forms, but SBLR emission does not follow the spec’s required opcodes for SET TIME ZONE and RESET.
- Spec’s “Implementation References” point to V2 parser files, not the V3 parser.
- Spec requires `string_id` identifiers in payloads, but V3 emitter encodes strings/idents directly.

Key findings:

## Statement Dispatch / Parsing
[~] V3 parser handles SET/SHOW/RESET/EXPLAIN/ANALYZE/CONNECT/DISCONNECT/COMMENT (see `src/parser/parser_v3.cpp`).
[ ] Spec’s “Implementation References” still point to `parser_v2.cpp`, not V3.

## SBLR Emission
[ ] `SET TIME ZONE` is emitted as `SBLR3_SET_VARIABLE` with key `TIME ZONE`, not `SBLR3_SET_TIME_ZONE` (`src/parser/v3_emitter.cpp:2356`).
[ ] `RESET` / `RESET ALL` / `RESET ROLE` / `RESET SESSION AUTHORIZATION` / `RESET TIME ZONE` are emitted as `SBLR3_SET_VARIABLE` with `action=3`, not the dedicated `SBLR3_RESET*` opcodes required by the spec (`src/parser/v3_emitter.cpp:2440`).
[ ] Dedicated RESET opcodes do not exist in the opcode registry (`include/scratchbird/sblr/v3_opcodes.generated.h`) even though they are referenced in `src/sblr/v3_payload_map.generated.cpp`.
[~] SHOW forms emit `SBLR3_SHOW_*` opcodes (e.g., SHOW TABLES, SHOW DATABASES) as specified, but schema mapping for many SHOW opcodes is incomplete (see `SBLR_V3_OPCODE_PAYLOADS.md` report).

## Payload Encoding Requirements
[ ] Spec requires all identifiers to use `string_id` in payloads; V3 emitter uses inline strings/idents (`Value(std::string)`), and symbol table pooling is not enforced.

## Executor Semantics
[ ] No executor enforcement for SET LOCAL vs SESSION scope or SQLSTATE errors (e.g., `25P01` for SET LOCAL outside a transaction) was found in V3 executor paths.

