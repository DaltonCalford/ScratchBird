# Research Dossier - Product Completion Program

Date: 2026-02-24
Status: baseline research complete
Program: Product completion workpack (`PCG-001..PCG-072`)

## 1) Objective

Define the missing product-level capabilities that are normally expected before broad production adoption of a database platform, then map each gap to executable tickets, gate evidence, and staffing lanes.

This dossier supports:
- `PRODUCT_COMPLETION_SPECIFICATION_2026-02-24.md`
- `PRODUCT_COMPLETION_WORKPLAN_2026-02-24.md`
- `ORDERED_TASK_TICKETS.csv`
- `GATE_EVIDENCE_MATRIX.csv`

## 2) Method

1. Use current ScratchBird planning artifacts as truth for what is implemented, in-progress, and planned.
2. Use local source clones of major engines as capability baseline references.
3. Compare ScratchBird current release plan against common production expectations.
4. Convert each capability gap into ticketed closure with objective evidence requirements.

## 3) Current-State Baseline (ScratchBird)

### 3.1 Confirmed in-progress or planned programs

- Remote UDF connector program exists with completed, in-progress, and pending slices in the master tracker.
- Parser emulation contract currently scopes this cycle to Firebird, MySQL, and PostgreSQL only.
- Live migration pass-through + cutover program exists but is still draft planning.
- Hybrid SBLR/native program exists with explicit-compile-first strategy and optional JIT.
- Cluster/native alignment program exists with phase structure and gating.

### 3.2 Implication

Core engine and major expansion tracks are active, but several product-completion concerns remain outside those tracks or are not yet closed with release-grade evidence.

## 4) Production Capability Baseline (Cross-Engine Local Clone Study)

Local clone evidence shows that mature engines generally ship all of the following classes together:

| Capability class | Common implementation signals observed across local clones |
| --- | --- |
| Backup, restore, DR rehearsal | Native backup utilities, restore tooling, and recurring verification workflows |
| PITR / retention governance | Log retention controls and point-in-time restoration patterns |
| Upgrade / downgrade safety | Official upgrade tooling, format compatibility contracts, rollback guidance |
| Transport + at-rest security | TLS/mTLS controls, key lifecycle management, and at-rest encryption support |
| Immutable auditing | Security/event auditing channels and traceability controls |
| Access governance | Strong role/policy controls and least-privilege defaults |
| Migration continuity | CDC/replication controls plus cutover safety workflows |
| Reliability engineering | Failure injection/failpoint support, soak and endurance validation |
| SRE operations | Health/readiness, monitoring surfaces, support diagnostics |
| Supply-chain controls | SBOM/provenance/signature workflows in release pipelines |
| Compliance packaging | Legal notices, lifecycle policy, and audit-ready release bundles |

Engines reviewed via local clones for this baseline include PostgreSQL, MySQL, Firebird, Cassandra, OpenSearch, ClickHouse, DuckDB, Redis, and MongoDB.

## 5) Gap Analysis and Ticket Mapping

