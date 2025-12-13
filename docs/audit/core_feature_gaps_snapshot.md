# Core Feature Gaps & Unclear Areas (Snapshot)

Purpose: quick sweep of expected DB-engine capabilities that are missing or unclear in current codebase. Not exhaustive; prioritizes ambiguity and feature holes.

## Storage/Recovery
- [ ] WAL/redo and crash recovery path not evident; clarify durability story beyond page writes.  
- [ ] Backup/restore tooling (hot/online) unclear; only page-level primitives seen.  
- [ ] Checkpointing frequency/policy not documented.  

## Query Planning/Stats
- [ ] Statistics collection/invalidation usage in planner is unclear (stats catalog exists).  
- [ ] Cost-based choices for indexes/joins not visible; confirm optimizer maturity.  
- [ ] Adaptive plans/replan on parameter sniffing not observed.  

## Isolation/Locking
- [ ] Lock manager surface (table/index/row) and deadlock detection not documented; Firebird-style MGA assumed, but conflict resolution paths need verification.  
- [ ] Serializable/Repeatable Read semantics not specified; need visible rules for readers/writers.  

## Permissions
- [ ] RLS enforcement path not confirmed (see `security_rls_cls_gaps.md`).  
- [ ] Column masking/redaction missing.  
- [ ] Package member visibility rules need implementation (see `package_support_notes.md`).  

## DDL/Dependencies
- [ ] Full dependency enforcement across object types missing (see `dependency_lifecycle_audit.md`).  
- [ ] DROP CASCADE/RESTRICT semantics unclear; current drops may bypass dependency checks.  

## Data Types/Functions
- [ ] 128-bit numeric support pending (see `numeric_128bit_todo.md`).  
- [ ] Structured domains with computed fields, casts, EXTRACT/SET not implemented (see `domain_implementation_gaps.md`).  
- [ ] Feature coverage for VECTOR/spatial/JSONB advanced ops not audited.  

## Cursors
- [ ] First-class cursor handles/pass-through not implemented (see `cursor_first_class_todo.md`).  

## Triggers
- [ ] Runtime firing of DB/table triggers needs verification; SELECT triggers unclear (see `triggers_support_matrix.md`).  

## Replication/Clustering
- [ ] No clear replication/failover/clustering hooks; only single-node assumptions visible.  

## Logging/Monitoring
- [ ] Audit logging present for auth; query/audit logging scope unclear.  
- [ ] Metrics/telemetry integration status unclear.  

## Language/Search Path
- [ ] Search path and name resolution rules for schemas/packages/UDRs not fully specified.  
- [ ] Dialect isolation vs ScratchBird features needs enforcement (no fallback to ScratchBird parser for emulations).  

## Next Steps
- Prioritize by criticality: durability/recovery, dependency enforcement, RLS/masking, package resolution, planner/stats, type/domain completeness.  
- Add focused audits/tests per area and wire missing runtime hooks.  
