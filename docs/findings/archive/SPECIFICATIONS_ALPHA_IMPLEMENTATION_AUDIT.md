# Alpha Specification Implementation Audit (Preliminary)
Status: Superseded (implementation verified)
Last Updated: 2026-02-02

Note: All gaps called out here are closed. Track any remaining work in
`docs/planning/TRACKER_OUTSTANDING_MASTER.md`.


Date: 2026-01-20  
Scope: `ScratchBird/docs/specifications` (alpha scope only). ScratchBird source is read-only;
this is a structure-and-evidence audit, not full behavioral validation.

## Method

- Inventory all specification documents under `ScratchBird/docs/specifications`.
- Classify alpha vs out-of-scope (beta/cluster/archive/future/reference).
- For alpha-scope areas, check for **implementation evidence** by source tree
  presence and known gap docs. No deep behavioral tests were executed.

## Status Legend

- **Implemented (evidence)**: corresponding source modules exist; behavior not fully verified.
- **Partial**: some code exists or explicit gap docs list missing features.
- **Outstanding**: no code evidence or explicitly unimplemented.
- **Out of scope**: beta/cluster/archive/future/reference (not part of alpha compliance).
- **Needs update**: spec conflicts with current alpha scope or architecture decisions.

## Inventory Summary

- Total spec documents: **430**
- Out-of-scope documents (beta/cluster/archive/future): **146**
- Alpha-scope candidates (remaining): **284**

## Alpha-Scope Category Matrix (Evidence-Based)