| Gap ID | Gap Summary | Current-State Evidence | Risk if Unaddressed | Ticket Closure |
| --- | --- | --- | --- | --- |
| GAP-01 | No release-program-level backup/restore contract with DR drill gating | Existing plans focus on migration/native/cluster and do not close backup/restore as a release gate | Recovery uncertainty during incident | `PCG-010..PCG-014` |
| GAP-02 | Upgrade/rollback compatibility is not yet closed as a dedicated product gate | Upgrade safety requirements are distributed, not consolidated into one gate program | Irreversible version drift | `PCG-020..PCG-024` |
| GAP-03 | Security controls need end-to-end gate closure (TLS, key lifecycle, audit immutability, masking, legal hold) | Security features exist in components, but release-level completion package is not consolidated | Data exposure or weak forensic posture | `PCG-030..PCG-036` |
| GAP-04 | Multi-engine conformance and certification closure for connector+parser matrix is not yet represented as one executable harness | UDF and parser tracks are separate and at different completion points | Behavior drift by engine/version | `PCG-040..PCG-042` |
| GAP-05 | Continuous CDC divergence detection and deterministic reconciliation after cutover needs explicit ownership | Live migration plan is draft and broad; post-cutover sync governance is not yet a dedicated closure lane | Silent divergence after migration | `PCG-043..PCG-045` |
| GAP-06 | SRE package is incomplete as a strict launch gate (SLO/error-budget, support bundle, runbooks) | Observability planning exists but full operations handoff contract is not yet consolidated | Slow incident response and unclear ops ownership | `PCG-050..PCG-055` |
| GAP-07 | Reliability proof (chaos + soak) is not yet tied to product release gate | Existing plans include testing but not unified product-completion reliability closure | Unknown long-run failure modes | `PCG-053..PCG-055`, `PCG-070` |
| GAP-08 | Supply-chain integrity controls (SBOM/signing/provenance/reproducibility/CVE SLA) need strict gateing | Partial references exist in other workstreams; no dedicated release-integrity closure lane | Enterprise adoption blocker | `PCG-060..PCG-063` |
| GAP-09 | Compliance/legal packaging and lifecycle policy are not yet formalized as launch-blocking artifacts | LTS/deprecation/compliance artifacts are distributed | Legal or contractual release risk | `PCG-023`, `PCG-064`, `PCG-072` |
| GAP-10 | End-to-end integrated gameday and final launch handoff package not yet centralized | Signoff exists in separate tracks, not as one integrated go/no-go contract | Hidden cross-track failures at launch | `PCG-070..PCG-072` |

## 6) ScratchBird Strengths Relevant to Closure

1. Strong invariant posture already documented: SBLR canonical and MGA-first semantics.
2. Existing gate/evidence culture in other worktrees can be reused directly.
3. Program already has active expansion tracks (UDF, parser, migration, native, cluster) to feed inputs.
4. Existing ownership model by role (not individual) is compatible with multi-sprint staffing.

## 7) Weaknesses / Risks

1. Current capabilities are spread across many tracks; product-completion visibility can fragment.
2. Several high-risk concerns are cross-cutting (security, migration, reliability, supply chain) and can stall late if not synchronized.
3. Without strict gate artifacts, "feature exists" can be mistaken for "release ready".
4. Cross-engine completion timelines can drift unless conformance closure is centralized.

## 8) Pros and Cons of Closing Through the PCG Program

### Pros

1. Explicit, auditable closure path for product expectations not covered by single feature tracks.
2. Gate-driven evidence avoids subjective release decisions.
3. Preserves existing architecture invariants while adding operational maturity.
4. Makes enterprise readiness measurable.

### Cons

1. Adds process and documentation overhead.
2. Extends total calendar time if resourcing is thin.
3. Requires strict discipline to keep ticket evidence current.

## 9) Research Materials Required for Execution

The following materials should be assembled per phase and linked in evidence bundles:

1. Backup/restore drill templates and RPO/RTO target contracts.
2. Upgrade compatibility matrix format and rollback checkpoint schemas.
3. Security policy templates (TLS, keys, audit immutability, masking, retention/legal hold).
4. Engine-version conformance vector corpus and diff-oracle policy.
5. CDC divergence taxonomy and reconciliation rules.
6. SLO/error-budget policy templates and runbook standards.
7. Chaos/fault-injection scenario catalog and soak profile standards.
8. SBOM/provenance/signature/reproducible-build policy templates.
9. Compliance notice bundle template and deprecation policy template.
10. Integrated gameday script pack and launch handoff checklist.

## 10) Recommended Execution Shape

1. Keep SBLR/MGA invariants explicit in every phase.
2. Treat `PCG-GATE-05`, `PCG-GATE-06`, and `PCG-GATE-07` as launch-blocking by policy.
3. Force weekly synchronization between UDF/parser/migration/native/cluster owners and PCG owners.
4. Require every `PCG-*` closure to include negative-path evidence, not only happy-path tests.

## 11) Dossier Exit Criteria

1. Every identified gap maps to one or more `PCG-*` tickets.
2. Every ticket maps to objective gate artifacts.
3. Staffing/schedule plan exists with critical-path identification.
4. No conflict with SBLR-canonical and MGA invariants.
