# V3 Imported Trees Targeted Audit

**Date:** 2026-02-08  
**Scope:** `docs/specifications/parser/v3/Security Design Specification/`, `docs/specifications/parser/v3/Cluster Specification Work/`, `docs/specifications/parser/v3/Alpha_Phase_1_Archive/`  
**Goal:** Identify content that is draft, placeholder, legacy, or non-authoritative; flag items that prevent a low‑level AI from producing a complete system without ambiguity.

## Summary

**Result:** The imported trees are **not fully authoritative**. Security and Cluster specs are explicitly marked “Draft for Review” in multiple files; Alpha archive contains many placeholder and legacy artifacts. These must be normalized or explicitly fenced as non‑authoritative before “no‑grey‑areas” status can be claimed.

## Security Design Specification Findings

1. **Draft status on primary security documents.** Many core documents are marked “Draft for Review”, which means they are not final and may conflict with later edits. Examples:
1. `docs/specifications/parser/v3/Security Design Specification/00_SECURITY_SPEC_INDEX.md`
1. `docs/specifications/parser/v3/Security Design Specification/01_SECURITY_ARCHITECTURE.md`
1. `docs/specifications/parser/v3/Security Design Specification/02_IDENTITY_AUTHENTICATION.md`
1. `docs/specifications/parser/v3/Security Design Specification/03_AUTHORIZATION_MODEL.md`
1. `docs/specifications/parser/v3/Security Design Specification/04_ENCRYPTION_KEY_MANAGEMENT.md`
1. `docs/specifications/parser/v3/Security Design Specification/05_IPC_SECURITY.md`
1. `docs/specifications/parser/v3/Security Design Specification/06_CLUSTER_SECURITY.md`
1. `docs/specifications/parser/v3/Security Design Specification/07_NETWORK_PRESENCE_BINDING.md`
1. `docs/specifications/parser/v3/Security Design Specification/08_AUDIT_COMPLIANCE.md`
1. `docs/specifications/parser/v3/Security Design Specification/09_SECURITY_LEVELS.md`

2. **Archived draft security work still present and potentially conflicting.** The `archive security work/` subtree contains draft or alternate specs that can contradict the primary documents.
1. `docs/specifications/parser/v3/Security Design Specification/archive security work/draft_security_architecture_specification.md`
1. `docs/specifications/parser/v3/Security Design Specification/archive security work/SECURITY_IMPLIMENTATION_DETAILS.md`

3. **Deprecated security mechanisms are mentioned without deprecation policy.** Example deprecations (MD5, TLS 1.2 compatibility) are referenced but not tied to clear enable/disable rules:
1. `docs/specifications/parser/v3/Security Design Specification/AUTH_PASSWORD_METHODS.md`
1. `docs/specifications/parser/v3/Security Design Specification/AUTH_CERTIFICATE_TLS.md`

## Cluster Specification Work Findings

1. **Explicit draft document in the cluster tree.** The following document is labeled draft and sits alongside authoritative SBCLUSTER specs, creating ambiguity for implementers:
1. `docs/specifications/parser/v3/Cluster Specification Work/scratch_bird_cluster_architecture_security_specifications_draft.md`

2. **Future‑work markers are present without a strict separation from core requirements.** Example:
1. `docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-IMPLEMENTATION-BOUNDARY.md` (Post‑GA and future‑work items are mixed with core content)

3. **Deprecated protocol options referenced without a formal compatibility policy.** Example:
1. `docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-02-MEMBERSHIP-AND-IDENTITY.md` (TLS 1.2 allowed for compatibility, deprecated)

## Alpha_Phase_1_Archive Findings

1. **Multiple placeholder docs exist in the archive and are linked by other legacy artifacts.** These are non‑authoritative but visible to a reader/AI:
1. `docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/README.md`
1. `docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/COMPREHENSIVE_CODE_ANALYSIS_REPORT.md`
1. `docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/COMPREHENSIVE_DOCUMENTATION_ANALYSIS_REPORT.md`
1. `docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/MVCC_VS_MGA_CODE_REVIEW.md`

2. **Legacy planning documents contain “placeholder” code paths that are now misleading.** Examples:
1. `docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SWEEP_INTEGRATION_PLAN.md`
1. `docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/BRIN_INDEX_COMPLETION_SPEC.md`

3. **Large volume of deprecated or legacy content introduces noise for AI consumption.** This tree is clearly historical and should be fenced as non‑authoritative if used by AI.

## Recommendations (Blocking to “No‑Grey‑Areas”)

1. **Security:** Either finalize the security documents (remove “Draft for Review” markers) or add a top‑level “Authoritative vs Draft” banner that strictly prevents AI from using archived draft docs.
2. **Cluster:** Either remove or explicitly fence `scratch_bird_cluster_architecture_security_specifications_draft.md` as non‑authoritative.
3. **Alpha Archive:** Mark the entire `Alpha_Phase_1_Archive` as **non‑authoritative** with a single root README that instructs AI/implementers to ignore it unless explicitly requested.

## Readiness Impact

**Current state:** Not ready for a low‑level AI to build the engine without ambiguity. The above items must be resolved or fenced.
