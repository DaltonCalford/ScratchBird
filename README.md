# ScratchBird Database Engine

Firebird-style MGA database engine with multi-dialect wire compatibility (Firebird, MySQL, PostgreSQL) and the ScratchBird SBLR execution layer. Alpha 1 (engine/storage) and Alpha 2 (parser v2, multi-dialect) are complete; Alpha 3 (network/service mode and dependency integrity) is in progress.

## Status
- Alpha 1: complete
- Alpha 2: complete
- Alpha 3: in progress (focus: dependency life-cycle integrity, dialect parity, adapter wire conformance)
- Tests: `ctest --output-on-failure` (all passing in latest run)

## Key Docs
- Architecture rules: `MGA_RULES.md`
- Roadmap: `OFFICIAL_ROADMAP.md`
- Current work: `PROJECT_CONTEXT.md`
- Status dashboard: `docs/IMPLEMENTATION_STATUS_DASHBOARD.md`
- Planning notes: `docs/planning/` (e.g., `dependency_lifecycle_audit.md`, `alpha3_gap_todo.md`)
- Specifications: `docs/specifications/`

## Build & Test (workspace root)
```bash
cmake -S . -B build
cmake --build build
ctest --output-on-failure -C Debug --test-dir build
```
