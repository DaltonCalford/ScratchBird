# V3 Executor SBLR Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/EXECUTOR_V3_SBLR.md`
Date: 2026-02-09
Status: Partially implemented

## Summary
The V3 executor accepts SBLR3 containers and executes decoded instructions, but several contract items from the spec are missing or only partially enforced. Validation is performed in the server/protocol adapters, not inside the V3 executor path itself, and the spec’s SBX error codes are not used. Constant pool/symbol table handling appears unused by the executor (instructions are decoded directly from the bytecode stream), and opcode semantics/lock/GC constraints are not enforced via the spec’s semantics/constraint sources.

## Findings by Spec Item

### Input Contract
- [~] Input is a V3 bytecode container per `SBLR_V3_BYTECODE_CONTAINER.md`.
  - V3 executor detects containers (`SBL3` header) and decodes them via `decodeContainer`. See `src/sblr/executor.cpp:39817` and `src/sblr/v3_container.cpp`.
- [~] Bytecode must pass `SBLR_V3_VALIDATION_RULES.md`.
  - Validation exists in `src/sblr/v3_validator.cpp` and is invoked by `sblr::validateBytecode`, but validation is enforced by callers (`src/server/server_session.cpp:1269`, `src/protocol/adapters/protocol_adapter.cpp:632`) rather than inside `Executor::executeV3`. `executeV3` only decodes container/instructions and returns generic errors. This is only partially aligned with the spec’s executor contract.
- [ ] Constant pools and symbol tables are resolved per `SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`.
  - `decodeContainer` loads symbol/constant sections, but `executeV3` does not reference `container.symbols` or `container.constants`. Instruction decoding uses only the bytecode stream. See `src/sblr/executor.cpp:39817-39960` and absence of any `container.symbols/constants` usage.

### Execution Rules
- [~] Execute opcodes in order with a stack-based VM.
  - `executeV3` iterates decoded instructions in order and dispatches via `execStmt` (sequential execution). There is no explicit V3 stack-based VM loop; stack usage appears tied to v2 execution/expression paths, not a V3 VM. See `src/sblr/executor.cpp:52398-52516`.
- [ ] Enforce opcode semantics per `SBLR_V3_OPCODE_SEMANTICS.md`.
  - Executor does not consult `v3_semantics` metadata; it switches on opcodes with ad-hoc handling. No semantics validation is performed. See `src/sblr/v3_semantics.cpp` vs. `src/sblr/executor.cpp`.
- [ ] Enforce lock/GC/constraint rules per `EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`.
  - No lock/GC/constraint enforcement per the matrix is evident in the V3 execution path. See `src/sblr/executor.cpp` (no SBX lock/constraint handling) and `docs/findings/v3_executor_lock_gc_constraint_matrix_report.md`.

### Error Rules
- [ ] Validation failure -> `SBX-INVALID-BYTECODE`.
  - Validation failures in `validateBytecode` map to `core::Status` and are surfaced as SQLSTATE `0A000` with text (e.g., `Invalid bytecode`) in `server_session` and `protocol_adapter`. No SBX codes are emitted. See `src/sblr/bytecode_validator.cpp`, `src/server/server_session.cpp:1269-1287`, `src/protocol/adapters/protocol_adapter.cpp:631-649`.
- [ ] Unsupported opcode -> `SBX-UNSUPPORTED-OPCODE`.
  - Unsupported V3 opcodes return `ExecutionResult("V3 opcode not implemented in executor: <name>")` and are surfaced with SQLSTATE `42000`. No SBX code mapping. See `src/sblr/executor.cpp:52458-52483`.
- [ ] Runtime constraint violation -> SQLSTATE per opcode semantics.
  - V3 execution uses generic error strings and SQLSTATE `42000` for execution failures; no opcode-specific SQLSTATE mapping is present in the V3 path. See `src/server/server_session.cpp:1289-1304` and `src/protocol/adapters/protocol_adapter.cpp:651-663`.

## Notes
- This spec is authoritative; gaps should be prioritized relative to other authoritative executor-related specs.
- Any SBX-* error code mapping should be aligned across executor, server session, and protocol adapter error handling.
