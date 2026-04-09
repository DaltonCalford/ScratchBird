# Diagnostic Design Audit (2026-02-11)

## Scope
- Canonical specification tree only: `docs/specifications/[00-31]_*`.
- Excluded: `legacy_imports/`, `source_copies/`, and historical work artifacts.
- Objective:
- Find contradictions that weaken security.
- Find logical inconsistencies across sections.
- Find poor design choices that force inference by low-capability/non-reasoning implementations.

## Method
- Cross-section consistency sweep (catalog names, table names, security policy, parser/engine boundary).
- Placeholder/ambiguity sweep (`TBD/TODO/XXX`, schema placeholders, undefined enums).
- Targeted line-level validation for high-risk areas: sections `01`, `18`, `19`, `24`, `25`, `26`, `27`, `28`.

## Targeted Sweep Update (2026-02-11, Canonical-Only)
Scope:
- `docs/specifications/[00-31]_*` only.
- Excluded `legacy_imports`, `source_copies`, and non-canonical work artifacts.

Summary:
- New findings: `5`
- Critical: `1`
- High: `1`
- Medium: `3`

### Finding Matrix (Current Targeted Sweep)
| ID | Severity | Category | Title | Status |
| --- | --- | --- | --- | --- |
| TS-01 | Critical | security weakening, logical inconsistency | Cluster auth method contradiction (`authkey_shared_secret` allowed vs forbidden) | Closed |
| TS-02 | High | logical inconsistency | Listener startup dependency contradiction (independent vs server-managed/open-db-gated) | Closed |
| TS-03 | Medium | poor design choice, logical inconsistency | Security decision defaults misplaced under `Open Questions` and unverified by tests | Closed |
| TS-04 | Medium | security weakening risk, logical inconsistency | OLAP cube sharing rule is ambiguous against deny-by-default and object grants | Closed |
| TS-05 | Medium | logical inconsistency | Handshake gate test text conflicts with Alpha mTLS requirement for `group_fabric` | Closed |

### TS-01 Critical - Cluster auth method contradiction (`authkey_shared_secret`)
- Evidence:
- `docs/specifications/19_Security_Model/SPEC_OUTLINE.md:65`
- `docs/specifications/27_Native_Handshake/AUTH_NEGOTIATION_AND_POLICY.md:27`
- `docs/specifications/27_Native_Handshake/AUTH_NEGOTIATION_AND_POLICY.md:28`
- Why this is a problem:
- Section 19 allows `authkey_shared_secret` for cluster channels.
- Section 27 forbids shared secret on any cluster channel and requires mTLS.
- A low-capability implementation cannot infer the correct security rule and may implement weaker auth on cluster paths.
- Alternatives:
- A. Align section 19 to section 27: shared secret is parser-local IPC only; cluster/group fabric is mTLS-only in Alpha.
- B. Relax section 27 to allow cluster shared secret under policy controls (not recommended; weakens current security posture and conflicts with existing mTLS decisions).
- Recommended:
- A.

Closure evidence (2026-02-11 patch):
- `docs/specifications/19_Security_Model/SPEC_OUTLINE.md` now constrains `authkey_shared_secret` to parser-local IPC service channels only and excludes group/cluster channels.

### TS-02 High - Listener startup dependency contradiction
- Evidence:
- `docs/specifications/00_Governance_and_Invarients/TEST_CONTRACT.md:13`
- `docs/specifications/29_Listener_and_Server_Orchestration/SPEC_OUTLINE.md:33`
- `docs/specifications/29_Listener_and_Server_Orchestration/PROCESS_MODEL_AND_DEPENDENCY_GRAPH.md:35`
- `docs/specifications/25_Runtime_Modes/NORMATIVE_STARTUP_BOOTSTRAP_AND_INSTALL_GATES.md:56`
- Why this is a problem:
- Governance and section-29 outline still claim listener startup independence from IPC/server DB-open state.
- Runtime and process-model canonical docs require server-managed listener startup only after open-database success.
- This creates direct startup orchestration ambiguity.
- Alternatives:
- A. Update section `00` and section `29` outline invariants to match server-managed/open-db-gated model.
- B. Reopen architecture decision to permit independent listener boot (not recommended; conflicts with current auth/database-availability gate model).
- Recommended:
- A.

