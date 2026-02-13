# archive/alpha_phase_2/05-Wire-Protocol-Integration-Specification.md - Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/05-Wire-Protocol-Integration-Specification.md`

Status notes:
- The document explicitly states it is **Non-Authoritative** (draft design).

Implementation notes:
- Wire protocol adapters exist for native/PostgreSQL/MySQL/Firebird in `src/protocol/adapters/*_adapter.cpp` and `include/scratchbird/protocol/adapters/*_adapter.h`.
- Listener processes are built (`sb_listener_native/pg/mysql/fb`) and spawned by `sb_server` (see `src/server/service_controller.cpp`).
- No MSSQL/TDS adapter or listener found in this repo.

Verification:
- Partial code-level verification (adapter presence only). No protocol conformance or full message flow verification.
