# Official Roadmap

## Alpha
- **Alpha 1 – Engine/Core**  
  - [x] Storage engine (heap), buffer pool, page manager, MGA transactions  
  - [x] Catalog/tables/indexes/sequences; bootstrap parser; basic DDL/DML  
- **Alpha 1 – Engine/Core (completion sequence)**  
  - [ ] 1) Catalog bootstrap schema roots (/emulated + /public fix)  
  - [ ] 2) Tablespace routing + GPID wiring (DDL/DML, multi-file)  
  - [ ] 3) Index TID updates + migration-safe rebuilds (all index types)  
  - [ ] 4) Scheduler/job system (maintenance jobs)  
  - [ ] 5) Constraint enforcement (PK/FK/UNIQUE/CHECK/NOT NULL)  
- **Alpha 2 – Parser V2 & Dialects**  
  - [x] Context-aware parser v2 (ScratchBird dialect)  
  - [x] Firebird/MySQL/PostgreSQL dialect parsers  
  - [x] Semantic analyzer v2, SBLR bytecode generator v2  
- **Alpha 3 – Network & Service** *(current)*  
  - [x] Network stack, service mode, security (core+enterprise), pooling  
  - [x] Listener/pool/parser/server process operational (per-dialect pools + socket handoff)  
  - [x] Wire adapters (FB/MySQL/PG/native) + FDW/UDR framework  
  - [ ] Driver adapters (ODBC/JDBC) readiness and integration (tracked in `docs/planning/`)  
  - [ ] Server auth wiring (HBA/SCRAM/TLS/MFA)  
  - [ ] Dependency integrity: create/alter validation, drop protection, refresh on alters  
  - [ ] Dialect parity + adapter e2e suites per dialect (no cross-dialect fallbacks)  

## Beta (next)
- TDS/MSSQL adapter (deferred from Alpha 3)  
- Performance, long-haul stability, migration tooling, clustering prep  

## References
- Status: `docs/IMPLEMENTATION_STATUS_DASHBOARD.md`  
- Current work: `PROJECT_CONTEXT.md`  
- Planning: `docs/planning/`