Closure evidence (2026-02-11 patch):
- `docs/specifications/00_Governance_and_Invarients/TEST_CONTRACT.md` now states server-managed listener startup with open-database prerequisite.
- `docs/specifications/29_Listener_and_Server_Orchestration/SPEC_OUTLINE.md` invariant 1 now matches server-managed/open-db-gated startup model.

### TS-03 Medium - Security defaults misplaced and not test-gated
- Evidence:
- `docs/specifications/19_Security_Model/DECISION_RECORD.md:23`
- `docs/specifications/19_Security_Model/DECISION_RECORD.md:24`
- `docs/specifications/19_Security_Model/DECISION_RECORD.md:25`
- `docs/specifications/19_Security_Model/TEST_CONTRACT.md:3`
- Why this is a problem:
- `DECISION_RECORD` marks `Open Questions` as `None` but includes normative policy defaults under the same heading.
- `TEST_CONTRACT` does not include explicit gates for those defaults (deny-by-default path, owner/grantee row-policy defaults, column-hide semantics, masking-order rule).
- Low-capability implementation can miss mandatory defaults because they are structurally misplaced and not test-anchored.
- Alternatives:
- A. Move policy defaults into `Decisions` (or a dedicated `Normative Defaults` section) and add explicit tests in section-19 test contract.
- B. Keep placement and rely on implicit interpretation (not recommended; too inference-heavy).
- Recommended:
- A.

Closure evidence (2026-02-11 patch):
- `docs/specifications/19_Security_Model/DECISION_RECORD.md` policy defaults moved under `Decisions` and removed from `Open Questions`.
- `docs/specifications/19_Security_Model/TEST_CONTRACT.md` now includes explicit `T19-DEF-01..04` default-policy gates.

### TS-04 Medium - OLAP cube-sharing rule is security-ambiguous
- Evidence:
- `docs/specifications/25_Runtime_Modes/OLAP_NODE_AND_CUBE_SUPPORT.md:68`
- `docs/specifications/25_Runtime_Modes/OLAP_NODE_AND_CUBE_SUPPORT.md:69`
- `docs/specifications/19_Security_Model/DECISION_RECORD.md:26`
- Why this is a problem:
- Section 25 allows cube sharing across sessions when row policies are absent.
- Section 19 sets deny-by-default when no grant path exists.
- The cube rule does not explicitly require object-level authorization checks before serving shared materialization rows, creating a potential fail-open interpretation.
- Alternatives:
- A. Make cube serving conditional on standard object/schema privilege checks regardless of row policy presence.
- B. Keep current rule but add explicit language that shared cube cache is a storage optimization only and never bypasses grant checks.
- Recommended:
- A plus B wording.

Closure evidence (2026-02-11 patch):
- `docs/specifications/25_Runtime_Modes/OLAP_NODE_AND_CUBE_SUPPORT.md` now requires normal object/schema grant checks before shared cube use and states sharing is optimization-only, not an auth bypass.

### TS-05 Medium - Handshake test text drifts from Alpha group mTLS rule
- Evidence:
- `docs/specifications/27_Native_Handshake/AUTH_NEGOTIATION_AND_POLICY.md:31`
- `docs/specifications/27_Native_Handshake/TEST_CONTRACT.md:40`
- Why this is a problem:
- Normative auth policy says `group_fabric` MUST use mTLS in Alpha.
- Test text says `group_fabric` follows configured mTLS policy and localhost fallback restrictions, which is weaker/ambiguous and can permit misread as optional.
- Alternatives:
- A. Tighten `T27-G02` to explicitly require mTLS for `group_fabric` in Alpha.
- B. Keep configurable wording but add explicit Alpha lock clause in test case body.
- Recommended:
- A.

