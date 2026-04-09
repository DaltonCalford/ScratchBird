# Native Admin Language Definition

## Current code-backed truth
- The lexer reserves `SHOW`, `SET`, `EXPLAIN`, `ANALYZE`, `CALL`, `EXECUTE`, and `PREPARE`.
- The parser has real set or show and utility entry points.
- Real runtime control-plane code exists for listener and manager orchestration in `service_controller.cpp` and `sb_manager_main.cpp`.

## Capability-state boundary
- `supported_parser_surface`:
  - set or show
  - utility-style parser entry points
- `partial`:
  - exact historical admin statement inventory
  - exact per-statement listener-control SQL proof
  - exact diagnostics-control SQL proof
- `fail_closed`:
  - broad runtime parity for every admin-family form listed historically

## Proven anchors
- `include/scratchbird/parser/parser_v3.h`
- `include/scratchbird/parser/lexer_v3.h`
- `src/server/service_controller.cpp`
- `src/server/sb_manager_main.cpp`
- `tests/conformance/public_beta/run_required_public_beta_gate.sh`

## Boundary
- Admin parser surfaces are real.
- Exact statement inventory for every historical admin command remains partial.
- Listener, security, diagnostics, connector, and cluster runtime behavior must defer to their owning sections.
