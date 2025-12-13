# Implementation Status Dashboard

**Last updated:** current session  
**Tests:** all passing (`ctest --output-on-failure` from `build/`)

## Phase Summary (history + todo)
- **Alpha 1 – Engine/Core**  
  - [x] Storage engine, MGA transactions, catalog, indexes, sequences  
  - [x] Parser v1, basic DDL/DML, core tests  
- **Alpha 2 – Parser V2 & Dialects**  
  - [x] Parser v2 (context-aware), ScratchBird dialect  
  - [x] Firebird/MySQL/PostgreSQL dialect parsers  
  - [x] Semantic analyzer v2, SBLR bytecode v2  
- **Alpha 3 – Network & Service** *(current)*  
  - [x] Network stack, service mode, security (core/enterprise)  
  - [x] Wire adapters (FB/MySQL/PG/native), pooling, FDW/UDR, ODBC/JDBC  
  - [ ] Dependency integrity: validate on create/alter; block drop with dependents; refresh on alter (see `docs/planning/dependency_lifecycle_audit.md`)  
  - [ ] Dialect parity + adapter e2e suites per dialect; no cross-dialect fallbacks; Firebird→MySQL→PostgreSQL order  

## Outstanding Detail (Alpha 3 blockers)
1) Full dependency life-cycle enforcement across all object types.  
2) Dialect-specific adapter e2e coverage (Firebird, then MySQL, then PostgreSQL).  

## Links
- Roadmap: `OFFICIAL_ROADMAP.md`  
- Current context: `PROJECT_CONTEXT.md`  
- Planning: `docs/planning/` (e.g., `alpha3_gap_todo.md`, `dependency_lifecycle_audit.md`)  
- Specs: `docs/specifications/`