Closure evidence (2026-02-11 patch):
- `docs/specifications/27_Native_Handshake/TEST_CONTRACT.md` `T27-G02` now requires mTLS for `group_fabric` in Alpha and deterministic reject for non-mTLS.

## Post-Sweep Update (Catalog-Name Normalization)
- Update date: `2026-02-11` (post-normalization sweep pass).
- Sweep scope:
- Canonical numbered sections only: `docs/specifications/[00-31]_*`.
- Excluded: `legacy_imports`, `source_copies`, and `work/*`.
- Sweep outcomes:
- Canonical catalog-name drift from `sb_*` table names to schema-first canonical names has been normalized in numbered sections.
- `sys.node.*` and `sys.config.*` underscore variants were normalized to dotted schema forms.
- Section 24 now includes parser capability catalog entries (`parser_profile`, `parser_capability_entry`, `parser_transform_entry`, `parser_error_map_entry`, `parser_feature_precedence`) in inventory + branch assignment.
- Section 24 now includes clock/SLO/autoscale/admission-tuning objects in inventory + branch assignment.

### Finding Status Matrix (After Sweep)
| Finding | Title | Status | Notes |
| --- | --- | --- | --- |
| 1 | Security catalog contract inconsistent | `Closed` | Section 24 now materializes PKI/crypto physical schemas (`CATALOG_TABLE_SCHEMA_SECURITY.md`), inventory entries, and branch assignments under `root.sys.security.catalog.pki`/`root.sys.security.catalog.crypto`. |
| 2 | Node lifecycle storage name conflict | `Closed` | `sys.node.*` is now consistently used across section 24 and section 25. |
| 3 | Parser capability catalog missing in section 24 | `Closed` | Canonical section-24 parser capability table schemas are now defined in `CATALOG_TABLE_SCHEMA_PARSER_CAPABILITIES.md`. |
| 4 | Clock/SLO catalogs missing in section 24 | `Closed` | Canonical section-24 runtime policy schemas are now defined in `CATALOG_TABLE_SCHEMA_CLOCK_SLO_AND_AUTOSCALE.md`. |
| 5 | Canonical singular/plural naming drift | `Closed` | Canonical core table names are normalized to singular forms (`database`, `schema`, `object`, `object_name`) across sections 06/07/24. |
| 6 | Index catalog naming inconsistency | `Closed` | `sb_index*` vs `sb_indexes` family drift is resolved in numbered canonical sections. |
| 7 | Text search naming conflict | `Closed` | `sb_tsearch_*` vs `sb_ts_*` conflict resolved to canonical `ts_*` names. |
| 8 | Domain parameter naming drift | `Closed` | Residual canonical references were normalized to `domain_parameter`. |
| 9 | Stale authoritative contradiction source | `Closed` | `EMULATED_CATALOG_GAP_CONSOLIDATION.md` rewritten as a delta ledger with no contradictory "missing" rows. |
| 10 | Config authority split (file vs catalog) | `Closed` | `01_Configuration_Subsystem/SPEC_OUTLINE.md` now declares catalog-authoritative model and bootstrap-only file role. |
| 11 | Fail-open unknown config key policy | `Closed` | Strict unknown-key policy is now explicit for security/transport/cluster namespaces (and production profile hard-fail). |
| 12 | Cluster transport/auth ambiguity | `Closed` | Section 26/27 now explicitly requires mTLS for cluster channels and forbids `authkey_shared_secret` on cluster traffic. |
| 13 | `ENUM(...)` placeholders remain | `Closed` | Placeholder `ENUM(...)` entries were replaced with explicit label sets in section 25. |