| Category | Spec Count | Implementation Evidence | Status | Notes |
| --- | --- | --- | --- | --- |
| **core/** | 9 | `src/core/` | Partial | Core engine exists; full parity not validated. |
| **catalog/** | 5 | `src/catalog/` | Partial | Catalog code exists; F-022/F-023 still in progress. |
| **storage/** | 10 | core storage likely in `src/core/` | Partial | No dedicated `src/storage/`; needs deep audit. |
| **transaction/** | 7 | `src/core/` | Partial | MGA code present; GC/sweep audit pending. |
| **indexes/** | 13 | `src/index/` | Partial | LSM/advanced index specs likely future; verify. |
| **sblr/** | 10 | `src/sblr/` | Partial | Execution performance plans exist; needs code parity audit. |
| **parser/** | 11 | `src/parser/` | Partial | Gap docs exist (MySQL/PG); verify native parity. |
| **ddl/** | 22 | parser/executor | Partial | Spec-heavy; implementation verification pending. |
| **dml/** | 8 | parser/executor | Partial | Spec-heavy; implementation verification pending. |
| **triggers/** | 2 | parser/executor | Partial | Needs trigger runtime audit. |
| **types/** | 9 | core/storage | Partial | Data type persistence audit pending. |
| **network/** | 9 | `src/network/`, `src/protocol/` | Partial | Listener not ready; port 3092 spec ok. |
| **wire_protocols/** | 6 | `src/protocol/` | Partial | TDS is post-gold; native/PG/MySQL/FB need parity check. |
| **security/** (Security Design Specification) | 46 | `src/security/` | Partial | Security layers exist; audit required. |
| **admin/** | 3 | `src/cli/` | Partial | sb_admin surface not fully verified. |
| **tools/** | 3 | `src/cli/` | Partial | Tooling specs need parity audit. |
| **operations/** | 5 | `src/server/` | Partial | Monitoring views pending instrumentation. |
| **deployment/** | 2 | `src/server/` | Partial | Service/config docs need runtime validation. |
| **drivers/** | 13 | `src/odbc/`, `src/client/` | Partial | Only some clients exist; many are planned. |
| **remote_database_udr/** | 10 | `src/fdw/` | Partial | Adapters exist; full coverage unknown. |
| **udr/** | 2 | none obvious | Outstanding | No obvious runtime module found. |
| **udr_connectors/** | 7 | `src/fdw/` (partial) | Partial | Firebird/MySQL/PG adapters exist; confirm parity. |
| **api/** | 3 | `src/client/` | Partial | Needs API parity audit. |
| **query/** | 2 | `src/optimizer/` | Partial | Optimizer docs vs code need verification. |
| **scheduler/** | 3 | `src/core/job_scheduler.cpp` | Partial | Scheduler implemented; verify spec parity. |
| **compression/** | 2 | none obvious | Outstanding | Compression specs appear unimplemented. |
| **TEMPORARY_TABLES_SPECIFICATION.md** | 1 | unknown | Partial | Spec exists; code parity not verified. |
| **MEMORY_MANAGEMENT.md** | 1 | core | Partial | Needs allocator/buffer audit. |
| **PERFORMANCE_BENCHMARKS.md** | 1 | testing | Partial | Benchmark guidance; not implementation. |
| **FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md** | 1 | parser/runtime | Partial | Parity audit ongoing. |
| **V2_PARSER_FIREBIRD_ALIGNMENT_SPECIFICATION.md** | 1 | parser | Partial | Needs code comparison. |
| **V2_PARSER_INDEX_TYPE_COMPLETENESS.md** | 1 | parser/index | Partial | Needs code comparison. |
| **MYSQL_PARSER_IMPLEMENTATION_GAPS.md** | 1 | gap list | Outstanding | By definition missing features. |
| **POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md** | 1 | gap list | Outstanding | By definition missing features. |
| **PARSER_REMAPPING_AND_IMPLEMENTATION_STRATEGY.md** | 1 | plan | Needs update | Validate vs current parser architecture. |
| **Alpha Phase 2/** | 19 | mixed | Needs update | Contains alpha/beta mix; needs pruning. |
| **reference/** | 30 | reference | Out of scope | Reference material only (non-normative). |
| **BACKUP_AND_RESTORE.md** | 1 | beta | Out of scope | Backup is beta per scope definition. |

## Alpha-Scope Outstanding or High-Risk Areas

- **Scheduler/job runner**: implemented; verify spec parity and cluster-forward compatibility.
- **Git config normalization**: spec updated to canonical `repo_*` keys; parser is legacy-only.
- **Parser gaps**: `MYSQL_PARSER_IMPLEMENTATION_GAPS.md` and
  `POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md` list missing items.
- **Compression**: specs exist without code evidence.
- **UDR core**: no clear UDR runtime module found.
- **Network listener**: specs exist; implementation readiness is still pending.

## Specs Needing Update or Scope Clarification

- **Y-Valve vs Listener**: `specifications/core/Y_VALVE_ARCHITECTURE.md` and
  `specifications/network/Y_VALVE_DESIGN_PRINCIPLES.md` should be reconciled with
  the network listener pool design (listener replaces Y-Valve).
- **Alpha Phase 2**: multiple docs include post-gold MSSQL/TDS references; these
  should be explicitly marked out-of-scope or moved to beta/forward-looking areas.
- **Backup/restore**: `BACKUP_AND_RESTORE.md` is in alpha tree but backup is beta
  scope; consider relocating or adding a stronger scope disclaimer.

## Specs No Longer Applicable (Candidates)

These appear to be out of scope for alpha or deprecated by newer architecture
decisions; confirm before removing:

- `specifications/network/Y_VALVE_DESIGN_PRINCIPLES.md` (superseded by listener)
- `specifications/core/Y_VALVE_ARCHITECTURE.md` (superseded by listener)
- `specifications/Alpha Phase 2/11d-MSSQL-Client-Implementation.md` (post-gold)
- `specifications/wire_protocols/tds_wire_protocol.md` (post-gold)

## Next Steps (Deep Audit)

1. For each alpha category, map spec sections to concrete code paths and confirm
   implementation status (file/line evidence).
2. Promote confirmed gaps into `docs/findings/CONSOLIDATED_FINDINGS.md`.
3. Remove or move beta/post-gold specs out of the alpha tree to avoid scope drift.
