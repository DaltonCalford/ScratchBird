# Execution Error Model and Diagnostics

Status: current_authority

Current authority:
- parser and compiler errors from `QueryCompilerV3`
- validator errors from `bytecode_validator.cpp`
- runtime-plan and explain-facing diagnostics in `query_planner.cpp`, `plan_payload.cpp`, and `executor.cpp`

## Current guarantees

- parse errors report source line and column
- emit or finalize failures are surfaced by the current compiler path
- validator failures preserve opcode-symbol or instruction-offset detail where available
- runtime-plan payloads expose current diagnostics, provenance, search summaries, and rejection or confidence fields where current code populates them

## Non-claims

- a complete universal error-code taxonomy for every future planning and execution state
- universal machine-readable diagnostics parity across all future compiler or executor subsystems