### Current Work Closure Evidence (2026-02-11)
- Finding `1` closure evidence:
- Section-24 security physical schemas now include PKI/crypto table definitions in `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_SECURITY.md`.
- Inventory and branch placement now explicitly map these tables under `root.sys.security.catalog.pki` and `root.sys.security.catalog.crypto` in `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md` and `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_OBJECT_SCHEMA_BRANCH_ASSIGNMENT.md`.
- Stale `encryption_key` placement in code/integration schema was removed; section-24 code/integration now references security schema ownership in `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_CODE_AND_INTEGRATION.md`.
- Finding `5` closure evidence:
- Canonical core table naming is now singular (`database`, `schema`, `object`, `object_name`) across bootstrap and catalog sections:
- `docs/specifications/06_Fixed_Bootstrap_Page_Map/BOOTSTRAP_PAGE_LAYOUTS.md`
- `docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/CATALOG_BOOTSTRAP_LAYOUT.md`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md`
- Finding `8` closure evidence:
- Firebird overlay mapping now uses `domain_parameter` in `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_ANALYSIS_FIREBIRD.md`.
- Finding `3` closure evidence:
- Parser capability physical schemas are now defined in `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_PARSER_CAPABILITIES.md`.
- Finding `4` closure evidence:
- Clock/SLO/autoscale/admission policy physical schemas are now defined in `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_CLOCK_SLO_AND_AUTOSCALE.md`.
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CLUSTER_OPERATIONS_CATALOG_SCHEMA.md` now references the canonical clock/SLO schema file.
- Finding `9` closure evidence:
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_GAP_CONSOLIDATION.md` was rewritten to a current delta ledger and now reports no open canonical gaps.
- Finding `10` and `11` closure evidence:
- `docs/specifications/01_Configuration_Subsystem/SPEC_OUTLINE.md` now enforces catalog-first authority and strict unknown-key handling for critical namespaces.
- Finding `12` closure evidence:
- `docs/specifications/27_Native_Handshake/AUTH_NEGOTIATION_AND_POLICY.md` now restricts `authkey_shared_secret` to parser-to-IPC localhost channels only and forbids it on cluster channels.
- `docs/specifications/27_Native_Handshake/HANDSHAKE_MESSAGE_SCHEMAS.md` and `docs/specifications/26_Native_Wire_Protocol/IPC_SBWP_FRAME_SPEC.md` now enforce cluster mTLS policy boundaries.
- Finding `13` closure evidence:
- `docs/specifications/25_Runtime_Modes/NODE_ROLE_SLO_AND_ERROR_BUDGET_POLICY.md` now uses explicit role enum labels at all previously placeholder sites.

## Findings (Ordered By Severity)
Status authority:
- The `Finding Status Matrix (After Sweep)` section above is the current-state authority.
- The `Current Work Closure Evidence (2026-02-11)` section above is the closure authority for findings marked `Closed`.
- Detailed finding bodies below are preserved as historical baseline context and may contain superseded naming examples.

### 1) Critical - Security catalog contract is internally inconsistent (`sys.*` vs canonical `sb_*`)
- Category: `security weakening`, `logical inconsistency`
- Evidence:
- `docs/specifications/19_Security_Model/KEY_STORAGE_AND_CERTIFICATES.md:7`
- `docs/specifications/19_Security_Model/KEY_STORAGE_AND_CERTIFICATES.md:18`
- `docs/specifications/19_Security_Model/ENCRYPTION_AND_KEY_MANAGEMENT.md:21`
- `docs/specifications/19_Security_Model/ENCRYPTION_AND_KEY_MANAGEMENT.md:30`
- `docs/specifications/19_Security_Model/ENCRYPTION_AND_KEY_MANAGEMENT.md:40`
- `docs/specifications/19_Security_Model/ENCRYPTION_AND_KEY_MANAGEMENT.md:49`
- `docs/specifications/19_Security_Model/PKI_LIFECYCLE_CLUSTER_CHANNELS.md:21`
- `docs/specifications/19_Security_Model/PKI_LIFECYCLE_CLUSTER_CHANNELS.md:26`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:286`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_CODE_AND_INTEGRATION.md:356`
- Why this is a problem:
- Section 19 requires `sys.cert_registry`, `sys.private_key_store`, `sys.trust_anchor`, `sys.channel_cert_binding`, `sys.cert_revocation`, `sys.pki_distribution_state`, `sys.trust_anchor_rollover`, and `sys.encryption_*`.
- Section 24 canonical inventory/schemas do not define these as physical catalog tables; only `sb_encryption_key` is defined.
- A low-capability implementation cannot determine the true on-disk table set.
- Alternatives:
- A. Canonicalize all security PKI/encryption physical tables to `sb_*` and declare `sys.*` as virtual overlays/views only.
- B. Canonicalize physical tables as `sys.*` and update all section-24 inventory/schema docs to match exactly.
- Recommended:
- Use A (aligns with current catalog naming direction and existing `sb_*` inventory conventions).

### 2) Critical - Node lifecycle storage names conflict (`sys.node_*` vs `sb_node_*`)
- Category: `security weakening`, `logical inconsistency`
- Evidence:
- Runtime lifecycle uses `sys.node_*`:
- `docs/specifications/25_Runtime_Modes/CLUSTER_OLTP_NODE_LIFECYCLE.md:40`
- `docs/specifications/25_Runtime_Modes/CLUSTER_OLTP_NODE_LIFECYCLE.md:52`
- `docs/specifications/25_Runtime_Modes/CLUSTER_OLTP_NODE_LIFECYCLE.md:136`
- `docs/specifications/25_Runtime_Modes/CLUSTER_OLTP_NODE_LIFECYCLE.md:137`
- `docs/specifications/25_Runtime_Modes/CLUSTER_SEGMENTATION_ALERTING_HEALING.md:50`
- `docs/specifications/25_Runtime_Modes/TEST_CONTRACT.md:19`
- Canonical catalog defines `sb_node_*`:
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:284`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:285`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CLUSTER_OPERATIONS_CATALOG_SCHEMA.md:466`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CLUSTER_OPERATIONS_CATALOG_SCHEMA.md:479`
- Why this is a problem:
- Node join/bootstrap/revocation lifecycle writes become ambiguous.
- Security-sensitive lifecycle evidence can be written to the wrong object model.
- Alternatives:
- A. Update runtime docs to `sb_node_bootstrap_token` and `sb_node_lifecycle_event`.
- B. Add explicit virtual view aliases `sys.node_bootstrap_token` and `sys.node_lifecycle_event` mapped to `sb_*`, then require runtime docs to identify physical vs virtual.
- Recommended:
- A now, optionally B as compatibility overlays.

### 3) Critical - Required parser capability catalog is missing from section 24
- Category: `logical inconsistency`
- Evidence:
- Required parser tables are normative:
- `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_ENTRIES_CANONICAL.md:15`
- `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_ENTRIES_CANONICAL.md:25`
- `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_ENTRIES_CANONICAL.md:39`
- `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_ENTRIES_CANONICAL.md:49`
- `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_ENTRIES_CANONICAL.md:58`
- No canonical section-24 inventory/schema entries for these tables.
- Why this is a problem:
- Parser normalization, determinism, and per-dialect gating cannot be implemented from section 24.
- Low-capability implementation has no authoritative storage contract.
- Alternatives:
- A. Add parser capability tables to `CATALOG_TABLE_INVENTORY.md` and a dedicated schema file under section 24.
- B. Move parser capability storage out of catalog into immutable build artifacts and remove section-28 table requirements.
- Recommended:
- A (current design already expects catalog-driven runtime behavior).

### 4) Critical - Clock/SLO runtime catalogs required by section 25 are not in section 24 canonical schemas
- Category: `logical inconsistency`
- Evidence:
- Clock tables required:
- `docs/specifications/25_Runtime_Modes/CLUSTER_CLOCK_DISCIPLINE_AND_SKEW_POLICY.md:20`
- `docs/specifications/25_Runtime_Modes/CLUSTER_CLOCK_DISCIPLINE_AND_SKEW_POLICY.md:34`
- `docs/specifications/25_Runtime_Modes/CLUSTER_CLOCK_DISCIPLINE_AND_SKEW_POLICY.md:45`
- `docs/specifications/25_Runtime_Modes/CLUSTER_CLOCK_DISCIPLINE_AND_SKEW_POLICY.md:59`
- SLO/autoscale tables required:
- `docs/specifications/25_Runtime_Modes/NODE_ROLE_SLO_AND_ERROR_BUDGET_POLICY.md:20`
- `docs/specifications/25_Runtime_Modes/NODE_ROLE_SLO_AND_ERROR_BUDGET_POLICY.md:38`
- `docs/specifications/25_Runtime_Modes/NODE_ROLE_SLO_AND_ERROR_BUDGET_POLICY.md:53`
- `docs/specifications/25_Runtime_Modes/NODE_ROLE_SLO_AND_ERROR_BUDGET_POLICY.md:69`
- `docs/specifications/25_Runtime_Modes/NODE_ROLE_SLO_AND_ERROR_BUDGET_POLICY.md:82`
- `docs/specifications/25_Runtime_Modes/NODE_ROLE_SLO_AND_ERROR_BUDGET_POLICY.md:98`
- `docs/specifications/25_Runtime_Modes/NODE_ROLE_SLO_AND_ERROR_BUDGET_POLICY.md:113`
- No corresponding entries in section-24 canonical inventory/schema files.
- Why this is a problem:
- Runtime policy control cannot be implemented deterministically if storage schema is undefined.
- Alternatives:
- A. Add all required clock/SLO tables to section 24 inventory + schema + branch assignment.
- B. Downgrade section-25 requirements to non-authoritative planning docs.
- Recommended:
- A.

### 5) High - Canonical object naming is inconsistent (singular/plural drift)
- Category: `logical inconsistency`, `poor design choice`
- Evidence:
- Canonical inventory uses plural roots:
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:15`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:16`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:17`
- But canonical docs also use singular variants:
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SCHEMA_TREE_CANONICAL.md:228`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SCHEMA_TREE_CANONICAL.md:229`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SCHEMA_TREE_CANONICAL.md:236`
- `docs/specifications/18_Index_Framework/INDEX_CATALOG_AND_METADATA.md:15`
- Why this is a problem:
- Two incompatible names for the same object family force guesswork and cause implementation drift.
- Alternatives:
- A. Declare one canonical naming set (`sb_databases`, `sb_schemas`, etc.) and update all docs/examples.
- B. Keep dual names but explicitly define one as compatibility view aliases.
- Recommended:
- A with optional B aliases.

### 6) High - Index catalog names are inconsistent (`sb_index*` vs `sb_indexes`/`sb_index_columns`/`sb_index_opclasses`)
- Category: `logical inconsistency`
- Evidence:
- Correct canonical names:
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:99`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:100`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:101`
- Inconsistent names still used:
- `docs/specifications/18_Index_Framework/SPEC_OUTLINE.md:78`
- `docs/specifications/18_Index_Framework/SPEC_OUTLINE.md:91`
- `docs/specifications/18_Index_Framework/SPEC_OUTLINE.md:98`
- `docs/specifications/06_Fixed_Bootstrap_Page_Map/BOOTSTRAP_PAGE_LAYOUTS.md:66`
- `docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/CATALOG_BOOTSTRAP_LAYOUT.md:80`
- `docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/SPEC_OUTLINE.md:23`
- Why this is a problem:
- Bootstrap and index implementation can diverge on table names.
- Alternatives:
- A. Normalize all to `sb_index`, `sb_index_column`, `sb_index_opclass`.
- B. Maintain alias list and enforce parser/DDL rewrite.
- Recommended:
- A.

