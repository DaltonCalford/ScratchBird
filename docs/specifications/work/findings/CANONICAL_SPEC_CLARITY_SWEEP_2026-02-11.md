# Canonical Spec Clarity Sweep (2026-02-11)

## Scope
- Included: `docs/specifications/[00-31]_*`, `docs/specifications/README.md`, `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md`
- Excluded: `legacy_imports`, `source_copies`, `docs/specifications/library`, `docs/specifications/work`

## Method
- Placeholder scan: `TODO/TBD/FIXME/XXX`
- Ambiguity scan: open questions, soft wording (`etc`, `typically`, `as needed`)
- Consistency scan: catalog naming alignment and cross-doc statements
- Invariant scan: WAL and parser/engine boundary checks
- Link integrity scan: local markdown links

## Summary
- Canonical placeholder markers: `1` hit (legacy link label only)
- Broken local links in canonical docs: `0`
- Files with `Open Questions` sections: `81`
- Files with unresolved `Open Questions`: `78`
- Files with `Open Questions: None`: `3`

## Blocking Findings

1. Unresolved design decisions remain in core sections (`78` files), so the spec set is not yet deterministic for low-capability implementation.
2. Catalog naming mismatch is still unresolved between bootstrap and canonical catalog naming:
   - `sb_types` vs `sb_type`
   - `sb_functions` vs `sb_function`
   - Source: `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:340`
   - Source: `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:341`
   - Bootstrap usage currently appears in:
     - `docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/CATALOG_BOOTSTRAP_LAYOUT.md:58`
     - `docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/CATALOG_BOOTSTRAP_LAYOUT.md:93`

## High-Priority Clarity Gaps

1. Ambiguous option list uses `etc` in index DDL options.
   - `docs/specifications/18_Index_Framework/INDEX_DDL_AND_SEMANTICS.md:35`
2. Filespace naming still uses `typically` rather than strict rule.
   - `docs/specifications/02_Filespace_Lifecycle/SPEC_OUTLINE.md:13`
3. Transaction context mapping uses `as needed` without trigger condition.
   - `docs/specifications/08_Transaction_Core/TRANSACTION_CONTEXT_MAPPING.md:85`

## Consistency/Invariant Status

- WAL posture is consistent with guardrails (references are anti-WAL or reserved-disabled only).
- Parser/engine separation is consistently stated (engine executes SBLR only; parser owns SQL).
- Local markdown links are valid (`BROKEN_LINK_COUNT=0`).

## Open Questions By Section (Unresolved Count)

| Section | Unresolved Files |
| --- | --- |
| `24_Catalog_Model_and_Virtual_Overlays` | 12 |
| `01_Configuration_Subsystem` | 4 |
| `19_Security_Model` | 4 |
| `15_Complex_Types` | 3 |
| `00_Governance_and_Invarients` | 2 |
| `02_Filespace_Lifecycle` | 2 |
| `03_Disk_Allocator_and_Free_Space` | 2 |
| `04_Page_Size_Policy` | 2 |
| `05_Page_Taxonomy_and_Binary_Layouts` | 2 |
| `06_Fixed_Bootstrap_Page_Map` | 2 |
| `07_Catalog_Bootstrap_and_UUID_Mapping` | 2 |
| `08_Transaction_Core` | 2 |
| `09_Lock_Manager_Core` | 2 |
| `10_GC_and_Sweep` | 2 |
| `11_TOAST_and_LOB_Storage` | 2 |
| `12_Temporary_Tables` | 2 |
| `13_Operator_Model_and_Coercion` | 2 |
| `14_Base_Scalar_Types` | 2 |
| `16_Context_Variables` | 2 |
| `17_Functions_and_Procedures` | 2 |
| `20_Diagnostics_Audit_and_Observability` | 2 |
| `21_V3_Dialect_Surface` | 2 |
| `22_SBLR_Canonical_Model_and_Opcodes` | 2 |
| `23_SBLR_VM_Compiler_and_Executor` | 2 |
| `25_Runtime_Modes` | 2 |
| `26_Native_Wire_Protocol` | 2 |
| `27_Native_Handshake` | 2 |
| `28_Parser_Implementations` | 2 |
| `29_Listener_and_Server_Orchestration` | 2 |
| `30_Client_Tooling` | 2 |
| `31_Conformance_Performance_and_Reliability_Gates` | 2 |
| `18_Index_Framework` | 1 |

## Placeholder Marker Hit (Canonical)

- `docs/specifications/28_Parser_Implementations/README.md:205`
  - Contains link text/path ending in `TODO.md` under `legacy_imports`; not a canonical requirement gap.

## Ready-For-DDL Assessment

Status: `Not ready`.

