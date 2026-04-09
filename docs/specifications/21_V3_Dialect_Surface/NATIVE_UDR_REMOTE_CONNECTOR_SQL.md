# Native UDR Remote Connector SQL

## Current code-backed truth
- UDR compile and emit proof exists in `test_parser_v3_udr_compile_emitter_contract.cpp`.
- Remote connector and UDR runtime boundaries are section-owned by section `17`.
- Parser-side connector SQL can therefore be treated as partially backed front-door inventory, not as proof of unified remote execution parity.

## Boundary
- Keep unified remote execution guarantees fail-closed here.
- Treat connector SQL as `partial` unless a later contradiction pass ties exact parser entry points to current runtime code and tests.
- package `03` uses this file as parser-front-door and UDR-readiness inventory
  only; end-to-end remote execution closure is intentionally deferred