### 7) High - Text search catalog names conflict (`sb_tsearch_*` vs canonical `sb_ts_*`)
- Category: `logical inconsistency`
- Evidence:
- Canonical section-24 names:
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:326`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:327`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:328`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:329`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:330`
- Conflicting section-18 names:
- `docs/specifications/18_Index_Framework/FULLTEXT_PG_TSCONFIG.md:106`
- `docs/specifications/18_Index_Framework/FULLTEXT_PG_TSCONFIG.md:136`
- `docs/specifications/18_Index_Framework/FULLTEXT_PG_TSCONFIG.md:156`
- `docs/specifications/18_Index_Framework/FULLTEXT_PG_TSCONFIG.md:185`
- `docs/specifications/18_Index_Framework/FULLTEXT_PG_TSCONFIG.md:186`
- `docs/specifications/18_Index_Framework/FULLTEXT_PG_STOPWORDS_DATA.md:4`
- Why this is a problem:
- Full-text dictionary/stopword storage contract is ambiguous.
- Alternatives:
- A. Move all full-text dictionary artifacts to section-24 canonical names and add any missing support tables there.
- B. Keep `sb_tsearch_*` as physical and define `sb_ts_*` as views.
- Recommended:
- A.

### 8) Medium - Domain parameter naming drift (`sb_domain_param` vs `sb_domain_parameter`)
- Category: `logical inconsistency`
- Evidence:
- Canonical:
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:43`
- Non-canonical reference:
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_ANALYSIS_FIREBIRD.md:38`
- Why this is a problem:
- Firebird catalog overlay implementation can target wrong table.
- Alternatives:
- A. Replace all `sb_domain_param` references with `sb_domain_parameter`.
- B. Define `sb_domain_param` as explicit compatibility view alias.
- Recommended:
- A with optional B alias.

### 9) Medium - Authoritative document set includes stale contradiction source
- Category: `poor design choice`, `logical inconsistency`
- Evidence:
- `EMULATED_CATALOG_GAP_CONSOLIDATION` claims it lists only missing tables:
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_GAP_CONSOLIDATION.md:4`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_GAP_CONSOLIDATION.md:9`
- But lists many tables already present in inventory:
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_GAP_CONSOLIDATION.md:14`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md:52`
- This file is currently marked authoritative:
- `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md:278`
- Why this is a problem:
- Low-capability implementation can follow contradictory authoritative instructions.
- Alternatives:
- A. Remove this file from authoritative inventory.
- B. Rewrite this file to machine-validated current state and keep authoritative.
- Recommended:
- B if kept; otherwise A immediately.

### 10) Medium - Configuration authority model is split between file-centric and catalog-centric contracts
- Category: `logical inconsistency`, `poor design choice`
- Evidence:
- Catalog-authoritative model:
- `docs/specifications/01_Configuration_Subsystem/CONFIG_CATALOG_AND_BOOTSTRAP.md:11`
- `docs/specifications/01_Configuration_Subsystem/CONFIG_CATALOG_AND_BOOTSTRAP.md:20`
- `docs/specifications/01_Configuration_Subsystem/CONFIG_CATALOG_AND_BOOTSTRAP.md:21`
- `docs/specifications/01_Configuration_Subsystem/CONFIG_CATALOG_AND_BOOTSTRAP.md:40`
- File-centric model still normative in outline:
- `docs/specifications/01_Configuration_Subsystem/SPEC_OUTLINE.md:17`
- `docs/specifications/01_Configuration_Subsystem/SPEC_OUTLINE.md:18`
- `docs/specifications/01_Configuration_Subsystem/SPEC_OUTLINE.md:19`
- `docs/specifications/01_Configuration_Subsystem/SPEC_OUTLINE.md:20`
- `docs/specifications/01_Configuration_Subsystem/SPEC_OUTLINE.md:30`
- `docs/specifications/01_Configuration_Subsystem/SPEC_OUTLINE.md:33`
- Why this is a problem:
- Startup/reload behavior may be implemented two different ways.
- Alternatives:
- A. Make `SPEC_OUTLINE` explicitly derivative of catalog-first model and mark file set as bootstrap-only artifacts.
- B. Keep dual model but define strict phase boundary and key ownership list.
- Recommended:
- A.

### 11) Medium - Fail-open handling for unknown config keys can weaken security posture
- Category: `security weakening`, `poor design choice`
- Evidence:
- `docs/specifications/01_Configuration_Subsystem/SPEC_OUTLINE.md:41`
- `docs/specifications/01_Configuration_Subsystem/SPEC_OUTLINE.md:62`
- Why this is a problem:
- Typos in security-critical keys can silently disable intended protections.
- Alternatives:
- A. Fail hard on unknown keys in `security.*`, `listener.*`, `ipc.*`, and cluster policy namespaces.
- B. Keep warnings globally but require strict mode in production profiles.
- Recommended:
- A in Alpha production profiles.

### 12) Medium - Transport/auth policy ambiguity for cluster channels
- Category: `security weakening`, `logical inconsistency`
- Evidence:
- SBWP allows insecure dev override:
- `docs/specifications/26_Native_Wire_Protocol/IPC_SBWP_FRAME_SPEC.md:81`
- Cluster runtime states control plane is always mTLS:
- `docs/specifications/25_Runtime_Modes/CLUSTER_OLTP_NODE_LIFECYCLE.md:115`
- Auth negotiation includes `authkey_shared_secret` for cluster/service:
- `docs/specifications/27_Native_Handshake/AUTH_NEGOTIATION_AND_POLICY.md:10`
- `docs/specifications/27_Native_Handshake/AUTH_NEGOTIATION_AND_POLICY.md:22`
- PKI says mTLS required in production profiles:
- `docs/specifications/19_Security_Model/PKI_LIFECYCLE_CLUSTER_CHANNELS.md:12`
- Why this is a problem:
- Without explicit environment gates, implementation may permit non-mTLS cluster auth paths unintentionally.
- Alternatives:
- A. Explicitly forbid `authkey_shared_secret` on cluster channels when `security.pki.require_mtls_cluster=true`.
- B. Allow `authkey_shared_secret` only for parser<->ipc localhost channels; never cluster membership/control channels.
- Recommended:
- A + B.

### 13) Medium - Normative schema placeholders remain (`ENUM(...)`)
- Category: `poor design choice`
- Evidence:
- `docs/specifications/25_Runtime_Modes/NODE_ROLE_SLO_AND_ERROR_BUDGET_POLICY.md:56`
- `docs/specifications/25_Runtime_Modes/NODE_ROLE_SLO_AND_ERROR_BUDGET_POLICY.md:72`
- `docs/specifications/25_Runtime_Modes/NODE_ROLE_SLO_AND_ERROR_BUDGET_POLICY.md:84`
- `docs/specifications/25_Runtime_Modes/NODE_ROLE_SLO_AND_ERROR_BUDGET_POLICY.md:100`
- `docs/specifications/25_Runtime_Modes/NODE_ROLE_SLO_AND_ERROR_BUDGET_POLICY.md:115`
- Why this is a problem:
- A low-capability implementation cannot derive exact enum values without inference.
- Alternatives:
- A. Replace every `ENUM(...)` with explicit enum sets.
- B. Reference fixed enum domain ids from section 24 with exact names.
- Recommended:
- A now, B as cross-linking.

## Summary Risk Assessment (Post-Sweep)
- Historical baseline at audit creation:
- `Critical`: 4
- `High`: 3
- `Medium`: 6
- Current active status:
- `Closed`: 13 findings (`1`-`13`)
- `Partial`: 0
- `Open`: 0
- Current active severity footprint (Open + Partial):
- `Critical`: 0
- `High`: 0
- `Medium`: 0

## Recommended Remediation Order (Post-Sweep)
1. Keep section-24 parser/clock/SLO schema docs in lockstep with section-25/28 algorithm docs.
2. Keep `EMULATED_CATALOG_GAP_CONSOLIDATION.md` delta-only; do not reintroduce already-present inventory items.
3. Re-run strict sweep before each major dialect-surface expansion gate.

## Gate Recommendation Before DDL/PSQL/TSQL/Admin Surface Expansion
- Naming and PKI placement blockers previously in Findings `1`, `2`, `5`, `6`, `7`, and `8` are closed.
- Remaining findings `3`-`13` are closed.
- DDL/PSQL/TSQL/Admin surface expansion gates may proceed under normal stage-gate evidence requirements.