Reason: unresolved decision inventory remains large and includes core physical/behavioral decisions that affect DDL/DML/PSQL/TSQL/admin semantics (configuration mutability, page sizing defaults, lock matrix details, transaction defaults, type mapping edge rules, protocol framing limits, parser coverage targets, and benchmark thresholds).

## Pass 2 Update (2026-02-11)

### Delta Summary
- Files with unresolved `Open Questions`: `55` (down from `78`)
- Files with `Open Questions: None`: `26` (up from `3`)
- Canonical placeholder markers: still `1` hit (legacy link label in parser README only)
- Broken local links: `0`

### Resolved In This Pass
1. Catalog naming mismatch resolved:
   - canonicalized to `sb_type` and `sb_procedure` in bootstrap docs.
2. `sb_index_stats` schema ownership resolved:
   - canonicalized to `18_Index_Framework/INDEX_CATALOG_AND_METADATA.md`.
3. Section `24` unresolved blocks closed:
   - decisions fixed for Firebird, PostgreSQL, MySQL, Cassandra, MongoDB, Neo4j, Redis, Milvus overlay requirements.
4. Section `01` unresolved blocks closed:
   - fixed parser/file format rules, default ports, non-root paths, mutable vs restart-required key policy, cluster propagation transport/retry semantics.
5. Section `19` unresolved blocks closed:
   - fixed Alpha auth methods, role hierarchy defaults, RLS/column policy defaults, K-of-N and mandatory cipher requirements, pre-decryption seed limits.
6. Ambiguity hotspots removed:
   - removed `etc`, `typically`, `as needed` usage from flagged canonical lines.

### Remaining Unresolved By Section
| Section | Unresolved Files |
| --- | --- |
| `15_Complex_Types` | 3 |
| `00_Governance_and_Invarients` | 2 |
| `02_Filespace_Lifecycle` | 2 |
| `03_Disk_Allocator_and_Free_Space` | 2 |
| `04_Page_Size_Policy` | 2 |
| `05_Page_Taxonomy_and_Binary_Layouts` | 2 |
| `06_Fixed_Bootstrap_Page_Map` | 2 |
| `08_Transaction_Core` | 2 |
| `09_Lock_Manager_Core` | 2 |
| `10_GC_and_Sweep` | 2 |
| `11_TOAST_and_LOB_Storage` | 2 |
| `12_Temporary_Tables` | 2 |
| `13_Operator_Model_and_Coercion` | 2 |
| `14_Base_Scalar_Types` | 2 |
| `16_Context_Variables` | 2 |
| `17_Functions_and_Procedures` | 2 |
| `20_Diagnostics_Audit_and_Observability` | 2 |
| `21_V3_Dialect_Surface` | 2 |
| `22_SBLR_Canonical_Model_and_Opcodes` | 2 |
| `23_SBLR_VM_Compiler_and_Executor` | 2 |
| `25_Runtime_Modes` | 2 |
| `26_Native_Wire_Protocol` | 2 |
| `27_Native_Handshake` | 2 |
| `28_Parser_Implementations` | 2 |
| `29_Listener_and_Server_Orchestration` | 2 |
| `30_Client_Tooling` | 2 |
| `31_Conformance_Performance_and_Reliability_Gates` | 2 |

## Pass 3 Update (2026-02-11)

### Delta Summary
- Files with unresolved `Open Questions`: `0` (down from `26`)
- Files with `Open Questions: None`: `81` (all files with open-question sections)
- Canonical placeholder markers: `1` hit (legacy link label in parser README only)
- Catalog naming mismatch (`sb_types`/`sb_functions`): fully cleared in canonical docs

### Closed In This Pass
1. Governance and core storage/transaction sections:
   - `00`, `02`-`15` now contain explicit resolved decisions and `Open Questions: None`.
2. Remaining architecture/runtime/protocol sections:
   - `16`, `17`, `20`-`23`, `25`-`31` now contain explicit resolved decisions and `Open Questions: None`.
3. README index synchronization:
   - `sync_section_readmes.sh` re-run after edits.

### Ready-For-DDL Assessment (Updated)

Status: `Ready to begin DDL/DML/PSQL/TSQL/admin language definition work`.

Residual note:
- One canonical scan hit remains for a legacy link path containing `TODO.md`:
  - `docs/specifications/28_Parser_Implementations/README.md:205`

## Pass 4 Verification (2026-02-11)

- `Open Questions` audit:
  - `OPEN_QUESTIONS_TOTAL=81`
  - `OPEN_QUESTIONS_NONE=81`
  - `OPEN_QUESTIONS_UNRESOLVED=0`
- Ambiguity keyword sweep (`etc|typically|as needed|...`) has no decision-language hits; remaining match is `/etc/scratchbird/` path text in configuration defaults.
