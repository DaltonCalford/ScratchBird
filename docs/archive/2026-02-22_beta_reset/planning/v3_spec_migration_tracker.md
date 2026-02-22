# V3 Spec Migration Tracker

Legend:
- `[ ]` not started
- `[~]` in progress / partially verified
- `[*]` verified complete

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ACCESS_CONTROL.md
Status: [~]
Spec: V3 Parser: GRANT and REVOKE (Access Control)
Tasks:
[~] Grammar: privilege grants/revokes with optional object_type - ON is required; object_type defaults to TABLE when unrecognized; see `src/parser/parser_v3.cpp:10521`
[ ] Grammar: role grants/revokes (no ON) with ADMIN OPTION / ADMIN OPTION FOR - no V3 role-grant parsing; see `src/parser/parser_v3.cpp:10521` and `include/scratchbird/parser/ast_v3.h:2173`
[~] Privilege list: SELECT, INSERT, UPDATE, DELETE, TRUNCATE, REFERENCES, TRIGGER, EXECUTE, EXECUTE EXTERNAL JOB, USAGE, COPY, CREATE JOB, VIEW JOB HISTORY, ALL - GRANT supports all; REVOKE missing TRUNCATE/REFERENCES/TRIGGER/USAGE; see `src/parser/parser_v3.cpp:10521`
[ ] Object types: TABLE, VIEW, SEQUENCE, FUNCTION, PROCEDURE, SCHEMA, DATABASE, INDEX, DOMAIN, TYPE, JOB, POLICY, SERVER, FOREIGN TABLE, SYNONYM - V3 parser supports only TABLE/JOB/SEQUENCE/FUNCTION/PROCEDURE/SCHEMA/DATABASE; enum lacks many types; see `src/parser/parser_v3.cpp:10553` and `include/scratchbird/parser/ast_v3.h:2147`
[ ] Grantee parsing: PUBLIC and USER/ROLE/GROUP keywords with ROLE->GROUP->USER resolution and ambiguity error - only PUBLIC or bare identifier supported; see `src/parser/parser_v3.cpp:10590`
[ ] Privilege matrix validation at parse time with SQLSTATE 0A000/42809 - no validation in parser; see `src/parser/parser_v3.cpp:10521`
[~] AST fields for Grant/Revoke: kind, roles list, grantees list, admin/grant options, cascade/restrict - only privileges/object/grantees + grant_option/cascade; no roles/admin/restrict; see `include/scratchbird/parser/ast_v3.h:2173`
[ ] SBLR emission: SBLR3_GRANT + GRANT_PRIVILEGE, GRANT_OPTION, GRANT_ROLE/REVOKE_ROLE; string_id payloads; object_list - emitter uses single SCHEMA_GRANT_REVOKE with bitmask and only first object; see `src/parser/v3_emitter.cpp:2118` and `src/sblr/v3_schema.generated.cpp:593`
[ ] Catalog storage: sys.sec.privileges and sys.sec.role_members with SBDB$ domains - storage uses internal permissions/role_membership heap tables; see `src/core/catalog_manager.cpp:3000` and records around permissions/role_memberships
[ ] Executor semantics: grantor rights, ALL PRIVILEGES expansion, GRANT OPTION FOR, CASCADE/RESTRICT, ADMIN OPTION FOR - no V3 SBLR3_GRANT/REVOKE executor handling; legacy EXT_* executor path only; see `src/sblr/executor.cpp:3289` and absence of SBLR3_GRANT cases
[ ] Transactional behavior and MGA rollback for GRANT/REVOKE - not verified in V3 path; no dedicated handling found
[ ] Lock ordering per EXECUTOR_V3_SBLR and EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX; lexicographic UUID order - not implemented/verified in V3 grant/revoke path
[ ] SQLSTATE error codes for access control failures - errors raised as strings; no SQLSTATE mapping in V3 path
[ ] Deterministic privilege list canonicalization in emission - not implemented; V3 emitter uses bitmask without per-privilege emission

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ARCHITECTURE_CLARIFICATIONS.md
Status: [~]
Spec: Architecture Clarifications (non-authoritative; internally claims authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md` (internal “Authoritative Reference” label conflicts).
[~] Architecture guidance only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/AST_TYPE_AND_LITERAL_SPEC.md
Status: [~]
Spec: AST Type and Literal Specification (non-authoritative; internally claims authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md` (internal “Authoritative (V3)” label conflicts).
[ ] Spec `TypeSpec` structure not present in AST (uses `TypeName` instead).
[ ] Spec `ValueSpec` wrapper not present in AST.
[ ] AST `TypeKind` does not match spec’s expanded type list.
[~] Literal expression classes largely exist but differ in type references (U128 IDs, `TypeName` for range/array element types).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md
Status: [~]
Spec: Authoritative Spec Inventory
Tasks:
[*] Inventory file reviewed; hashes and path completeness not verified.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/IMPLEMENTATION_AUDIT.md
Status: [~]
Spec: Implementation Audit (archived, non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/INTEGRATION_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Integration Implementation Plan (archived, non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Implementation plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/BITMAP_INDEX_COMPLETION_PLAN.md
Status: [~]
Spec: Bitmap Index Completion Plan (archived, non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Implementation plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/BRIN_INDEX_COMPLETION_PLAN.md
Status: [~]
Spec: BRIN Index Completion Plan (archived, non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Implementation plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/COLUMNSTORE_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Columnstore Implementation Plan (archived, non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Implementation plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/GIST_INDEX_COMPLETION_PLAN.md
Status: [~]
Spec: GiST Index Completion Plan (archived, non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Implementation plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/HNSW_INDEX_COMPLETION_PLAN.md
Status: [~]
Spec: HNSW Index Completion Plan (archived, non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Implementation plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/LSM_TREE_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: LSM-Tree Implementation Plan (archived, non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Implementation plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/LSM_TREE_INTEGRATION_PLAN.md
Status: [~]
Spec: LSM-Tree Integration Plan (archived, non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Integration plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/SPGIST_INDEX_COMPLETION_PLAN.md
Status: [~]
Spec: SP-GiST Index Completion Plan (archived, non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Implementation plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/MISSING_FUNCTIONS_PRIORITY.md
Status: [~]
Spec: Missing Functions Priority Guide (archived, non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Backlog/prioritization only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/README.md
Status: [~]
Spec: Alpha Phase 1 Archive README (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Archive guidance only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/00_COMPREHENSIVE_AUDIT_PLAN.md
Status: [~]
Spec: Comprehensive Audit Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/04_DEFERRED_WORK_CORRECTED_ASSESSMENT.md
Status: [~]
Spec: Deferred Work Corrected Assessment (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Assessment only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/04_DEFERRED_WORK_INVENTORY.md
Status: [~]
Spec: Deferred Work Inventory (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Inventory only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/07_ALPHA_COMPLETION_ROADMAP.md
Status: [~]
Spec: Alpha Completion Roadmap (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Roadmap only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-18_CORE_STORAGE_AUDIT.md
Status: [~]
Spec: Core Storage Audit (2025-11-18) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-18_DATA_TYPE_SYSTEM_REPORT.md
Status: [~]
Spec: Data Type System Report (2025-11-18) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-18_ELEMENT_EXTRACTION_DETAILED.md
Status: [~]
Spec: Element Extraction Detailed (2025-11-18) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit/analysis report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-18_EXECUTIVE_SUMMARY.md
Status: [~]
Spec: Executive Summary (2025-11-18) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Summary only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-18_IMPLEMENTATION_GAPS.md
Status: [~]
Spec: Implementation Gaps (2025-11-18) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Gap analysis only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-18_SECURITY_CRITICAL_ISSUES.md
Status: [~]
Spec: Security Critical Issues (2025-11-18) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Issue summary only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-19_AUDIT_CORRECTIONS_REPORT.md
Status: [~]
Spec: Audit Corrections Report (2025-11-19) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-19_CONSTRAINT_INDEX_VERIFICATION.md
Status: [~]
Spec: Constraint Index Verification (2025-11-19) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Verification report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-19_CONSTRAINT_SYSTEM_CRITICAL_ISSUES.md
Status: [~]
Spec: Constraint System Critical Issues (2025-11-19) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Issue summary only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-19_DATA_TYPE_SYSTEM_AUDIT.md
Status: [~]
Spec: Data Type System Audit (2025-11-19) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-19_DOCUMENTATION_VS_IMPLEMENTATION_DISCREPANCIES.md
Status: [~]
Spec: Documentation vs Implementation Discrepancies (2025-11-19) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Discrepancy report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-19_EXECUTIVE_SUMMARY.md
Status: [~]
Spec: Executive Summary (2025-11-19) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Summary only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-19_INDEX_SYSTEM_DETAILED_REPORT.md
Status: [~]
Spec: Index System Detailed Report (2025-11-19) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_COMPREHENSIVE_AUDIT_EXECUTIVE_SUMMARY.md
Status: [~]
Spec: Comprehensive Audit Executive Summary (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Summary only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_CONSTRAINT_ENFORCEMENT_VERIFICATION.md
Status: [~]
Spec: Constraint Enforcement Verification (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Verification report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_EXECUTIVE_SUMMARY.md
Status: [~]
Spec: Executive Summary (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Summary only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_INDEX_SYSTEM_AUDIT.md
Status: [~]
Spec: Index System Audit (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_INDEX_SYSTEM_AUDIT_CORRECTED.md
Status: [~]
Spec: Index System Audit Corrected (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit corrections only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_MEMORY_SAFETY_AUDIT.md
Status: [~]
Spec: Memory Safety Audit (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_MEMORY_SAFETY_FIXES_IMPLEMENTED.md
Status: [~]
Spec: Memory Safety Fixes Implemented (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Implementation notes only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_SECURITY_FIXES_IMPLEMENTATION_NOTES.md
Status: [~]
Spec: Security Fixes Implementation Notes (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Implementation notes only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_SECURITY_VULNERABILITIES_AUDIT.md
Status: [~]
Spec: Security Vulnerabilities Audit (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_TRANSACTION_MANAGEMENT_AUDIT.md
Status: [~]
Spec: Transaction Management Audit (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/CATALOG_CRUD_AUDIT.md
Status: [~]
Spec: Catalog CRUD Audit (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/CATALOG_SYSTEM_AUDIT.md
Status: [~]
Spec: Catalog System Audit (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/COLUMNSTORE_100_PERCENT_COMPLETE.md
Status: [~]
Spec: Columnstore 100% Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Status report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/COLUMNSTORE_PHASE2_3_SUMMARY.md
Status: [~]
Spec: Columnstore Phase 2-3 Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Summary report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/COMPREHENSIVE_CODE_AUDIT_2025-11-20.md
Status: [~]
Spec: Comprehensive Code Audit (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/COMPREHENSIVE_REVIEW_CHECKLIST.md
Status: [~]
Spec: Comprehensive Review Checklist (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Checklist only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/DATABASE_PAGE_SIZE_AUDIT_REPORT.md
Status: [~]
Spec: Database Page Size Audit Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/DATABASE_PAGE_SIZE_AUDIT_REPORT_UPDATED.md
Status: [~]
Spec: Database Page Size Audit Report Updated (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Updated audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/DOCUMENTATION_DISCREPANCY_REPORT.md
Status: [~]
Spec: Documentation Discrepancy Report (2025-11-06) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Discrepancy report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/EXECUTIVE_SUMMARY.md
Status: [~]
Spec: Executive Summary (2025-11-23) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Summary only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/FIREBIRD_SCRATCHBIRD_FEATURE_COMPARISON.md
Status: [~]
Spec: Firebird vs ScratchBird Feature Comparison (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Comparison report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/FUNCTION_VERIFICATION_REPORT.md
Status: [~]
Spec: Function Verification Report (2025-11-06) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Verification report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/IMPROVEMENT_OPPORTUNITIES.md
Status: [~]
Spec: Improvement Opportunities (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Improvement backlog only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_COMPLIANCE_ANALYSIS_SUMMARY.md
Status: [~]
Spec: Index Compliance Analysis Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Summary report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_COMPLIANCE_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Index Compliance Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Implementation plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_COMPLIANCE_PROGRESS_REPORT.md
Status: [~]
Spec: Index Compliance Progress Report (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Progress report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_IMPLEMENTATION_AUDIT_RESULTS.md
Status: [~]
Spec: Index Implementation Audit Results (2025-11-19) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit results only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_ISSUES_DETAILED.txt
Status: [~]
Spec: Index Issues Detailed (2025-11-06) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Issue list only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_OPTIONAL_WORK_PLAN.md
Status: [~]
Spec: Index Optional Work Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Optional work plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_SYSTEM_AGENT_TASKS.md
Status: [~]
Spec: Index System Agent Tasks (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Task breakdown only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_SYSTEM_AUDIT_REPORT.md
Status: [~]
Spec: Index System Audit Report (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_SYSTEM_COMPREHENSIVE_AUDIT.md
Status: [~]
Spec: Index System Comprehensive Audit (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_SYSTEM_REMEDIATION_PLAN.md
Status: [~]
Spec: Index System Remediation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Remediation plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_VERIFICATION_REPORT_2025_11_06.md
Status: [~]
Spec: Index Verification Report (2025-11-06) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Verification report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_VERIFICATION_SUMMARY.csv
Status: [~]
Spec: Index Verification Summary CSV (2025-11-06) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] CSV summary only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/MGA_CORRECTNESS_REVIEW.md
Status: [~]
Spec: MGA Correctness Review (2025-11-03) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Review notes only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/PATH_TO_100_PERCENT.md
Status: [~]
Spec: Path to 100% Production Readiness (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Status/plan report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/QUERY_PLANNER_INDEX_INTEGRATION_STATUS.md
Status: [~]
Spec: Query Planner Index Integration Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Status report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/README.md
Status: [~]
Spec: Archive Audit Reports Index (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Index/summary only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/RTREE_MGA_AUDIT.md
Status: [~]
Spec: R-Tree MGA Audit (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/SECURITY_AUDIT_REPORT.md
Status: [~]
Spec: Security Audit Report (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/SECURITY_AUDIT_SUMMARY.md
Status: [~]
Spec: Security Audit Summary (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Summary only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/SECURITY_SYSTEM_COMPREHENSIVE_AUDIT.md
Status: [~]
Spec: Security System Comprehensive Audit (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/SQL_PARSER_BYTECODE_AUDIT_REPORT.md
Status: [~]
Spec: SQL Parser/Bytecode Audit Report (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/SQL_PARSER_BYTECODE_COMPREHENSIVE_AUDIT.md
Status: [~]
Spec: SQL Parser/Bytecode Comprehensive Audit (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/SQL_STATEMENT_VERIFICATION_REPORT.md
Status: [~]
Spec: SQL Statement Verification Report (2025-11-06) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Verification report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/STORAGE_ENGINE_AUDIT_REPORT.md
Status: [~]
Spec: Storage Engine Audit Report (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/TASK_BYTECODE_3_COMPLETION_REPORT.md
Status: [~]
Spec: Task Bytecode 3 Completion Report (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Completion report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/TECHNICAL_FINDINGS.md
Status: [~]
Spec: Technical Findings (2025-11-23) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/bytecode_executor_audit_report.md
Status: [~]
Spec: Bytecode Executor Audit Report (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/executor_dml_audit_report.md
Status: [~]
Spec: Executor DML Audit Report (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/00_EXECUTIVE_SUMMARY.md
Status: [~]
Spec: Executive Summary (2025-11-01) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/01_MGA_COMPLIANCE_AUDIT.md
Status: [~]
Spec: MGA Compliance Audit (2025-11-01) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/02_TOAST_IMPLEMENTATION_AUDIT.md
Status: [~]
Spec: TOAST Implementation Audit (2025-11-01) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/03_SQL_IDENTIFIER_AUDIT.md
Status: [~]
Spec: SQL Identifier Audit (2025-11-01) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/AUDIT_CORRECTIONS_SUMMARY.md
Status: [~]
Spec: Audit Corrections Summary (2025-10-25) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/COMPREHENSIVE_PRIORITY_AUDIT_2025-10-25.md
Status: [~]
Spec: Comprehensive Priority Audit (2025-10-25) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/CORRECTED_COMPREHENSIVE_AUDIT_2025-10-25.md
Status: [~]
Spec: Corrected Comprehensive Audit (2025-10-25) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/FEATURE_PARITY_GAP_ANALYSIS.md
Status: [~]
Spec: Feature Parity Gap Analysis (2025-10-25) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/FUNCTION_COMPLETENESS_AUDIT.md
Status: [~]
Spec: Function Completeness Audit (2025-10-25) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/INDEX_TYPE_COMPLETENESS_AUDIT.md
Status: [~]
Spec: Index Type Completeness Audit (2025-10-25) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/PARSER_COVERAGE_AUDIT.md
Status: [~]
Spec: Parser Coverage Audit (2025-10-25) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/PHASE_1_COMPLETION_AUDIT.md
Status: [~]
Spec: Phase 1 Completion Audit (2025-10-28) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/QUERY_OPTIMIZATION_AUDIT.md
Status: [~]
Spec: Query Optimization Audit (2025-10-25) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/README.md
Status: [~]
Spec: Audit Archive README (2025-10-24) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/SCHEMA_STRUCTURE_AUDIT.md
Status: [~]
Spec: Schema Structure Audit (2025-10-25) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/TYPE_SYSTEM_COMPLETENESS_AUDIT.md
Status: [~]
Spec: Type System Completeness Audit (2025-10-25) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/AUDIT_FIXES_MASTER_TODO.md
Status: [~]
Spec: Audit Fixes Master Todo (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/BTREE_COMPRESSION_IMPLEMENTATION_SUMMARY.md
Status: [~]
Spec: Btree Compression Implementation Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/BTREE_COMPRESSION_TESTING_PLAN.md
Status: [~]
Spec: Btree Compression Testing Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/BTREE_COMPRESSION_TEST_RESULTS.md
Status: [~]
Spec: Btree Compression Test Results (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/BUFFER_POOL_EXHAUSTION_RESULTS.md
Status: [~]
Spec: Buffer Pool Exhaustion Results (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/CI_CD_IMPLEMENTATION_SUMMARY.md
Status: [~]
Spec: Ci Cd Implementation Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/COMPREHENSIVE_AUDIT_REPORT.md
Status: [~]
Spec: Comprehensive Audit Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/COMPREHENSIVE_TESTING_SUMMARY.md
Status: [~]
Spec: Comprehensive Testing Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/CONCURRENT_PAGE_ACCESS_RESULTS.md
Status: [~]
Spec: Concurrent Page Access Results (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/EXCEPTION_INJECTION_TEST_RESULTS.md
Status: [~]
Spec: Exception Injection Test Results (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FINAL_STATUS_BTREE_COMPRESSION_OCT17.md
Status: [~]
Spec: Final Status Btree Compression Oct17 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FIX_1.1_CRC32C_VERIFICATION_REPORT.md
Status: [~]
Spec: Fix 1.1 Crc32C Verification Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FIX_1.2_ATOMIC_XID_VERIFICATION_REPORT.md
Status: [~]
Spec: Fix 1.2 Atomic Xid Verification Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FIX_1.3_BUFFER_POOL_LRU_VERIFICATION_REPORT.md
Status: [~]
Spec: Fix 1.3 Buffer Pool Lru Verification Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FIX_1.4_HEAP_PAGE_MEMORY_LEAK_ANALYSIS.md
Status: [~]
Spec: Fix 1.4 Heap Page Memory Leak Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FIX_1.4_HEAP_PAGE_MEMORY_LEAK_VERIFICATION_REPORT.md
Status: [~]
Spec: Fix 1.4 Heap Page Memory Leak Verification Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FIX_1.5_FSYNC_VERIFICATION_REPORT.md
Status: [~]
Spec: Fix 1.5 Fsync Verification Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FIX_1.6_CONST_CORRECTNESS_VERIFICATION_REPORT.md
Status: [~]
Spec: Fix 1.6 Const Correctness Verification Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/HELGRIND_AND_STRESS_TESTS_RESULTS.md
Status: [~]
Spec: Helgrind And Stress Tests Results (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_2_16_STATUS.md
Status: [~]
Spec: Issue 2 16 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_2_17_STATUS.md
Status: [~]
Spec: Issue 2 17 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_2_18_STATUS.md
Status: [~]
Spec: Issue 2 18 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_2_19_STATUS.md
Status: [~]
Spec: Issue 2 19 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_2_20_STATUS.md
Status: [~]
Spec: Issue 2 20 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_10_STATUS.md
Status: [~]
Spec: Issue 3 10 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_1_STATUS.md
Status: [~]
Spec: Issue 3 1 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_2_STATUS.md
Status: [~]
Spec: Issue 3 2 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_3_STATUS.md
Status: [~]
Spec: Issue 3 3 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_4_STATUS.md
Status: [~]
Spec: Issue 3 4 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_5_STATUS.md
Status: [~]
Spec: Issue 3 5 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_6_STATUS.md
Status: [~]
Spec: Issue 3 6 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_7_STATUS.md
Status: [~]
Spec: Issue 3 7 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_8_STATUS.md
Status: [~]
Spec: Issue 3 8 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_9_STATUS.md
Status: [~]
Spec: Issue 3 9 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/MEDIUM_PRIORITY_TEST_RESULTS.md
Status: [~]
Spec: Medium Priority Test Results (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/OCTOBER_17_2025_COMPLETION_SUMMARY.md
Status: [~]
Spec: October 17 2025 Completion Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/SESSION_COMPLETION_SUMMARY_OCT17.md
Status: [~]
Spec: Session Completion Summary Oct17 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/TSAN_BUFFER_POOL_FINAL_RESULTS.md
Status: [~]
Spec: Tsan Buffer Pool Final Results (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/TSAN_BUFFER_POOL_TEST_RESULTS.md
Status: [~]
Spec: Tsan Buffer Pool Test Results (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/TSAN_LOCK_ORDERING_RESULTS.md
Status: [~]
Spec: Tsan Lock Ordering Results (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/faulty_2025-10-24/ALPHA_COMPLETENESS_ASSESSMENT.md
Status: [~]
Spec: Faulty Audit 2025-10-24 - Alpha Completeness Assessment (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/faulty_2025-10-24/COMPONENT_VERIFICATION_REPORT.md
Status: [~]
Spec: Faulty Audit 2025-10-24 - Component Verification Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/faulty_2025-10-24/COMPREHENSIVE_CODE_AUDIT_2025-10-24.md
Status: [~]
Spec: Faulty Audit 2025-10-24 - Comprehensive Code Audit 2025-10-24 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/faulty_2025-10-24/CRITICAL_DISCREPANCIES_SUMMARY.md
Status: [~]
Spec: Faulty Audit 2025-10-24 - Critical Discrepancies Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/faulty_2025-10-24/PLANNING_DOCUMENTS_REORGANIZATION.md
Status: [~]
Spec: Faulty Audit 2025-10-24 - Planning Documents Reorganization (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/faulty_2025-10-24/README.md
Status: [~]
Spec: Faulty Audit 2025-10-24 - Readme (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/faulty_2025-10-24/component_verification_script.sh
Status: [~]
Spec: Faulty Audit 2025-10-24 - Component Verification Script (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/faulty_2025-10-24/reorganize_planning_docs.sh
Status: [~]
Spec: Faulty Audit 2025-10-24 - Reorganize Planning Docs (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_COMPLETION_COMPREHENSIVE_ANALYSIS.md
Status: [~]
Spec: Older Audit - Alpha Completion Comprehensive Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_COMPLETION_DETAILED_TODO.md
Status: [~]
Spec: Older Audit - Alpha Completion Detailed Todo (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_COMPLETION_DETAILED_TODO_PART2.md
Status: [~]
Spec: Older Audit - Alpha Completion Detailed Todo Part2 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_COMPLETION_DETAILED_TODO_PART3.md
Status: [~]
Spec: Older Audit - Alpha Completion Detailed Todo Part3 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_EXECUTIVE_SUMMARY.md
Status: [~]
Spec: Older Audit - Alpha Executive Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_FINAL_COMPREHENSIVE_AUDIT.md
Status: [~]
Spec: Older Audit - Alpha Final Comprehensive Audit (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_ISSUES_TRACKER.md
Status: [~]
Spec: Older Audit - Alpha Issues Tracker (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_PLANNING_INDEX.md
Status: [~]
Spec: Older Audit - Alpha Planning Index (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/AUDIT_SUMMARY_OCT_16_2025.md
Status: [~]
Spec: Older Audit - Audit Summary Oct 16 2025 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/INDEX_MGA_ALPHA_READINESS_SUMMARY.md
Status: [~]
Spec: Older Audit - Index Mga Alpha Readiness Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/INDEX_MGA_COMPLIANCE_ANALYSIS.md
Status: [~]
Spec: Older Audit - Index Mga Compliance Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/MGA_COMPLIANCE_REVIEW_TABLESPACE.md
Status: [~]
Spec: Older Audit - Mga Compliance Review Tablespace (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/MVCC_VS_MGA_CODE_REVIEW.md
Status: [~]
Spec: Older Audit - Mvcc Vs Mga Code Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_work/after_transaction_documentation_work.md
Status: [~]
Spec: Older Work - After Transaction Documentation Work (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_work/after_transaction_work.md
Status: [~]
Spec: Older Work - After Transaction Work (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_work/gemini_audit.md
Status: [~]
Spec: Older Work - Gemini Audit (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_work/gemini_docs_audit.md
Status: [~]
Spec: Older Work - Gemini Docs Audit (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_work/type_cast_safety_audit.md
Status: [~]
Spec: Older Work - Type Cast Safety Audit (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/2025-09-10.REVIEW.md
Status: [~]
Spec: Legacy Review - 2025-09-10 Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/AGENT_B_REVIEW_OF_AGENT_A_FIXES.md
Status: [~]
Spec: Legacy Review - Agent B Review Of Agent A Fixes (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/AGENT_B_REVIEW_STAGE_1_1_EXTENDED_PAGE_SIZES.md
Status: [~]
Spec: Legacy Review - Agent B Review Stage 1 1 Extended Page Sizes (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/README.md
Status: [~]
Spec: Legacy Review - Readme (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/REVIEW_TEMPLATE.md
Status: [~]
Spec: Legacy Review - Review Template (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/agent_b_code_review_2025-09-08.md
Status: [~]
Spec: Legacy Review - Agent B Code Review 2025-09-08 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/agent_b_codebase_analysis_2025_09_16.md
Status: [~]
Spec: Legacy Review - Agent B Codebase Analysis 2025 09 16 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/agent_b_heap_toast_review_2025-09-08.md
Status: [~]
Spec: Legacy Review - Agent B Heap Toast Review 2025-09-08 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_03_storage_engine_code_review_final.md
Status: [~]
Spec: Legacy Review - Alpha 1 03 Storage Engine Code Review Final (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_03_storage_engine_final_analysis.md
Status: [~]
Spec: Legacy Review - Alpha 1 03 Storage Engine Final Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_03_storage_engine_fixes_applied.md
Status: [~]
Spec: Legacy Review - Alpha 1 03 Storage Engine Fixes Applied (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_04_transaction_foundation_final_review.md
Status: [~]
Spec: Legacy Review - Alpha 1 04 Transaction Foundation Final Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_04_transaction_foundation_fix_report.md
Status: [~]
Spec: Legacy Review - Alpha 1 04 Transaction Foundation Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_04_transaction_foundation_review.md
Status: [~]
Spec: Legacy Review - Alpha 1 04 Transaction Foundation Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_05_bytecode_generator_review.md
Status: [~]
Spec: Legacy Review - Alpha 1 05 Bytecode Generator Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_05_complete_final_review.md
Status: [~]
Spec: Legacy Review - Alpha 1 05 Complete Final Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_05_semantic_analyzer_review.md
Status: [~]
Spec: Legacy Review - Alpha 1 05 Semantic Analyzer Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_05_sql_lexer_code_review.md
Status: [~]
Spec: Legacy Review - Alpha 1 05 Sql Lexer Code Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_05_sql_parser_complete_review.md
Status: [~]
Spec: Legacy Review - Alpha 1 05 Sql Parser Complete Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/comprehensive_uuid_audit_2025_09_16.md
Status: [~]
Spec: Legacy Review - Comprehensive Uuid Audit 2025 09 16 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/firebase_review_2025_09_15.md
Status: [~]
Spec: Legacy Review - Firebase Review 2025 09 15 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/phase_1_01_1_followup_review.md
Status: [~]
Spec: Legacy Review - Phase 1 01 1 Followup Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/phase_1_01_1_review.md
Status: [~]
Spec: Legacy Review - Phase 1 01 1 Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/phase_1_01_2_followup_review.md
Status: [~]
Spec: Legacy Review - Phase 1 01 2 Followup Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/phase_1_01_2_review.md
Status: [~]
Spec: Legacy Review - Phase 1 01 2 Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/phase_1_03_catalog_review.md
Status: [~]
Spec: Legacy Review - Phase 1 03 Catalog Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/uuid_migration_impact_report.md
Status: [~]
Spec: Legacy Review - Uuid Migration Impact Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/development_archive/COMPREHENSIVE_CODE_ANALYSIS_REPORT.md
Status: [~]
Spec: Comprehensive Code Analysis Report (2025-10-01) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/development_archive/COMPREHENSIVE_DOCUMENTATION_ANALYSIS_REPORT.md
Status: [~]
Spec: Comprehensive Documentation Analysis Report (2025-09-30) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/implementation/INDEX_BYTECODE_GENERATION.md
Status: [~]
Spec: Index Bytecode Generation (2025-11-20) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/implementation/LSM_TREE_RANGE_SCAN_IMPLEMENTATION_2025-11-06.md
Status: [~]
Spec: LSM-Tree Range Scan Implementation (2025-11-06) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/implementation/LSM_TREE_RANGE_SCAN_PROGRESS_2025-11-06.md
Status: [~]
Spec: LSM-Tree Range Scan Progress (2025-11-06) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/implementation/views-implementation.md
Status: [~]
Spec: Views Implementation (Alpha Phase 1) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/issues/ALPHA_1_2_REQUIREMENTS.md
Status: [~]
Spec: Alpha 1.2 Requirements (2025-10-07) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/COST_MODEL_DESIGN.md
Status: [~]
Spec: Planning Archive - Cost Model Design (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/EXPLAIN_COMMAND_DESIGN.md
Status: [~]
Spec: Planning Archive - Explain Command Design (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/FEATURE_PARITY_ROADMAP.md
Status: [~]
Spec: Planning Archive - Feature Parity Roadmap (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/JOIN_IMPLEMENTATION_COMPLETION.md
Status: [~]
Spec: Planning Archive - Join Implementation Completion (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/JOIN_PLANNER_DESIGN.md
Status: [~]
Spec: Planning Archive - Join Planner Design (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/PHASE_1_CLEANUP_GUIDE.md
Status: [~]
Spec: Planning Archive - Phase 1 Cleanup Guide (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/PHASE_2_KICKOFF.md
Status: [~]
Spec: Planning Archive - Phase 2 Kickoff (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/PHASE_2_WAVE_2_STRATEGY.md
Status: [~]
Spec: Planning Archive - Phase 2 Wave 2 Strategy (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/PLANNER_INTEGRATION_DESIGN.md
Status: [~]
Spec: Planning Archive - Planner Integration Design (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/PSQL_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Psql Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/QUERY_PLANNER_DESIGN.md
Status: [~]
Spec: Planning Archive - Query Planner Design (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/SELECTIVITY_ESTIMATION_DESIGN.md
Status: [~]
Spec: Planning Archive - Selectivity Estimation Design (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/STATISTICS_COLLECTION_DESIGN.md
Status: [~]
Spec: Planning Archive - Statistics Collection Design (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_14_FULL_TEXT_SEARCH_PROJECT_PLAN.md
Status: [~]
Spec: Planning Archive - Task 14 Full Text Search Project Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_14_TEXT_SEARCH_TYPES_PLAN.md
Status: [~]
Spec: Planning Archive - Task 14 Text Search Types Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_15_PHASE_6_GIST_DESIGN.md
Status: [~]
Spec: Planning Archive - Task 15 Phase 6 Gist Design (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_17_COMPLETE_IMPLEMENTATION_GUIDE.md
Status: [~]
Spec: Planning Archive - Task 17 Complete Implementation Guide (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_17_EXPRESSION_FILTERED_INDEXES_DESIGN.md
Status: [~]
Spec: Planning Archive - Task 17 Expression Filtered Indexes Design (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_17_MGA_COMPLIANCE_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Task 17 Mga Compliance Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_17_MGA_COMPLIANCE_IMPLEMENTATION_PLAN_REVISED.md
Status: [~]
Spec: Planning Archive - Task 17 Mga Compliance Implementation Plan Revised (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_17_MGA_PHASE_3_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Task 17 Mga Phase 3 Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_17_PHASE_6_13_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Task 17 Phase 6 13 Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/WAVE_2_AGENT_SPECS.md
Status: [~]
Spec: Planning Archive - Wave 2 Agent Specs (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/ALPHA_003_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive Implemented - Alpha 003 Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/ALPHA_1_2_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive Implemented - Alpha 1 2 Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/MGA_PROPER_INDEX_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive Implemented - Mga Proper Index Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/PHASE_2_COMPLETE.md
Status: [~]
Spec: Planning Archive Implemented - Phase 2 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/PHASE_3_COMPLETE.md
Status: [~]
Spec: Planning Archive Implemented - Phase 3 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/PHASE_3_READONLY_OPTIMIZATIONS.md
Status: [~]
Spec: Planning Archive Implemented - Phase 3 Readonly Optimizations (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/SPRINT0_BUG_FIX_COMPLETE.md
Status: [~]
Spec: Planning Archive Implemented - Sprint0 Bug Fix Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/SPRINT1_FOUNDATION_COMPLETE.md
Status: [~]
Spec: Planning Archive Implemented - Sprint1 Foundation Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/SPRINT3_ONLINE_MIGRATION_ARCHITECTURE.md
Status: [~]
Spec: Planning Archive Implemented - Sprint3 Online Migration Architecture (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/SPRINT3_SUMMARY.md
Status: [~]
Spec: Planning Archive Implemented - Sprint3 Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/ALPHA_CONTEXT_VARIABLES_DESIGN.md
Status: [~]
Spec: Deprecated Plan - Alpha Context Variables Design (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/ALPHA_CONTEXT_VARIABLES_V2_SUMMARY.md
Status: [~]
Spec: Deprecated Plan - Alpha Context Variables V2 Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/ALPHA_ROW_IDENTITY_AND_TRANSACTION_VISIBILITY.md
Status: [~]
Spec: Deprecated Plan - Alpha Row Identity And Transaction Visibility (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/ALPHA_ROW_IDENTITY_ENHANCED.md
Status: [~]
Spec: Deprecated Plan - Alpha Row Identity Enhanced (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/INDEX_MGA_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Deprecated Plan - Index Mga Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/MGA_ONLINE_MIGRATION_ANALYSIS.md
Status: [~]
Spec: Deprecated Plan - Mga Online Migration Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/MVCC_VS_MGA_CODE_REVIEW.md
Status: [~]
Spec: Deprecated Plan - Mvcc Vs Mga Code Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/OFFLINE_TABLE_MIGRATION_DESIGN.md
Status: [~]
Spec: Deprecated Plan - Offline Table Migration Design (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/OFFLINE_TABLE_MIGRATION_TODOS.md
Status: [~]
Spec: Deprecated Plan - Offline Table Migration Todos (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE4_TASK4_1_1_PARSER_TEST.md
Status: [~]
Spec: Deprecated Plan - Phase4 Task4 1 1 Parser Test (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE4_TASK4_1_2_CATALOG_STUB.md
Status: [~]
Spec: Deprecated Plan - Phase4 Task4 1 2 Catalog Stub (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE4_TASK4_1_3_PROGRESS_TRACKING.md
Status: [~]
Spec: Deprecated Plan - Phase4 Task4 1 3 Progress Tracking (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE4_TASK4_1_4_BATCH_PROCESSING.md
Status: [~]
Spec: Deprecated Plan - Phase4 Task4 1 4 Batch Processing (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE4_TASK4_1_5_INDEX_TID_UPDATE.md
Status: [~]
Spec: Deprecated Plan - Phase4 Task4 1 5 Index Tid Update (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE4_TASK4_1_6_EXECUTOR.md
Status: [~]
Spec: Deprecated Plan - Phase4 Task4 1 6 Executor (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_1_HEAP_PAGE_MIGRATION.md
Status: [~]
Spec: Deprecated Plan - Phase5 1 Heap Page Migration (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_FULL_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Deprecated Plan - Phase5 Full Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_TASK5_1_1_HEAP_PAGE_ENUMERATION.md
Status: [~]
Spec: Deprecated Plan - Phase5 Task5 1 1 Heap Page Enumeration (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_TASK5_1_2_PAGE_COPYING_TID_REMAPPING.md
Status: [~]
Spec: Deprecated Plan - Phase5 Task5 1 2 Page Copying Tid Remapping (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_TASK5_1_3_TOAST_HANDLING.md
Status: [~]
Spec: Deprecated Plan - Phase5 Task5 1 3 Toast Handling (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_TASK5_1_4_TRANSACTION_ROLLBACK.md
Status: [~]
Spec: Deprecated Plan - Phase5 Task5 1 4 Transaction Rollback (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_TASK5_2_BTREE_TID_UPDATES.md
Status: [~]
Spec: Deprecated Plan - Phase5 Task5 2 Btree Tid Updates (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_TASK5_3_OTHER_INDEX_TID_UPDATES.md
Status: [~]
Spec: Deprecated Plan - Phase5 Task5 3 Other Index Tid Updates (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_TASK5_4_ONLINE_MIGRATION_ANALYSIS.md
Status: [~]
Spec: Deprecated Plan - Phase5 Task5 4 Online Migration Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE7_COMPLETE_SCOPE.md
Status: [~]
Spec: Deprecated Plan - Phase7 Complete Scope (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT2_IMPLEMENTATION_PROGRESS.md
Status: [~]
Spec: Deprecated Plan - Sprint2 Implementation Progress (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT2_INDEX_TOAST_ANALYSIS.md
Status: [~]
Spec: Deprecated Plan - Sprint2 Index Toast Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT2_SUMMARY.md
Status: [~]
Spec: Deprecated Plan - Sprint2 Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT2_SUMMARY.md.old
Status: [~]
Spec: Deprecated Plan - Sprint2 Summary.Md (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT4_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Deprecated Plan - Sprint4 Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT4_SUMMARY.md
Status: [~]
Spec: Deprecated Plan - Sprint4 Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT5_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Deprecated Plan - Sprint5 Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT5_SUMMARY.md
Status: [~]
Spec: Deprecated Plan - Sprint5 Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT7_PHASE7_PREPARATION.md
Status: [~]
Spec: Deprecated Plan - Sprint7 Phase7 Preparation (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SWEEP_INTEGRATION_PLAN.md
Status: [~]
Spec: Deprecated Plan - Sweep Integration Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md
Status: [~]
Spec: Deprecated Plan - Tablespace Complete Implementation Roadmap (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/TABLESPACE_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Deprecated Plan - Tablespace Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/TABLESPACE_ROADMAP_SUMMARY.md
Status: [~]
Spec: Deprecated Plan - Tablespace Roadmap Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/ALPHA_ADVANCED_SECURITY_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Alpha Advanced Security Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/ALTER_TABLE_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Alter Table Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/CATALOG_CORRECTION_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Catalog Correction Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/DATA_TYPE_COMPLETION_PLAN_2025-11-06.md
Status: [~]
Spec: Planning Archive - Data Type Completion Plan 2025-11-06 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/DDL_MODIFICATIONS_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Ddl Modifications Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/EXTRACT_FUNCTION_COMPREHENSIVE_PLAN.md
Status: [~]
Spec: Planning Archive - Extract Function Comprehensive Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/FK_PHASE_C_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Fk Phase C Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/INDEX_COMPLETION_ROADMAP.md
Status: [~]
Spec: Planning Archive - Index Completion Roadmap (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/INDEX_CORRECTION_ACTION_PLAN_2025-11-06.md
Status: [~]
Spec: Planning Archive - Index Correction Action Plan 2025-11-06 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/INDEX_IMPLEMENTATION_FIX_PLAN.md
Status: [~]
Spec: Planning Archive - Index Implementation Fix Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/INDEX_INTEGRATION_COMPLETE_GUIDE.md
Status: [~]
Spec: Planning Archive - Index Integration Complete Guide (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/INDEX_INTEGRATION_IMPLEMENTATION_SUMMARY.md
Status: [~]
Spec: Planning Archive - Index Integration Implementation Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/MISSING_FUNCTIONS_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Missing Functions Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/PERMISSION_CACHE_OPTIMIZATION_PHASE3_2_3.md
Status: [~]
Spec: Planning Archive - Permission Cache Optimization Phase3 2 3 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/QUERY_PLAN_SECURITY_INTEGRATION.md
Status: [~]
Spec: Planning Archive - Query Plan Security Integration (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/QUERY_PLAN_SECURITY_PHASE3_2.md
Status: [~]
Spec: Planning Archive - Query Plan Security Phase3 2 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/SECURITY_PHASE3_3_COLUMN_LEVEL_PLAN.md
Status: [~]
Spec: Planning Archive - Security Phase3 3 Column Level Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/SECURITY_PHASE3_4_RLS_PLAN.md
Status: [~]
Spec: Planning Archive - Security Phase3 4 Rls Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/SECURITY_PHASE3_FINAL_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Security Phase3 Final Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/SECURITY_SYSTEM_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Security System Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/SEQUENCE_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Sequence Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/SQL_OBJECT_PERMISSIONS_DESIGN.md
Status: [~]
Spec: Planning Archive - Sql Object Permissions Design (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/TRUNCATE_IMPLEMENTATION_CODE.md
Status: [~]
Spec: Planning Archive - Truncate Implementation Code (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/TRUNCATE_TABLE_ASYNC_IMPLEMENTATION.md
Status: [~]
Spec: Planning Archive - Truncate Table Async Implementation (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/TRUNCATE_TABLE_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Truncate Table Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/VIEWS_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive - Views Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/legacy-plans/IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Legacy Plan - Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/legacy-plans/README.md
Status: [~]
Spec: Legacy Plan - Readme (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/legacy-plans/archive/uuid_migration_plan.md
Status: [~]
Spec: Legacy Plan Archive - Uuid Migration Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/legacy-plans/code_quality_remediation_plan_2025_09_16.md
Status: [~]
Spec: Legacy Plan - Code Quality Remediation Plan 2025 09 16 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/legacy-plans/column_uuid_migration_plan.md
Status: [~]
Spec: Legacy Plan - Column Uuid Migration Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/legacy-plans/id_alias_remediation_plan.md
Status: [~]
Spec: Legacy Plan - Id Alias Remediation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/legacy-plans/remediation_plan_2025_09_15.md
Status: [~]
Spec: Legacy Plan - Remediation Plan 2025 09 15 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/ALPHA1_ADVANCED_SQL_FEATURES_PLAN.md
Status: [~]
Spec: Planning Archive (1) - Alpha1 Advanced Sql Features Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/ALPHA1_CLI_TOOLS_AND_VIEWS_COMPLETION_PLAN.md
Status: [~]
Spec: Planning Archive (1) - Alpha1 Cli Tools And Views Completion Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/ALPHA1_CONSTRAINTS_AND_ENGINE_COMMANDS_PLAN.md
Status: [~]
Spec: Planning Archive (1) - Alpha1 Constraints And Engine Commands Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/ALPHA1_MASTER_COMPLETION_TRACKER.md
Status: [~]
Spec: Planning Archive (1) - Alpha1 Master Completion Tracker (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/ALPHA1_PSQL_TRIGGERS_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive (1) - Alpha1 Psql Triggers Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Planning Archive (1) - Alpha Phase1 Complete Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/CTE_IMPLEMENTATION_STATUS.md
Status: [~]
Spec: Planning Archive (1) - Cte Implementation Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/LSM_TREE_COMPLETION_PLAN.md
Status: [~]
Spec: Planning Archive (1) - Lsm Tree Completion Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/MERGE_AND_RETURNING_IMPLEMENTATION.md
Status: [~]
Spec: Planning Archive (1) - Merge And Returning Implementation (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/MGA_COMPLIANCE_FIX_PLAN.md
Status: [~]
Spec: Planning Archive (1) - Mga Compliance Fix Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/PHASE_3_REVISED_TASKS.md
Status: [~]
Spec: Planning Archive (1) - Phase 3 Revised Tasks (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/PSQL_IMPLEMENTATION_STATUS.md
Status: [~]
Spec: Planning Archive (1) - Psql Implementation Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/SQL_IDENTIFIER_UTF8_FIX_PLAN.md
Status: [~]
Spec: Planning Archive (1) - Sql Identifier Utf8 Fix Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/TOAST_MGA_COMPLIANCE_FIX_PLAN.md
Status: [~]
Spec: Planning Archive (1) - Toast Mga Compliance Fix Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/BITMAP_INDEX_COMPLETION_SPEC.md
Status: [~]
Spec: Index Spec Archive - Bitmap Index Completion Spec (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/BRIN_INDEX_COMPLETION_SPEC.md
Status: [~]
Spec: Index Spec Archive - Brin Index Completion Spec (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/GIST_INDEX_COMPLETION_SPEC.md
Status: [~]
Spec: Index Spec Archive - Gist Index Completion Spec (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/HNSW_INDEX_COMPLETION_SPEC.md
Status: [~]
Spec: Index Spec Archive - Hnsw Index Completion Spec (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/LOW_LEVEL_SPECIFICATION_B-TREE_INDEX.md
Status: [~]
Spec: Index Spec Archive - Low Level Specification B Tree Index (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/LOW_LEVEL_SPECIFICATION_BITMAP_INDEX.md
Status: [~]
Spec: Index Spec Archive - Low Level Specification Bitmap Index (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/LOW_LEVEL_SPECIFICATION_GIN_INDEX.md
Status: [~]
Spec: Index Spec Archive - Low Level Specification Gin Index (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/LOW_LEVEL_SPECIFICATION_HASH_INDEX.md
Status: [~]
Spec: Index Spec Archive - Low Level Specification Hash Index (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/SPGIST_INDEX_COMPLETION_SPEC.md
Status: [~]
Spec: Index Spec Archive - Spgist Index Completion Spec (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-12/CURRENT_STATUS.md
Status: [~]
Spec: Current Status (2025-10-12) (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/README.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 - Readme (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits/audit_2025_10_06.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits - Audit 2025 10 06 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/OctAudit/audit_2025_10_06.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Audit 2025 10 06 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/OctAudit/doc_audit.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Doc Audit (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/OctAudit/error_handling_audit_2025_10_07.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Error Handling Audit 2025 10 07 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/OctAudit/reconciliation_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Reconciliation Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/OctAudit/repair.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Repair (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/OctAudit/status_update.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Status Update (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/analysis/BUFFERPOOL_PIN_UNPIN_ANALYSIS.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Bufferpool Pin Unpin Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/analysis/DATATYPE_AUDIT_REPORT.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Datatype Audit Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/CATALOG_IMPLEMENTATION_2025_10_04.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Catalog Implementation 2025 10 04 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/CATALOG_SYSTEM_AUDIT_2025_10_03.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Catalog System Audit 2025 10 03 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/CHARACTER_SET_IMPLEMENTATION_2025_10_04.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Character Set Implementation 2025 10 04 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/COMPILATION_AUDIT_REPORT.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Compilation Audit Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/CORRECTED_STATUS_REPORT.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Corrected Status Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/btree_fix_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Btree Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/bufferpool_pin_unpin_analysis_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Bufferpool Pin Unpin Analysis Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/clog_procarray_vacuum_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Clog Procarray Vacuum Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/compilation_fixes_summary.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Compilation Fixes Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/cross_page_version_chains_fix_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Cross Page Version Chains Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/executor_tuple_fix_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Executor Tuple Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/page_lock_management_fix_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Page Lock Management Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/pointer_arithmetic_bounds_checking_fix_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Pointer Arithmetic Bounds Checking Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/pointer_safety_elimination_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Pointer Safety Elimination Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/timestamp_timezone_fix_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Timestamp Timezone Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/tip_chaining_fix_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Tip Chaining Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/toast_integration_fix_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Toast Integration Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/toast_integration_implementation_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Toast Integration Implementation Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/toast_thread_safety_fix_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Toast Thread Safety Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/type_conversion_fix_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Type Conversion Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/type_serialization_verification_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Type Serialization Verification Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/update_tuple_toast_fix_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Update Tuple Toast Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/varchar_max_length_fix_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Varchar Max Length Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/xid_validation_analysis.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Xid Validation Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/xid_validation_enhancements_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Xid Validation Enhancements Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/xid_validation_fix_report.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Xid Validation Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/specifications/design_limits.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Audits-Old - Design Limits (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/development/CODING_STANDARDS.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Development - Coding Standards (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/development/TODO.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Development - Todo (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/ADDITIONAL_FIXES_REPORT.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Issues - Additional Fixes Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/ALPHA_1_01_TO_1_05_REVISED_ASSESSMENT.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Issues - Alpha 1 01 To 1 05 Revised Assessment (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/ARCHITECTURE_CLARIFICATION.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Issues - Architecture Clarification (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/DEFICIENCY_ANALYSIS_AND_ACTION_PLAN.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Issues - Deficiency Analysis And Action Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/DEFICIENCY_CORRECTION_PLAN.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Issues - Deficiency Correction Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/IMPLEMENTATION_PROGRESS_REPORT.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Issues - Implementation Progress Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/ISSUE-001-test-failures.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Issues - Issue 001 Test Failures (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/OUTDATED_REPORTS_UPDATE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Issues - Outdated Reports Update (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/TIP_CORRUPTION_FIX_REPORT.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Issues - Tip Corruption Fix Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/ALPHA_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Alpha Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/BTREE_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Btree Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/CRITICAL_FIXES_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Critical Fixes Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/HASH_INDEX_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Hash Index Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/MGA_GAP_ANALYSIS.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Mga Gap Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/MGA_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Mga Implementation Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_1_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Phase 1 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_1_INTEGRATION_GUIDE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Phase 1 Integration Guide (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_1_INTEGRATION_STATUS.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Phase 1 Integration Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_2_PROGRESS.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Phase 2 Progress (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_3_PROGRESS.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Phase 3 Progress (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_3_TASK_3.4_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Phase 3 Task 3.4 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_4_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Phase 4 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_4_PART_1_PHYSICAL_TUPLE_REMOVAL_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Phase 4 Part 1 Physical Tuple Removal Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_4_PART_2_CONDITION_VARIABLE_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Phase 4 Part 2 Condition Variable Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_4_PART_3_ENHANCED_METRICS_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Phase 4 Part 3 Enhanced Metrics Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_4_PART_4_ADAPTIVE_TUNING_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Phase 4 Part 4 Adaptive Tuning Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_4_PART_5_PRIORITY_QUEUE_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Phase 4 Part 5 Priority Queue Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_4_PART_6_COMPREHENSIVE_TESTS_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Planning - Phase 4 Part 6 Comprehensive Tests Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/BTREE_IMPLEMENTATION_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Btree Implementation Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/BTREE_PHASE3_RANGE_SCAN_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Btree Phase3 Range Scan Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/BTREE_PHASE4_COMPRESSION_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Btree Phase4 Compression Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/BTREE_PHASE5_VACUUM_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Btree Phase5 Vacuum Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/BTREE_STATUS.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Btree Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/COMPREHENSIVE_CODE_ANALYSIS_REPORT.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Comprehensive Code Analysis Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/COMPREHENSIVE_DOCUMENTATION_ANALYSIS_REPORT.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Comprehensive Documentation Analysis Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/CURRENT_STATUS.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Current Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/HASH_INDEX_STATUS.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Hash Index Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/LEGACY_PROJECT_STATUS.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Legacy Project Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/LEGACY_STATUS.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Legacy Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/MGA_IMPLEMENTATION_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Mga Implementation Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/MGA_IMPLEMENTATION_STATUS.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Mga Implementation Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/MGA_PHASE1_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Mga Phase1 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/MGA_PHASE2_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Mga Phase2 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/MGA_PHASES_3_4_COMPLETE.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Mga Phases 3 4 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/OVERALL_PROJECT_STATUS.md
Status: [~]
Spec: Status Archive Pre-Phase2/3 Status - Overall Project Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-sessions/DOCUMENTATION_REORGANIZATION_2025_10_23.md
Status: [~]
Spec: Status Archive 2025-10 Sessions - Documentation Reorganization 2025 10 23 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-sessions/SESSION_PROGRESS_2025_10_28_CONDITIONAL_FUNCTIONS.md
Status: [~]
Spec: Status Archive 2025-10 Sessions - Session Progress 2025 10 28 Conditional Functions (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-sessions/SESSION_SUMMARY_2025_10_23_MGA_CATALOG_COMPLIANCE.md
Status: [~]
Spec: Status Archive 2025-10 Sessions - Session Summary 2025 10 23 Mga Catalog Compliance (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-sessions/SESSION_SUMMARY_2025_10_24_CONTEXT_VARIABLES_DESIGN.md
Status: [~]
Spec: Status Archive 2025-10 Sessions - Session Summary 2025 10 24 Context Variables Design (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-sessions/SESSION_SUMMARY_2025_10_28_JSON_FUNCTIONS_PRODUCTION.md
Status: [~]
Spec: Status Archive 2025-10 Sessions - Session Summary 2025 10 28 Json Functions Production (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/ALL_INDEX_WORK_COMPLETE_2025-11-06.md
Status: [~]
Spec: Status Archive 2025-11 Completion - All Index Work Complete 2025-11-06 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/ALPHA_003_AUDIT_FINDINGS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Alpha 003 Audit Findings (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/ALPHA_003_PROGRESS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Alpha 003 Progress (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/ALPHA_ENGINE_READINESS_SUMMARY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Alpha Engine Readiness Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/ALTER_TABLE_COMPLETE_2025-11-07.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Alter Table Complete 2025-11-07 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/BITMAP_INDEX_COMPLETION_REPORT_2025-11-04.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Bitmap Index Completion Report 2025-11-04 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/BITMAP_INDEX_IMPLEMENTATION_SUMMARY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Bitmap Index Implementation Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/BRIN_COMPLETION_REPORT_2025-11-03.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Brin Completion Report 2025-11-03 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/BRIN_COMPLETION_REPORT_2025-11-04.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Brin Completion Report 2025-11-04 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/BUILD_FIXES_2025-11-07.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Build Fixes 2025-11-07 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CATALOG_CORRECTIONS_COMPLETE_2025-11-09.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Catalog Corrections Complete 2025-11-09 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CATALOG_CORRECTIONS_PHASE1-5_COMPLETE_2025-11-09.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Catalog Corrections Phase1-5 Complete 2025-11-09 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CATALOG_CORRECTION_SESSION_2025-11-08.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Catalog Correction Session 2025-11-08 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CHECK_CONSTRAINTS_COMPLETE_2025-11-13.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Check Constraints Complete 2025-11-13 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CHECK_PARSER_COMPLETE_2025-11-13.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Check Parser Complete 2025-11-13 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/COLUMNSTORE_COMPLETION_REPORT_2025-11-03.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Columnstore Completion Report 2025-11-03 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/COLUMNSTORE_IMPLEMENTATION_ROADMAP_2025-11-04.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Columnstore Implementation Roadmap 2025-11-04 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/COLUMNSTORE_PHASE1_RLE_COMPLETION_2025-11-04.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Columnstore Phase1 Rle Completion 2025-11-04 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/COLUMNSTORE_PHASE2_DICT_COMPLETION_2025-11-04.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Columnstore Phase2 Dict Completion 2025-11-04 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/COLUMNSTORE_PHASE3_BITPACK_COMPLETION_2025-11-04.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Columnstore Phase3 Bitpack Completion 2025-11-04 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/COLUMNSTORE_PHASE4_PREDICATE_COMPLETION_2025-11-04.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Columnstore Phase4 Predicate Completion 2025-11-04 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/COLUMNSTORE_PHASES_1-4_SUMMARY_2025-11-04.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Columnstore Phases 1-4 Summary 2025-11-04 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/COMPILATION_SUCCESS_2025-11-06.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Compilation Success 2025-11-06 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CONNECTION_CONTEXT_SECURITY_INTEGRATION_2025-11-10.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Connection Context Security Integration 2025-11-10 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CONSTRAINTS_COMPLETE_2025-11-13.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Constraints Complete 2025-11-13 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CONSTRAINT_ENFORCEMENT_COMPLETE_2025-11-12.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Constraint Enforcement Complete 2025-11-12 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CONSTRAINT_ENFORCEMENT_PHASE1_COMPLETE_2025-11-12.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Constraint Enforcement Phase1 Complete 2025-11-12 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CUSTOM_TABLESPACE_COMPLETION_REPORT_2025-11-06.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Custom Tablespace Completion Report 2025-11-06 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/DATA_TYPE_IMPLEMENTATION_STATUS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Data Type Implementation Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/DDL_COMPLETE_2025-11-07.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Ddl Complete 2025-11-07 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/DDL_COMPLETION_REPORT_2025-11-07.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Ddl Completion Report 2025-11-07 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/DDL_IMPLEMENTATION_STATUS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Ddl Implementation Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/DDL_MODIFICATIONS_COMPLETE_2025-11-07.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Ddl Modifications Complete 2025-11-07 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/DOCUMENTATION_CLEANUP_REPORT_2025-11-03.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Documentation Cleanup Report 2025-11-03 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/DOCUMENTATION_UPDATE_SUMMARY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Documentation Update Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FEATURE_BRANCH_CREATED_2025-11-08.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Feature Branch Created 2025-11-08 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FK_PHASE_A_COMPLETE_2025-11-14.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Fk Phase A Complete 2025-11-14 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FK_PHASE_A_ENFORCEMENT_COMPLETE_2025-11-14.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Fk Phase A Enforcement Complete 2025-11-14 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FK_PHASE_B_COMPLETE_2025-11-14.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Fk Phase B Complete 2025-11-14 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FK_PHASE_C_COMPLETE_2025-11-14.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Fk Phase C Complete 2025-11-14 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FOREIGN_KEY_FRAMEWORK_COMPLETE_2025-11-12.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Foreign Key Framework Complete 2025-11-12 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FTS_COMPLETION_REPORT_2025-11-03.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Fts Completion Report 2025-11-03 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FUNCTIONS_EXPLORATION_SUMMARY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Functions Exploration Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FUNCTION_IMPLEMENTATION_ANALYSIS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Function Implementation Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/GIN_COMPLETION_REPORT_2025-11-03.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Gin Completion Report 2025-11-03 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/GIST_COMPLETION_REPORT_2025-11-03.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Gist Completion Report 2025-11-03 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/GIST_COMPLETION_REPORT_2025-11-04.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Gist Completion Report 2025-11-04 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/HNSW_COMPLETION_REPORT_2025-11-03.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Hnsw Completion Report 2025-11-03 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/HNSW_COMPLETION_REPORT_2025-11-04.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Hnsw Completion Report 2025-11-04 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/HNSW_INDEX_IMPLEMENTATION_SUMMARY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Hnsw Index Implementation Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/IMPLEMENTATION_TIMELINE.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Implementation Timeline (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/INDEX_AUDIT_DETAILED_FINDINGS_20251120.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Index Audit Detailed Findings 20251120 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/INDEX_AUDIT_REPORT_20251120.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Index Audit Report 20251120 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/INDEX_IMPLEMENTATION_STATUS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Index Implementation Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/INDEX_REVIEW_FILES_REFERENCE.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Index Review Files Reference (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/LSM_TREE_COMPLETION_REPORT_2025-11-05.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Lsm Tree Completion Report 2025-11-05 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/MATHEMATICAL_FUNCTIONS_COMPLETE_2025-11-12.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Mathematical Functions Complete 2025-11-12 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/MGA_ALPHA_STATUS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Mga Alpha Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/MIGRATION_SUMMARY_2025_10_03.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Migration Summary 2025 10 03 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/P3_HNSW_DISTANCE_METRICS_COMPLETE_2025-11-06.md
Status: [~]
Spec: Status Archive 2025-11 Completion - P3 Hnsw Distance Metrics Complete 2025-11-06 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE1_TASK1_2_ARCHITECTURAL_NOTE.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Phase1 Task1 2 Architectural Note (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE2_GC_COMPLETION_SUMMARY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Phase2 Gc Completion Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE3_3_1_NEXT_STEPS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Phase3 3 1 Next Steps (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE4_NEW_INDEX_TYPES_DEPENDENCY_ANALYSIS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Phase4 New Index Types Dependency Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE_1.5_GPID_MIGRATION_COMPLETE_2025-11-06.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Phase 1.5 Gpid Migration Complete 2025-11-06 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE_1_5_SUMMARY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Phase 1 5 Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE_1_5_TID_MIGRATION_STATUS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Phase 1 5 Tid Migration Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE_2_COMPLETION_PLAN.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Phase 2 Completion Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE_2_REMAINING_TASKS_ANALYSIS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Phase 2 Remaining Tasks Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE_2_STATUS_OCT_28_2025.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Phase 2 Status Oct 28 2025 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE_3_0_PLANNING_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Phase 3 0 Planning 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PROGRESS_UPDATE_2025-11-06-EVENING.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Progress Update 2025-11-06-Evening (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_IMPLEMENTATION_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Implementation Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_IMPLEMENTATION_PLAN_UPDATE_2025-11-10.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Implementation Plan Update 2025-11-10 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_BYTECODE_COMPLETE_2025-11-10.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase2 Bytecode Complete 2025-11-10 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_COMPLETE_2025-11-10.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase2 Complete 2025-11-10 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase2 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_EXECUTOR_STARTED_2025-11-10.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase2 Executor Started 2025-11-10 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_FINAL_STATUS_2025-11-10.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase2 Final Status 2025-11-10 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_PARSER_COMPLETE_2025-11-10.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase2 Parser Complete 2025-11-10 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_PROGRESS_2025-11-10.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase2 Progress 2025-11-10 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_SESSION_SUMMARY_2025-11-10.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase2 Session Summary 2025-11-10 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_0_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 0 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_1_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 1 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_2_1_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 2 1 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_2_2_ANALYSIS_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 2 2 Analysis 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_2_3_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 2 3 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_2_3_PARTIAL_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 2 3 Partial 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_3_1_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 3 1 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_3_2_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 3 2 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_3_3_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 3 3 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_3_4_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 3 4 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_3_5_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 3 5 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_3_5_PARTIAL_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 3 5 Partial 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_3_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 3 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_1_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 4 1 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_2_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 4 2 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_3_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 4 3 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_4_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 4 4 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_5_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 4 5 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_6_DEFERRED_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 4 6 Deferred 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_6_EXPRESSION_STORAGE_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 4 6 Expression Storage Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_7_RUNTIME_EVALUATION_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 4 7 Runtime Evaluation Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_8_TOAST_PERSISTENCE_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 4 8 Toast Persistence Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 4 Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_5_COMPLETE_2025-11-12.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 5 Complete 2025-11-12 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_SESSION_COMPLETE_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 Session Complete 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_STATUS_2025-11-11_FINAL.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Phase3 Status 2025-11-11 Final (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_SESSION_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Security Session 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SEQUENCES_IMPLEMENTATION_COMPLETE.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Sequences Implementation Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_2025-11-06_COMPILATION_SUCCESS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Session 2025-11-06 Compilation Success (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_2025-11-11_PHASE3_2_1.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Session 2025-11-11 Phase3 2 1 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_2025-11-11_PHASE3_2_3.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Session 2025-11-11 Phase3 2 3 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_2025-11-11_PHASE3_3_COMPLETE.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Session 2025-11-11 Phase3 3 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_2025-11-11_PHASE3_3_PROGRESS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Session 2025-11-11 Phase3 3 Progress (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_2025-11-11_PHASE3_4_PROGRESS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Session 2025-11-11 Phase3 4 Progress (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_2025-11-11_PHASE3_4_STARTED.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Session 2025-11-11 Phase3 4 Started (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_FINAL_2025-11-11.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Session Final 2025-11-11 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_SUMMARY_2025-11-06.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Session Summary 2025-11-06 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_SUMMARY_2025-11-07.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Session Summary 2025-11-07 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_SUMMARY_2025_11_03.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Session Summary 2025 11 03 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_SUMMARY_CHECK_CONSTRAINTS_2025-11-13.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Session Summary Check Constraints 2025-11-13 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SPGIST_COMPLETION_REPORT_2025-11-03.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Spgist Completion Report 2025-11-03 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SPGIST_COMPLETION_REPORT_2025-11-04.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Spgist Completion Report 2025-11-04 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SRID_VALIDATION_CLARIFICATION.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Srid Validation Clarification (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_CURRENT_STATUS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Current Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_EXECUTIVE_SUMMARY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Executive Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_FINAL_SESSION_SUMMARY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Final Session Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_COMPLIANCE_ANALYSIS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Mga Compliance Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_COMPLIANCE_COMPLETE_SUMMARY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Mga Compliance Complete Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_INFRASTRUCTURE_ASSESSMENT.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Mga Infrastructure Assessment (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_PHASE_1_3_ASSESSMENT.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Mga Phase 1 3 Assessment (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_PHASE_2_2_PARTIAL.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Mga Phase 2 2 Partial (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_ROLLBACK_ANALYSIS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Mga Rollback Analysis (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_SESSION_2_SUMMARY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Mga Session 2 Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_SESSION_3_SUMMARY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Mga Session 3 Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_SESSION_SUMMARY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Mga Session Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_PHASE_10_12_COMPLETION_REPORT.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Phase 10 12 Completion Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_PHASE_10_12_STATUS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Phase 10 12 Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_PHASE_1_EVALUATOR_FIXES.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Phase 1 Evaluator Fixes (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_PHASE_1_SERIALIZER_FIXES.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Phase 1 Serializer Fixes (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_QUICK_REFERENCE.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Quick Reference (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_SESSION_REPORT.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 17 Session Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_9_2_RTREE_PLANNER_PROGRESS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 9 2 Rtree Planner Progress (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_9_4_MULTI_GEOMETRY_STATUS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 9 4 Multi Geometry Status (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_9_5_IMPLEMENTATION_GUIDE.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 9 5 Implementation Guide (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_9_5_S3_COMPLETION_REPORT.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 9 5 S3 Completion Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_9_AGENT_STRATEGY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task 9 Agent Strategy (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_DML_1_GIN_INDEX_DML_INTEGRATION.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Task Dml 1 Gin Index Dml Integration (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TRUNCATE_TABLE_ASYNC_COMPLETE.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Truncate Table Async Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/VIEWS_IMPLEMENTATION_COMPLETE.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Views Implementation Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/VIEWS_PARSING_FIX_2025-11-08.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Views Parsing Fix 2025-11-08 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/WAVE_1_COMPLETION_REPORT.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Wave 1 Completion Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/WAVE_1_FINAL_DELIVERY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Wave 1 Final Delivery (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/WAVE_1_SESSION_HANDOFF.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Wave 1 Session Handoff (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/WAVE_2_COMPLETION_SUMMARY.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Wave 2 Completion Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/WAVE_2_PROGRESS_REPORT.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Wave 2 Progress Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/WAVE_3_AGENT_PERMISSIONS.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Wave 3 Agent Permissions (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/WAVE_3_LAUNCH_PLAN.md
Status: [~]
Spec: Status Archive 2025-11 Completion - Wave 3 Launch Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/README.md
Status: [~]
Spec: Status Archive README (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/development/TODO.md
Status: [~]
Spec: Status Archive Development TODO (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/README.md
Status: [~]
Spec: Status Archive Legacy README (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/2025-09-15_btree_analysis_report.md
Status: [~]
Spec: Legacy Progress - 2025 09 15 Btree Analysis Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/COMPRESSION_IMPLEMENTATION_SUMMARY.md
Status: [~]
Spec: Legacy Progress - Compression Implementation Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/HEAP_TOAST_INTEGRATION_COMPLETE.md
Status: [~]
Spec: Legacy Progress - Heap Toast Integration Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/ImplementationAndReviewProcessLog.md
Status: [~]
Spec: Legacy Progress - Implementationandreviewprocesslog (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/LOG_FORMAT.md
Status: [~]
Spec: Legacy Progress - Log Format (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/MERGE_SUMMARY_STAGE_1_1.md
Status: [~]
Spec: Legacy Progress - Merge Summary Stage 1 1 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/PROGRESS_LOG_TEMPLATE.md
Status: [~]
Spec: Legacy Progress - Progress Log Template (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/PROGRESS_TEMPLATE.md
Status: [~]
Spec: Legacy Progress - Progress Template (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/README.md
Status: [~]
Spec: Legacy Progress - Readme (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/STAGE_1_1_COMPLETE_SUMMARY.md
Status: [~]
Spec: Legacy Progress - Stage 1 1 Complete Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/alpha_1_01_1.log.md
Status: [~]
Spec: Legacy Progress - Alpha 1 01 1.Log (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/alpha_1_01_2.log.md
Status: [~]
Spec: Legacy Progress - Alpha 1 01 2.Log (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/alpha_1_03.log.md
Status: [~]
Spec: Legacy Progress - Alpha 1 03.Log (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/alpha_1_03_complete.md
Status: [~]
Spec: Legacy Progress - Alpha 1 03 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/alpha_1_03_storage_engine.log.md
Status: [~]
Spec: Legacy Progress - Alpha 1 03 Storage Engine.Log (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/alpha_1_04_transaction_foundation.log.md
Status: [~]
Spec: Legacy Progress - Alpha 1 04 Transaction Foundation.Log (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/alpha_1_05_complete_summary.md
Status: [~]
Spec: Legacy Progress - Alpha 1 05 Complete Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/alpha_1_05_sql_parser.log.md
Status: [~]
Spec: Legacy Progress - Alpha 1 05 Sql Parser.Log (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/alpha_1_05_week2_summary.md
Status: [~]
Spec: Legacy Progress - Alpha 1 05 Week2 Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/alpha_1_05_week3_summary.md
Status: [~]
Spec: Legacy Progress - Alpha 1 05 Week3 Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/catalog_manager_refactoring_issues_2025-09-08.md
Status: [~]
Spec: Legacy Progress - Catalog Manager Refactoring Issues 2025 09 08 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/deficiency_remediation_2025-09-09.md
Status: [~]
Spec: Legacy Progress - Deficiency Remediation 2025 09 09 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/implementation/AGENT_A_FINAL_FIXES_STAGE_1_1.md
Status: [~]
Spec: Legacy Progress - Agent A Final Fixes Stage 1 1 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/implementation/AGENT_A_FIXES_STAGE_1_1_ISSUES.md
Status: [~]
Spec: Legacy Progress - Agent A Fixes Stage 1 1 Issues (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/implementation/AGENT_B_REVIEW_SUMMARY_STAGE_1_1_PAGE_SIZES.md
Status: [~]
Spec: Legacy Progress - Agent B Review Summary Stage 1 1 Page Sizes (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/implementation/IMPL_PROGRESS_TEMPLATE.md
Status: [~]
Spec: Legacy Progress - Impl Progress Template (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/phases_1_01_to_1_05_complete_review.md
Status: [~]
Spec: Legacy Progress - Phases 1 01 To 1 05 Complete Review (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/stage_1_1_extended_page_sizes.log.md
Status: [~]
Spec: Legacy Progress - Stage 1 1 Extended Page Sizes.Log (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/testing/TEST_PROGRESS_TEMPLATE.md
Status: [~]
Spec: Legacy Progress - Test Progress Template (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/uuid_migration_summary_2025-09-09.md
Status: [~]
Spec: Legacy Progress - Uuid Migration Summary 2025 09 09 (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/AGENT_C_REVIEW_TEST_SUMMARY.md
Status: [~]
Spec: Legacy Tests - Agent C Review Test Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/AGENT_C_TEST_REPORT_FOR_AGENT_A.md
Status: [~]
Spec: Legacy Tests - Agent C Test Report For Agent A (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/COMPREHENSIVE_TEST_REPORT.md
Status: [~]
Spec: Legacy Tests - Comprehensive Test Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/CORRECTED_ALPHA_104_ASSESSMENT.md
Status: [~]
Spec: Legacy Tests - Corrected Alpha 104 Assessment (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/CRITICAL_FIXES_TEST_PLAN.md
Status: [~]
Spec: Legacy Tests - Critical Fixes Test Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/EDGE_CASE_TEST_REPORT.md
Status: [~]
Spec: Legacy Tests - Edge Case Test Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/FINAL_ALPHA_104_ASSESSMENT.md
Status: [~]
Spec: Legacy Tests - Final Alpha 104 Assessment (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/FINAL_TEST_STATUS_REPORT.md
Status: [~]
Spec: Legacy Tests - Final Test Status Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/LEXER_COMPREHENSIVE_TEST_PLAN.md
Status: [~]
Spec: Legacy Tests - Lexer Comprehensive Test Plan (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/LEXER_TEST_SUMMARY.md
Status: [~]
Spec: Legacy Tests - Lexer Test Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/PARSER_TEST_RECONCILIATION.md
Status: [~]
Spec: Legacy Tests - Parser Test Reconciliation (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/PARSER_TEST_SUMMARY.md
Status: [~]
Spec: Legacy Tests - Parser Test Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/README.md
Status: [~]
Spec: Legacy Tests - Readme (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/UNTESTABLE_REQUIREMENTS.md
Status: [~]
Spec: Legacy Tests - Untestable Requirements (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/WEEK3_WEEK4_TEST_SUMMARY.md
Status: [~]
Spec: Legacy Tests - Week3 Week4 Test Summary (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/phase1_session_docs/PHASE_1_5_QUICK_START.md
Status: [~]
Spec: Legacy Phase1 Session - Phase 1 5 Quick Start (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/phase1_session_docs/PHASE_1_5_README.md
Status: [~]
Spec: Legacy Phase1 Session - Phase 1 5 Readme (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/phase1_session_docs/PHASE_1_5_WORK_COMPLETED.md
Status: [~]
Spec: Legacy Phase1 Session - Phase 1 5 Work Completed (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/phase1_standalone_tests/test_aggregation_execution.cpp
Status: [~]
Spec: Legacy Phase1 Standalone Tests - Test Aggregation Execution (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/phase1_standalone_tests/test_join_parsing.cpp
Status: [~]
Spec: Legacy Phase1 Standalone Tests - Test Join Parsing (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/phase1_standalone_tests/test_limit_execution.cpp
Status: [~]
Spec: Legacy Phase1 Standalone Tests - Test Limit Execution (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/phase1_standalone_tests/test_sort_execution.cpp
Status: [~]
Spec: Legacy Phase1 Standalone Tests - Test Sort Execution (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/phase1_standalone_tests/test_update_delete.cpp
Status: [~]
Spec: Legacy Phase1 Standalone Tests - Test Update Delete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/phase1_standalone_tests/test_update_delete_execution.cpp
Status: [~]
Spec: Legacy Phase1 Standalone Tests - Test Update Delete Execution (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/phase1_standalone_tests/test_update_delete_simple.cpp
Status: [~]
Spec: Legacy Phase1 Standalone Tests - Test Update Delete Simple (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_001_COMPLETE.md
Status: [~]
Spec: Phase Completion - Alpha 001 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_002_COMPLETE.md
Status: [~]
Spec: Phase Completion - Alpha 002 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_002_PHASE_1_COMPLETE.md
Status: [~]
Spec: Phase Completion - Alpha 002 Phase 1 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_002_PHASE_2_COMPLETE.md
Status: [~]
Spec: Phase Completion - Alpha 002 Phase 2 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_002_PHASE_3_COMPLETE.md
Status: [~]
Spec: Phase Completion - Alpha 002 Phase 3 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_002_PHASE_4_COMPLETE.md
Status: [~]
Spec: Phase Completion - Alpha 002 Phase 4 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_002_PHASE_5_COMPLETE.md
Status: [~]
Spec: Phase Completion - Alpha 002 Phase 5 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_002_PHASE_6_COMPLETE.md
Status: [~]
Spec: Phase Completion - Alpha 002 Phase 6 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_003_GIN_PHASE_1_COMPLETE.md
Status: [~]
Spec: Phase Completion - Alpha 003 Gin Phase 1 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_003_GIN_PHASE_2_COMPLETE.md
Status: [~]
Spec: Phase Completion - Alpha 003 Gin Phase 2 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_003_GIN_PHASE_3_COMPLETE.md
Status: [~]
Spec: Phase Completion - Alpha 003 Gin Phase 3 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_003_GIN_PHASE_4_COMPLETE.md
Status: [~]
Spec: Phase Completion - Alpha 003 Gin Phase 4 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_003_GIN_PHASE_5_COMPLETE.md
Status: [~]
Spec: Phase Completion - Alpha 003 Gin Phase 5 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_003_GIN_PHASE_6_COMPLETE.md
Status: [~]
Spec: Phase Completion - Alpha 003 Gin Phase 6 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/MGA_COMPLIANCE_COMPLETE.md
Status: [~]
Spec: Phase Completion - Mga Compliance Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE1_STRUCTURE_ANALYSIS_REPORT.md
Status: [~]
Spec: Phase Completion - Phase1 Structure Analysis Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE1_UTF8_UTILS_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase1 Utf8 Utils Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE2_CATALOG_STORAGE_EXPANSION_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase2 Catalog Storage Expansion Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE2_IMPLEMENTATION_STATUS_REPORT.md
Status: [~]
Spec: Phase Completion - Phase2 Implementation Status Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE3_CATALOG_WRITE_LOGIC_FIXES_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase3 Catalog Write Logic Fixes Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE3_STORAGE_ENGINE_INTEGRATION_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase3 Storage Engine Integration Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE4_CATALOG_READ_SAFETY_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase4 Catalog Read Safety Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE4_GARBAGE_COLLECTION_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase4 Garbage Collection Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE5_SQL_UTF8_TESTING_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase5 Sql Utf8 Testing Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE5_TESTING_VALIDATION_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase5 Testing Validation Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE6_DOCUMENTATION_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase6 Documentation Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE6_DOCUMENTATION_OPTIMIZATION_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase6 Documentation Optimization Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_1_INT128_UINT_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase 1 Int128 Uint Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_2_MONEY_TYPE_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase 2 Money Type Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_2_TASK_9_3_GEOMETRY_CONSTRUCTORS_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase 2 Task 9 3 Geometry Constructors Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_2_TASK_9_SPATIAL_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase 2 Task 9 Spatial Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_3_INTERVAL_TYPE_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase 3 Interval Type Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_4_DECIMAL_ARITHMETIC_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase 4 Decimal Arithmetic Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_5_JSONB_TYPE_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase 5 Jsonb Type Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_6_XML_TYPE_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase 6 Xml Type Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_7_VECTOR_TYPE_COMPLETE.md
Status: [~]
Spec: Phase Completion - Phase 7 Vector Type Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/SESSION_SUMMARY_PHASE3_COMPLETE.md
Status: [~]
Spec: Phase Completion - Session Summary Phase3 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/SQL_IDENTIFIER_UTF8_FIX_COMPLETE.md
Status: [~]
Spec: Phase Completion - Sql Identifier Utf8 Fix Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/SQL_SPECIFICATION_IMPLEMENTATION_REPORT.md
Status: [~]
Spec: Phase Completion - Sql Specification Implementation Report (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TABLESPACE_IMPLEMENTATION_COMPLETE.md
Status: [~]
Spec: Phase Completion - Tablespace Implementation Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_12_ARRAY_FUNCTIONS_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 12 Array Functions Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_13_TEXT_SEARCH_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 13 Text Search Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_14_PHASE_1_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 14 Phase 1 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_14_PHASE_2_TEXT_PROCESSING_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 14 Phase 2 Text Processing Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_14_PHASE_3_OPERATORS_FUNCTIONS_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 14 Phase 3 Operators Functions Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_14_PHASE_4_GIN_INTEGRATION_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 14 Phase 4 Gin Integration Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_14_PHASE_5_SQL_INTEGRATION_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 14 Phase 5 Sql Integration Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_15_RANGE_TYPES_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 15 Range Types Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_16_NETWORK_TYPES_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 16 Network Types Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_FOUNDATION_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 17 Foundation Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_MGA_PHASE_1_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 17 Mga Phase 1 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_MGA_PHASE_2_1_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 17 Mga Phase 2 1 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_MGA_PHASE_2_2_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 17 Mga Phase 2 2 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_MGA_PHASE_2_3_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 17 Mga Phase 2 3 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_MGA_PHASE_3_1_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 17 Mga Phase 3 1 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_MGA_PHASE_3_2_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 17 Mga Phase 3 2 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_MGA_PHASE_3_3_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 17 Mga Phase 3 3 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_MGA_PHASE_4_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 17 Mga Phase 4 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_PHASE_6_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 17 Phase 6 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_PHASE_7_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 17 Phase 7 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_PHASE_8_9_MATCHER_FIXES_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 17 Phase 8 9 Matcher Fixes Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_PHASE_8_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 17 Phase 8 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_PHASE_9_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 17 Phase 9 Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_9_2_RTREE_PLANNER_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 9 2 Rtree Planner Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_9_SPATIAL_INTEGRATION_COMPLETE.md
Status: [~]
Spec: Phase Completion - Task 9 Spatial Integration Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TOAST_MGA_COMPLIANCE_COMPLETE.md
Status: [~]
Spec: Phase Completion - Toast Mga Compliance Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TOAST_MGA_PHASE3_ANALYSIS_COMPLETE.md
Status: [~]
Spec: Phase Completion - Toast Mga Phase3 Analysis Complete (archive, non-authoritative)
Tasks:
[*] Authoritative status check: archive/non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.
## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/BACKUP_AND_RESTORE.md
Status: [~]
Spec: Backup and Restore (non-authoritative)
Tasks:
[*] Authoritative status check: not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Findings report only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/BETA_SQL2023_IMPLEMENTATION_SPECIFICATION.md
Status: [~]
Spec: SQL:2023 implementation coverage (authoritative for V3). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_sql2023_implementation_spec_report.md`.
Tasks:
[*] T801 JSON type name recognition and SBDB$ domain presence (lexer + domain manager).
[ ] T801 JSON type options (WITH UNIQUE KEYS, storage params payload) parsing and catalog mapping.
[ ] T801 JSON literal syntax (JSON '...' / JSON('...')) in v3 parser.
[ ] T801 IS JSON predicate parsing + executor semantics.
[~] T801 JSONB storage layout per spec (current JSONB uses CBOR canonicalization, not spec payload).
[ ] T802 WITH UNIQUE KEYS parsing and enforcement.
[~] T803 String-based JSON compatibility (JSON stored as string in TypedValue; no SQL:2023 behavior verification).
[ ] T840 Hex literals in SQL/JSON path.
[ ] T860-T863 Simplified accessors (dot, subscript, chaining, NULL handling) in v3 parser/executor.
[ ] T864 Array slicing rejected in V3 but requires ERR_FEATURE_DISABLED on use.
[ ] T865-T878 JSON item methods (.string(), .number(), .boolean(), .date(), .time(), .timestamp(), .bigint(), .integer(), .double()).
[ ] T879-T882 JSON comparison operators (JSON/JSONB comparisons).
[~] T661 Non-decimal literals: hex/binary supported; octal missing.
[ ] T662 Underscore digit separators in numeric literals.
[ ] F401 UNIQUE NULLS DISTINCT/NOT DISTINCT parsing and enforcement.
[ ] T056 Multi-character TRIM (leading/trailing/both, multi-char).
[ ] F868 TABLE keyword feature rejected in V3 but requires ERR_FEATURE_DISABLED on use.
[ ] SQL/PGQ rejected in V3 but requires ERR_FEATURE_DISABLED on use.
[ ] SQL/MDA rejected in V3 but requires ERR_FEATURE_DISABLED on use.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/BETA_SQL_STANDARD_COMPLIANCE_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (planning only). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_sql_standard_compliance_spec_report.md`.
Tasks:
[ ] GROUPING SETS / CUBE / ROLLUP (not verified; non-authoritative).
[ ] Full-Text Search SQL syntax (not verified; non-authoritative).
[ ] DEFERRABLE constraints (not verified; non-authoritative).
[ ] MATCH_RECOGNIZE pattern matching (not verified; non-authoritative).
[ ] EXCLUSION constraints (not verified; non-authoritative).
[ ] CHECK constraints with subqueries (not verified; non-authoritative).
[ ] Polymorphic table functions (not verified; non-authoritative).
[ ] Testing requirements (not verified; non-authoritative).
[ ] Documentation requirements (not verified; non-authoritative).
[ ] Migration and compatibility (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/README.md
Status: [~]
Spec: Non-authoritative README reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_cluster_spec_readme_report.md`.
Tasks:
[ ] SBCLUSTER-SUMMARY.md (not verified here).
[ ] SBCLUSTER-00-GUIDING-PRINCIPLES.md (not verified here).
[ ] SBCLUSTER-01-CLUSTER-CONFIG-EPOCH.md (not verified here).
[ ] SBCLUSTER-02-MEMBERSHIP-AND-IDENTITY.md (not verified here).
[ ] SBCLUSTER-03-CA-POLICY.md (not verified here).
[ ] SBCLUSTER-04-SECURITY-BUNDLE.md (not verified here).
[ ] SBCLUSTER-05-SHARDING.md (not verified here).
[ ] SBCLUSTER-06-DISTRIBUTED-QUERY.md (not verified here).
[ ] SBCLUSTER-07-REPLICATION.md (not verified here).
[ ] SBCLUSTER-08-BACKUP-AND-RESTORE.md (not verified here).
[ ] SBCLUSTER-09-SCHEDULER.md (not verified here).
[ ] SBCLUSTER-10-OBSERVABILITY.md (not verified here).
[ ] SBCLUSTER-11-SHARD-MIGRATION-AND-REBALANCING.md (not verified here).
[ ] SBCLUSTER-12-AUTOSCALING_AND_ELASTIC_LIFECYCLE.md (not verified here).
[ ] SBCLUSTER-IMPLEMENTATION-BOUNDARY.md (not verified here).
[ ] SBCLUSTER-AI-HANDOFF.md (not verified here).
[ ] SBCLUSTER-NORMATIVE-LANGUAGE.md (not verified here).
[ ] SBCLUSTER-THREAT-MODEL.md (not verified here).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-00-GUIDING-PRINCIPLES.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_00_guiding_principles_report.md`.
Tasks:
[ ] Engine authority (not verified; non-authoritative).
[ ] Shard-local MVCC (not verified; non-authoritative).
[ ] No cross-shard transactions (not verified; non-authoritative).
[ ] Push compute to data (not verified; non-authoritative).
[ ] Identical security config (not verified; non-authoritative).
[ ] One-way upgrades (not verified; non-authoritative).
[ ] Trust boundary enforcement (not verified; non-authoritative).
[ ] Immutable audit chain (not verified; non-authoritative).
[ ] Consensus over configuration (not verified; non-authoritative).
[ ] Observability by design (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-01-CLUSTER-CONFIG-EPOCH.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_01_cluster_config_epoch_report.md`.
Tasks:
[ ] Epoch record structure and hash chaining (not verified; non-authoritative).
[ ] Epoch lifecycle states (not verified; non-authoritative).
[ ] CCE validation and error handling (not verified; non-authoritative).
[ ] Raft log publication of epochs (not verified; non-authoritative).
[ ] Configuration bundle signing/verification (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-02-MEMBERSHIP-AND-IDENTITY.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_02_membership_identity_report.md`.
Tasks:
[ ] Node identity structure (not verified; non-authoritative).
[ ] mTLS authentication requirements (not verified; non-authoritative).
[ ] Role-based node authorization (not verified; non-authoritative).
[ ] Membership lifecycle (not verified; non-authoritative).
[ ] Peer observation protocol (not verified; non-authoritative).
[ ] Health/liveness tracking (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-03-CA-POLICY.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_03_ca_policy_report.md`.
Tasks:
[ ] CA policy bundle structure/versioning (not verified; non-authoritative).
[ ] Overlap rotation model (not verified; non-authoritative).
[ ] Peer observation during CA rotation (not verified; non-authoritative).
[ ] Emergency CA revocation (not verified; non-authoritative).
[ ] Certificate validation rules (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-04-SECURITY-BUNDLE.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_04_security_bundle_report.md`.
Tasks:
[ ] Security bundle structure/serialization (not verified; non-authoritative).
[ ] Raft distribution and convergence (not verified; non-authoritative).
[ ] Enforcement gates for distributed ops (not verified; non-authoritative).
[ ] Bundle hash/version handling (not verified; non-authoritative).
[ ] Disaster recovery/rollback (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-05-SHARDING.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_05_sharding_report.md`.
Tasks:
[ ] Consistent hash sharding strategy (not verified; non-authoritative).
[ ] Shard map structure/versioning (not verified; non-authoritative).
[ ] Virtual node distribution (not verified; non-authoritative).
[ ] Shard routing/ownership (not verified; non-authoritative).
[ ] Resharding/rebalancing procedures (not verified; non-authoritative).
[ ] Cross-shard query limitations (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-06-DISTRIBUTED-QUERY.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_06_distributed_query_report.md`.
Tasks:
[ ] Coordinator/subplan model (not verified; non-authoritative).
[ ] Push-compute-to-data execution (not verified; non-authoritative).
[ ] Result merge/aggregation (not verified; non-authoritative).
[ ] Security bundle enforcement gates (not verified; non-authoritative).
[ ] Shard-local snapshot consistency (not verified; non-authoritative).
[ ] Read-only distributed query restriction (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-07-REPLICATION.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_07_replication_report.md`.
Tasks:
[ ] Per-shard replication model (not verified; non-authoritative).
[ ] WAL/logical replication stream (not verified; non-authoritative).
[ ] Physical shadow replication (not verified; non-authoritative).
[ ] Failover/promotion/fencing (not verified; non-authoritative).
[ ] Replication lag monitoring (not verified; non-authoritative).
[ ] Consistency trade-offs (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-08-BACKUP-AND-RESTORE.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_08_backup_restore_report.md`.
Tasks:
[ ] Per-shard backup procedures (not verified; non-authoritative).
[ ] Cluster-consistent backup sets (not verified; non-authoritative).
[ ] Full/incremental backup mechanics (not verified; non-authoritative).
[ ] Restore procedures and validation (not verified; non-authoritative).
[ ] Trust boundary enforcement (not verified; non-authoritative).
[ ] Backup encryption/retention policies (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-09-SCHEDULER.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_09_scheduler_report.md`.
Tasks:
[ ] Distributed scheduler control plane/agents (not verified; non-authoritative).
[ ] Job classes and partition rules (not verified; non-authoritative).
[ ] Cron/recurring jobs (not verified; non-authoritative).
[ ] Failure handling/retries (not verified; non-authoritative).
[ ] Audit logging of job execution (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-10-OBSERVABILITY.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_10_observability_report.md`.
Tasks:
[ ] OpenTelemetry metrics/tracing (not verified; non-authoritative).
[ ] Prometheus metrics exposure (not verified; non-authoritative).
[ ] Cryptographic audit chain (not verified; non-authoritative).
[ ] Cluster health/churn metrics (not verified; non-authoritative).
[ ] Alerting/anomaly detection (not verified; non-authoritative).
[ ] Observability API (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-11-SHARD-MIGRATION-AND-REBALANCING.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_11_shard_migration_rebalancing_report.md`.
Tasks:
[ ] Shard migration protocol (not verified; non-authoritative).
[ ] Shard split/merge workflows (not verified; non-authoritative).
[ ] Rebalancing on topology changes (not verified; non-authoritative).
[ ] Versioned routing/session pinning (not verified; non-authoritative).
[ ] Rollback semantics (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-12-AUTOSCALING_AND_ELASTIC_LIFECYCLE.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_12_autoscaling_elastic_lifecycle_report.md`.
Tasks:
[ ] Autoscaling policies/decision flow (not verified; non-authoritative).
[ ] Node lifecycle state machine (not verified; non-authoritative).
[ ] Membership/shard migration integration (not verified; non-authoritative).
[ ] Metrics/thresholds for scaling (not verified; non-authoritative).
[ ] Safety gates/invariants (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-AI-HANDOFF.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_ai_handoff_report.md`.
Tasks:
[ ] Engine authority principle (not verified; non-authoritative).
[ ] Shard-local MVCC / no cross-shard transactions (not verified; non-authoritative).
[ ] CCE monotonicity enforcement (not verified; non-authoritative).
[ ] Trust boundary for backup/restore (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-IMPLEMENTATION-BOUNDARY.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_implementation_boundary_report.md`.
Tasks:
[ ] Alpha/Beta/GA milestone definitions (not verified; non-authoritative).
[ ] Feature matrix and conformance criteria (not verified; non-authoritative).
[ ] Dependency graph/testing requirements (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-NORMATIVE-LANGUAGE.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_normative_language_report.md`.
Tasks:
[ ] RFC 2119 keyword definitions (not verified; non-authoritative).
[ ] Requirement level interpretation rules (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-SUMMARY.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_summary_report.md`.
Tasks:
[ ] Cluster architecture summary (not verified; non-authoritative).
[ ] Spec suite navigation/reading order (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-THREAT-MODEL.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbcluster_threat_model_report.md`.
Tasks:
[ ] Attacker models and capabilities (not verified; non-authoritative).
[ ] Threat scenarios by surface (not verified; non-authoritative).
[ ] Mitigation mapping to specs (not verified; non-authoritative).
[ ] Residual risks/assumptions (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/sbsec_handoff_summary.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sbsec_handoff_summary_report.md`.
Tasks:
[ ] Security architecture summary/invariants (not verified; non-authoritative).
[ ] Engine as sole authority model (not verified; non-authoritative).
[ ] Execution pipeline security steps (not verified; non-authoritative).
[ ] Security context model and hooks (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/DATABASE_REGISTRY_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_database_registry_spec_report.md`.
Tasks:
[ ] Registry schema tables/views (not verified; non-authoritative).
[ ] Registry operations (not verified; non-authoritative).
[ ] DatabaseRegistry C++ API and IPC messages (not verified; non-authoritative).
[ ] Server startup/SQL integration (not verified; non-authoritative).
[ ] Migration from single-database mode (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/DATABASE_REGISTRY_SPECIFICATION_CORRECTED.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_database_registry_spec_corrected_report.md`.
Tasks:
[ ] Self-hosted registry database approach (not verified; non-authoritative).
[ ] Registry schema and indexes (not verified; non-authoritative).
[ ] Permission/statistics tables (not verified; non-authoritative).
[ ] Registry operations/integration (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/DDL_ALTER.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_ddl_alter_report.md`.
Tasks:
[ ] Implement required `SBLR3_DDL_ALTER` + `SBLR3_DDL_ALTER_ACTION` emission (currently specialized opcodes).
[~] ALTER TABLE actions (most supported; RESTRICT not explicit; emissions via `SBLR3_ALTER_TABLE`).
[ ] ALTER DATABASE SET OPTION_KV / SET DEFAULT TABLESPACE.
[ ] ALTER SCHEMA SET AUTHORIZATION.
[ ] ALTER INDEX actions beyond limited SET OPTIONS (RESET, TABLESPACE, STORAGE PARAMS, STATS, REBUILD).
[ ] ALTER VIEW SET OPTION.
[ ] ALTER SEQUENCE full option support (increment/min/max/start/cache/cycle).
[ ] ALTER DOMAIN SET/DROP NOT NULL.
[ ] ALTER TYPE per spec (attributes) instead of enum/range-only options.
[ ] ALTER FUNCTION/PROCEDURE/Package (definer/security, cost/rows, replace body).
[ ] ALTER TRIGGER (enable/disable/order/table).
[ ] ALTER POLICY rename/enable/disable.
[ ] ALTER TABLESPACE SET LOCATION/SET OPTION.
[ ] ALTER ROLE/USER/GROUP set password/options/default role/login.
[ ] ALTER SERVER/FOREIGN TABLE/USER MAPPING set options/owner.
[ ] ALTER SYNONYM SET TARGET.
[ ] ALTER JOB SET OWNER/RENAME TO.
[ ] ALTER SYSTEM RESET option.
[ ] Error code mapping to ERR_DDL_UNSUPPORTED_OBJECT/ACTION/PERMISSION.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/DDL_CREATE.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_ddl_create_report.md`.
Tasks:
[ ] Implement required `SBLR3_DDL_CREATE` emission for all CREATE statements.
[~] CREATE DATABASE/SCHEMA/TABLE/INDEX/VIEW/SEQUENCE/DOMAIN/TYPE/FUNCTION/PROCEDURE/PACKAGE/TRIGGER/POLICY/TABLESPACE/ROLE/USER/GROUP/SERVER/FOREIGN TABLE/USER MAPPING/SYNONYM/JOB/EXCEPTION (parsed and emitted via specialized opcodes; missing DDL_CREATE wrapper).
[ ] CREATE EXTENSION (not parsed/emitted).
[ ] Apply CREATE OR REPLACE flag in payload where supported.
[ ] Error code mapping to ERR_DDL_UNSUPPORTED_OBJECT/OPTION/ERR_OBJECT_EXISTS.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/DDL_DROP_TRUNCATE.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_ddl_drop_truncate_report.md`.
Tasks:
[ ] Implement `SBLR3_DDL_DROP` and `SBLR3_DDL_TRUNCATE` emission (currently specialized opcodes).
[ ] Emit all objects in multi-drop statements (currently only first object emitted).
[ ] Implement DROP USER in v3 parser.
[ ] Implement DROP EXTENSION.
[ ] Emit TRUNCATE flags for restart_identity and cascade.
[ ] Align CASCADE/RESTRICT behavior and error codes (ERR_DDL_UNSUPPORTED_OBJECT, ERR_DEPENDENCY_EXISTS, ERR_INVALID_OPERATION).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/DELETE.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_delete_report.md`.
Tasks:
[~] Parse DELETE target, USING, WHERE, RETURNING (present).
[ ] Emit `SBLR3_DML_DELETE` opcode and typed payload (currently `SBLR3_DELETE`).
[ ] Map required errors: ERR_PARSE_EXPECTED_TABLE, ERR_FEATURE_NOT_SUPPORTED (RETURNING), ERR_PERMISSION_DENIED.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/DRIVER_CONFORMANCE_TEST_HARNESS.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_driver_conformance_test_harness_report.md`.
Tasks:
[ ] Test manifest schema and test kinds (not verified; non-authoritative).
[ ] Feature gating via SB_CONFORMANCE_FEATURES (not verified; non-authoritative).
[ ] Reference runner and adapter contract (not verified; non-authoritative).
[ ] Result JSON format (not verified; non-authoritative).
[ ] Fixture SQL and required tests (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/DRIVER_STREAMING_AND_PAGING.md
Status: [~]
Spec: Non-authoritative reference (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_driver_streaming_and_paging_report.md`.
Tasks:
[ ] Binary-only streaming requirement (not verified; non-authoritative).
[ ] SBWP portal paging (not verified; non-authoritative).
[ ] LOB streaming frame handling (not verified; non-authoritative).
[ ] SQLSTATE codes for streaming/paging (not verified; non-authoritative).
[ ] Per-language streaming mappings (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_executor_lock_gc_constraint_matrix_report.md`.
Tasks:
[ ] Enforce/verify global lock ordering and deadlock handling per spec.
[ ] Implement SBX-* error code mapping (SBX-CONSTRAINT-*, SBX-LOCK-*, SBX-TXN-*, SBX-OBJ-*, SBX-TYPE-*).
[ ] Verify constraint enforcement order (domain -> not null -> column check -> table check -> FK -> unique/PK).
[ ] Verify MGA visibility/GC behavior per spec.
[ ] Verify per-opcode lock/constraint behavior for SELECT/INSERT/UPDATE/DELETE/MERGE/COPY/DDL/PSQL.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/EXECUTOR_V3_SBLR.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_executor_v3_sblr_report.md`.
Tasks:
[~] Input contract: V3 container decoding and execution path (present).
[~] Enforce V3 validation rules within executor path (validation exists but enforced in server/protocol adapters).
[ ] Resolve constant pools and symbol tables per `SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md` (decoded but unused).
[~] Execute opcodes in order with stack-based VM (sequential execution present; no explicit V3 stack VM).
[ ] Enforce opcode semantics per `SBLR_V3_OPCODE_SEMANTICS.md` (no semantics validation observed).
[ ] Enforce lock/GC/constraint rules per `EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`.
[ ] Map validation failure to `SBX-INVALID-BYTECODE` (currently SQLSTATE `0A000`).
[ ] Map unsupported opcode to `SBX-UNSUPPORTED-OPCODE` (currently generic execution error, SQLSTATE `42000`).
[ ] Map runtime constraint violations to SQLSTATE per opcode semantics (currently generic SQLSTATE `42000`).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_executor_v3_sql_engine_report.md`.
Tasks:
[~] Execution pipeline: parse/emit/execute V3 SBLR (present); binding and plan stages not verified.
[ ] Logical/physical plan pipeline for V3 not evidenced in parser/emitter.
[~] SELECT/INSERT/UPDATE/DELETE/MERGE V3 opcode handling present; full semantics not verified.
[ ] Join type semantics for V3 (INNER/LEFT/RIGHT/FULL/CROSS/SEMI/ANTI) not verified.
[~] Aggregation support via `SBLR3_AGG_*` opcodes; ordered/DISTINCT aggregates not verified.
[ ] Window functions via `SBLR3_WIN_*` opcodes not implemented in V3 executor path.
[~] Transaction opcode handling present (BEGIN/COMMIT/ROLLBACK/SAVEPOINT); MVCC default isolation not verified.
[ ] Domain constraint enforcement order for V3 DML not verified.
[ ] UUID v7 catalog ID binding at compile time not verified.
[~] Utility/session statements (COPY/SET/SHOW/PSQL) partially handled; full coverage not verified.
[ ] Required end-to-end SQL engine tests not identified/verified.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (explicitly marked; not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_firebird_v2_feature_parity_spec_report.md`.
Tasks:
[ ] Parity requirements for Firebird/V2 parser features (not verified; non-authoritative).
[ ] MGA/lock/GC references listed (not verified; non-authoritative).
[ ] Context variables and RDB$ context functions (not verified; non-authoritative).
[ ] Missing DDL/DML/PSQL features (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/IMPLEMENTATION_SAFETY_SUMMARY.md
Status: [~]
Spec: Authoritative checklist/index. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_implementation_safety_summary_report.md`.
Tasks:
[*] Authoritative status confirmed in inventory.
[~] Checklist-only; no direct code verification (referenced specs to be verified separately).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/IMPLEMENTATION_STANDARDS.md
Status: [~]
Spec: Non-authoritative reference (explicitly marked; not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_implementation_standards_report.md`.
Tasks:
[ ] Process and quality requirements (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/IMPLEMENTATION_STATUS_DASHBOARD.md
Status: [~]
Spec: Non-authoritative placeholder (not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_implementation_status_dashboard_report.md`.
Tasks:
[ ] Placeholder only; no verification (non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/INSERT.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_insert_report.md`.
Tasks:
[~] Parse INSERT target/columns/source (VALUES/SELECT/DEFAULT), ON CONFLICT, RETURNING (present).
[ ] Emit `SBLR3_DML_INSERT` opcode and typed payload (currently `SBLR3_INSERT`).
[ ] Map required errors: `ERR_PARSE_EXPECTED_TABLE`, `ERR_COLUMN_COUNT_MISMATCH`, `ERR_FEATURE_NOT_SUPPORTED` (no spec error codes in V3 path).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/INSTALLATION_AND_INITIALIZATION_SPECIFICATION.md
Status: [~]
Spec: Installation and Initialization Specification (non-authoritative; not in AUTHORITATIVE_SPEC_INVENTORY). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_installation_and_initialization_spec_report.md`.
Tasks:
[~] Authoritative status check - file header says non-authoritative; not in `docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`
[ ] Linux package install: preinst checks, postinst directories/permissions/logrotate
[ ] Windows MSI (WiX) directory/components for install
[ ] TLS self-signed certificate generation script (installer)
[ ] `sb_security` certificate management tool (generate/info/renew/client)
[ ] Default `sb_server.conf` template with server/logging/network/ssl/auth/memory/registry sections
[ ] Config validation: path existence, cert/key match, port availability, memory checks
[ ] Database registry initialization (SQLite + schema)
[ ] Security database initialization (users/roles/role hierarchy)
[ ] `sb_setup` interactive wizard
[ ] `sb_setup` non-interactive JSON/template apply
[ ] Post-install verification script (dirs, binaries, certs, registry, security DB, `sb_server --check`)
[ ] Health check endpoint (registry, security DB, listeners, disk, memory)
[ ] Full install/init procedures and TLS generation steps (non-authoritative; not verified).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/JOINS.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_joins_report.md`.
Tasks:
[~] Parse join types + ON/USING; comma joins -> CROSS (present).
[ ] Represent LATERAL in AST (no LATERAL type; parsed as SUBQUERY).
[ ] AST fields for NATURAL/USING semantics per spec (no `is_natural`, limited metadata).
[ ] Emit `SBLR3_TABLE_REF`, `SBLR3_JOIN_TYPE`, `SBLR3_JOIN_CONDITION`, `SBLR3_JOIN_USING` (currently embedded in SELECT payload).
[ ] Emit/implement join algorithms via `SBLR3_HASH_JOIN`/`SBLR3_NESTED_LOOP_JOIN`.
[ ] Implement V3 executor join semantics for INNER/LEFT/RIGHT/FULL/CROSS + NATURAL/USING expansion.
[ ] Implement LATERAL semantics in V3 executor.
[ ] Add SQLSTATE mapping for join/table/function errors (42601/42703/42883/42P01).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/MEMORY_MANAGEMENT.md
Status: [~]
Spec: Non-authoritative reference (explicitly marked; not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_memory_management_report.md`.
Tasks:
[ ] Memory architecture/context/buffer pool/shared memory/caches/limits (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/MERGE.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_merge_report.md`.
Tasks:
[ ] Enter DML parse mode for MERGE (no ParseMode guard in `parseMerge`).
[~] Parse target/source/ON/WHEN clauses (present, including NOT MATCHED BY SOURCE).
[ ] Emit `SBLR3_DML_MERGE` opcode with typed payload (currently `SBLR3_MERGE_START`).
[ ] Map required errors: `ERR_PARSE_EXPECTED_ON`, `ERR_PARSE_EXPECTED_WHEN`, `ERR_FEATURE_NOT_SUPPORTED`.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/MGA_RULES.md
Status: [~]
Spec: Non-authoritative reference (explicitly marked; not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_mga_rules_report.md`.
Tasks:
[ ] MGA rules (TIP, OIT/OAT/OST, back-versioning, etc.) not verified here (non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/MYSQL_PARSER_IMPLEMENTATION_GAPS.md
Status: [~]
Spec: MySQL Parser Implementation Gaps (non-authoritative; historical). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_mysql_parser_implementation_gaps_report.md`.
Tasks:
[~] Authoritative status check - non-authoritative; not in inventory (no code verification).
[ ] MySQL emulated parser gap list/remediation (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/OFFICIAL_ROADMAP.md
Status: [~]
Spec: Non-authoritative reference (explicitly marked; not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_official_roadmap_report.md`.
Tasks:
[ ] Roadmap/status only; no verification (non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/PARSER_AMBIGUITY_RESOLUTION.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_ambiguity_resolution_report.md`.
Tasks:
[~] Operator precedence mostly implemented; comparisons vs IN/BETWEEN/LIKE ordering diverges from spec.
[ ] Set operator precedence (INTERSECT > UNION > EXCEPT) not verified.
[~] JOIN binding left-associative; CROSS/NATURAL precedence not explicitly enforced.
[ ] CREATE TABLE vs CTAS and WITH disambiguation rules not verified.
[~] ORDER BY numeric positions implemented with bounds checks.
[~] DEFAULT/NULL emission: DEFAULT -> `SBLR3_DEFAULT_VALUE`, NULL -> literal null.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/PARSER_REMAPPING_AND_IMPLEMENTATION_STRATEGY.md
Status: [~]
Spec: Non-authoritative reference (explicitly marked; not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_remapping_and_implementation_strategy_report.md`.
Tasks:
[ ] Planning guidance for emulated parser feature handling (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/PARSER_TO_SBLR_EMISSION_RULES.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_to_sblr_emission_rules_report.md`.
Tasks:
[ ] Identifier canonicalization: lowercase folding + >128 byte rejection (`V3E-0091`) not implemented.
[ ] DISTINCT+ALL mutual exclusion rejection (`V3E-0070`) not implemented.
[ ] SELECT * expansion into explicit column refs (including JOIN USING coalesce) not implemented.
[~] ORDER BY numeric positions resolved (parser-time; spec says emit-time).
[ ] ORDER BY alias resolution to select-item expression not implemented.
[~] DEFAULT in VALUES emits `SBLR3_DEFAULT_VALUE` (implemented).
[~] INSERT DEFAULT VALUES emits source=DEFAULT (implemented).
[ ] MERGE must emit `SBLR3_MERGE` only (currently `SBLR3_MERGE_START`).
[ ] Remaining edge-case sections (CREATE TABLE/INDEX/ALTER/constraints/PSQL/dialect separation) not verified.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/PERFORMANCE_BENCHMARKS.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_performance_benchmarks_report.md`.
Tasks:
[ ] sbbench harness entry point + mandatory flags not verified.
[ ] JSON Lines output schema not verified.
[ ] Dataset definitions + deterministic seed handling not verified.
[ ] CI regression gate enforcement not verified.
[ ] WAL config rejection (`ERR_FEATURE_DISABLED`) not verified.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md
Status: [~]
Spec: Non-authoritative reference (explicitly marked; not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_postgresql_parser_implementation_gaps_report.md`.
Tasks:
[ ] PostgreSQL emulated parser gap list/remediation (not verified; non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/PROJECT_CONTEXT.md
Status: [~]
Spec: Non-authoritative reference (explicitly marked; not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_project_context_report.md`.
Tasks:
[ ] Context/roadmap only; no verification (non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/PSQL_RUNTIME_V3.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_psql_runtime_v3_report.md`.
Tasks:
[~] Variable frames and nested lookup implemented; shadowing allowed but not required to be explicit.
[ ] Enforce DECLARE-before-executable rule in blocks (not implemented).
[ ] Implement loop-scope variable frames; current loops do not push/pop frames.
[ ] Implement handler-scope locals; current handler frame only stores SQLSTATE/SQLERRM.
[~] ASSIGN performs type coercion and domain validation (implemented).
[ ] Sub-savepoint per PSQL statement and rollback-on-exception not implemented.
[ ] Cursor close-on-closed should be no-op unless strict; current behavior errors.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/PSQL_STATEMENTS.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_psql_statements_report.md`.
Tasks:
[~] Dispatch order mostly aligned (control flow → cursor → exception → assignment/call → DML).
[ ] TRY/EXCEPT dispatch not observed in `parsePSQLStatement`.
[ ] Assignment accepts `=` as well as `:=` (spec lists `SET` / `:=`).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/README.md
Status: [~]
Spec: Non-authoritative reference (explicitly marked; not in authoritative inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_readme_report.md`.
Tasks:
[ ] Scope guidance only; no verification (non-authoritative).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_BYTECODE_CANONICALIZATION.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_v3_bytecode_canonicalization_report.md`.
Tasks:
[ ] Identifier folding to lowercase + NFC not implemented in parser/emitter.
[~] Canonicalization helpers for symbols/constants exist but are unused.
[ ] Validator does not enforce canonicalization rules (V3E-0100..0105).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_BYTECODE_CONTAINER.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_v3_bytecode_container_report.md`.
Tasks:
[~] Container encoding/decoding exists (basic layout + required sections).
[ ] Validate header version_major==3 and container_size matches file length.
[ ] Validate section offsets are aligned, ordered, and non-overlapping.
[ ] Set header flags for optional sections; enforce flag/section consistency.
[ ] INTEGRITY verification not implemented.
[ ] Preserve unknown sections on rewrite (currently dropped).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_v3_constant_pool_and_symbols_report.md`.
Tasks:
[ ] Required symbol pooling (identifiers, names, labels, etc.) not implemented; strings emitted inline.
[ ] Symbol ordering/canonicalization not enforced (helpers unused).
[ ] Required constant pooling not implemented; literals emitted inline.
[ ] UUID catalog IDs not pooled as constants.
[ ] Deterministic hashing rules not implemented (no canonical module hash path).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_OLD_TO_NEW_MAPPING.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_v3_old_to_new_mapping_report.md`.
Tasks:
[~] V3 opcode registry exists; mapping table not fully verified against emission paths.
[ ] Audit emission paths for legacy opcode usage vs mapped V3 opcodes.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_v3_opcode_payloads_report.md`.
Tasks:
[*] Global encoding rules (string/bytes/varuint/schema_path/opt/list) implemented in `src/sblr/v3_codec.cpp`.
[ ] TYPE_SPEC type-specific payload rules not enforced; serialized as opaque bytes only.
[ ] COLUMN_DEF missing repeated `check_expr` list; schema only has `check_count` and emitter stores first check only.
[ ] SCHEMA_EXPR_CASE and SCHEMA_PSQL_CASE do not encode repeated WHEN/THEN pairs; schema uses single fields + when_count.
[ ] SCHEMA_WINDOW_SPEC alias missing from schema registry; `SBLR3_WINDOW_SPEC`/`SBLR3_WINDOW` schema lookup fails.
[ ] Literal DATE schema uses i64 instead of i32; complex literal canonical encodings not verified.
[ ] Opcode naming mismatches vs spec (CASE_WHEN, IN_LIST/SUBQUERY_IN, EXPR_LIKE/ILIKE, *_STMT DDL opcodes).
[ ] BETWEEN is lowered to comparisons, not emitted as SBLR3_BETWEEN/NOT_BETWEEN payload.
[ ] SBLR3_SHOW_* opcodes emitted with SET/SHOW/RESET payload but lack schema mapping, risking encode failures.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_OPCODE_SEMANTICS.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_v3_opcode_semantics_report.md`.
Tasks:
[ ] Lock ordering normative sequence not enforced in V3 executor; semantics only flag `requires_lock_order`.
[~] CONTROL semantics partially enforced by validator (VERSION first, END last, payload size checks).
[ ] Per-opcode stack_in/stack_out and error classes not implemented; `getOpcodeSemantics()` only sets coarse flags.
[ ] DDL/DML/TXN/DCL/SESSION per-opcode semantics (constraint order, dependency checks, SQLSTATE categories) not encoded in V3 executor or validator.
[ ] QUERY/EXPR/FUNC/AGG/WINDOW opcodes not executed as discrete semantics; executor interprets `SBLR3_SELECT` payloads instead.
[ ] TYPE/LITERAL opcode semantics (type stack, literal decoding) not implemented as spec describes.
[ ] INDEX/ARRAY/TEXTSEARCH/SPATIAL/JOB opcode semantics not implemented in V3 executor.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_v3_opcode_spec_report.md`.
Tasks:
[*] Instruction header encoding matches spec (u16/u16/u32 little-endian).
[~] VERSION first/END last ordering enforced; END payload_len=0 not enforced.
[ ] Flags must be zero unless specified; no validation.
[ ] EXTENDED opcode payload format not enforced.
[ ] `varint` (ZigZag) not implemented in codec.
[ ] `string_id` primitive not represented in schema field types.
[ ] Opcode registry missing `SBLR3_SET`/`SHOW`/`RESET`/`RESET_*`/`SET_TIME_ZONE` despite payload map references.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_VALIDATION_RULES.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_v3_validation_rules_report.md`.
Tasks:
[~] VERSION-first/END-last enforced; unknown opcodes rejected; extended opcode envelope rules not enforced.
[ ] Container validation rules not fully enforced (only alignment + non-empty stream).
[ ] Canonicalization and symbol/constant pool rules not enforced.
[ ] No deterministic V3E error codes or opcode offset reporting.
[ ] No max_stack_depth validation.
[ ] Limited expression stack checks only; no full stack_in/stack_out validation.
[ ] No type stack discipline for TYPE/LITERAL/CAST.
[ ] No operand type validation.
[ ] No opcode ordering enforcement (DDL/DML/PSQL/query sequencing).
[ ] No string_id/const_id range checks; inline constants allowed.
[ ] Literal bounds/timezone/range/array/tsvector validation not implemented.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SCRATCHBIRD_ARCHITECTURE_OVERVIEW.md
Status: [~]
Spec: Non-authoritative (conflicting internal \"Authoritative\" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_scratchbird_architecture_overview_report.md`.
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md` (internal label conflicts).
[~] Architecture overview only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SCRATCHBIRD_CONNECTION_RECOVERY_MODEL.md
Status: [~]
Spec: Non-authoritative (conflicting internal \"Authoritative\" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_scratchbird_connection_recovery_model_report.md`.
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md` (internal label conflicts).
[~] Connection recovery overview only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SCRATCHBIRD_EMBEDDED_MODE_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflicting internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_scratchbird_embedded_mode_spec_report.md`.
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md` (internal label conflicts).
[~] Embedded mode overview only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SCRATCHBIRD_SECURITY_AND_ACCESS_MODEL.md
Status: [~]
Spec: Non-authoritative (conflicting internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_scratchbird_security_and_access_model_report.md`.
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md` (internal label conflicts).
[~] Security/access overview only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SCRATCHBIRD_SERVER_ARCHITECTURE_CONSOLIDATED.md
Status: [~]
Spec: Non-authoritative (conflicting internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_scratchbird_server_architecture_consolidated_report.md`.
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md` (internal label conflicts).
[~] Architecture/lifecycle overview only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SELECT_AND_QUERY.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_select_and_query_report.md`.
Tasks:
[ ] Spec requires SBLR3_QUERY_* opcodes (SELECT/SETOP/VALUES/ORDER/LIMIT/OFFSET/FETCH/LOCK/CTE); none exist or are emitted.
[~] Implementation uses `SBLR3_SELECT` with `SCHEMA_SELECT` payload (set_op/with/order/limit/fetch fields).
[ ] Spec-defined error codes for SELECT/query forms not implemented.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SERVER_ARCHITECTURE_AND_CONNECTION_LIFECYCLE.md
Status: [~]
Spec: Non-authoritative (conflicting internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_architecture_and_connection_lifecycle_report.md`.
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md` (internal label conflicts).
[~] Architecture/lifecycle overview only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflicting internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_lifecycle_and_startup_spec_report.md`.
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md` (internal label conflicts).
[~] Server lifecycle/startup overview only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SESSION_AND_UTILITY.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_session_and_utility_report.md`.
Tasks:
[~] V3 parser supports SET/SHOW/RESET/EXPLAIN/ANALYZE/CONNECT/DISCONNECT/COMMENT (parser_v3).
[ ] Spec references parser_v2 implementation files (outdated).
[ ] SET TIME ZONE emitted as `SBLR3_SET_VARIABLE`, not `SBLR3_SET_TIME_ZONE`.
[ ] RESET variants emitted as `SBLR3_SET_VARIABLE` with action=3, not dedicated RESET opcodes.
[ ] RESET opcodes missing from opcode registry despite payload map references.
[ ] Spec requires string_id identifiers in payloads; emitter uses inline strings.
[ ] SET LOCAL semantics/SQLSTATE enforcement not found in V3 executor.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/00_SECURITY_SPEC_INDEX.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_00_security_spec_index_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/01_SECURITY_ARCHITECTURE.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_01_security_architecture_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/02_IDENTITY_AUTHENTICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_02_identity_authentication_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/03_AUTHORIZATION_MODEL.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_03_authorization_model_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/04.01_KEY_LIFECYCLE_STATE_MACHINES.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_04.01_key_lifecycle_state_machines_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/04.02_KEY_MATERIAL_HANDLING_REQUIREMENTS.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_04.02_key_material_handling_requirements_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/04.03_NONCE_IV_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_04.03_nonce_iv_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/04_ENCRYPTION_KEY_MANAGEMENT.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_04_encryption_key_management_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/05.A_IPC_WIRE_FORMAT_AND_EXAMPLES.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_05.a_ipc_wire_format_and_examples_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/05_IPC_SECURITY.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_05_ipc_security_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/06.01_QUORUM_PROPOSAL_CANONICALIZATION_HASHING.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_06.01_quorum_proposal_canonicalization_hashing_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/06.02_QUORUM_EVIDENCE_AND_AUDIT_COUPLING.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_06.02_quorum_evidence_and_audit_coupling_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/06_CLUSTER_SECURITY.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_06_cluster_security_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/07_NETWORK_PRESENCE_BINDING.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_07_network_presence_binding_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/08.01_AUDIT_EVENT_CANONICALIZATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_08.01_audit_event_canonicalization_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/08.02_AUDIT_CHAIN_VERIFICATION_CHECKPOINTS.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_08.02_audit_chain_verification_checkpoints_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/08_AUDIT_COMPLIANCE.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_08_audit_compliance_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/09_SECURITY_LEVELS.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_09_security_levels_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/10_RELEASE_INTEGRITY_PROVENANCE.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_10_release_integrity_provenance_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/AUTH_CERTIFICATE_TLS.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_auth_certificate_tls_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/AUTH_CORE_FRAMEWORK.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_auth_core_framework_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/AUTH_ENTERPRISE_LDAP_KERBEROS.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_auth_enterprise_ldap_kerberos_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/AUTH_MODERN_OAUTH_MFA.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_auth_modern_oauth_mfa_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/AUTH_PASSWORD_METHODS.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_auth_password_methods_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/EXTERNAL_AUTHENTICATION_DESIGN.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_external_authentication_design_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/ROLE_COMPOSITION_AND_HIERARCHIES.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_role_composition_and_hierarchies_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/contributor_security_rules.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_contributor_security_rules_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/sbsec_alpha_base.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_sbsec_alpha_base_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Security Design Specification/supportability_contract.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_security design specification_supportability_contract_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/TEMPORARY_TABLES_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflicting internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_temporary_tables_spec_report.md`.
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md` (internal label conflicts).
[~] Parser captures `temp_type` and `on_commit` in AST for TEMP/GLOBAL TEMP.
[ ] Dialect-specific ON COMMIT rejection rules not enforced in V3 parser.
[ ] V3 SBLR emission ignores temp flags/on_commit bitfield in CREATE TABLE payload.
[~] Executor expects legacy CREATE TABLE bytecode with temp flags; V3 SCHEMA payload path not implemented.
[~] Catalog includes temp metadata fields, but cleanup/visibility behaviors not verified.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/TRANSACTION_CONTROL.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_transaction_control_report.md`.
Tasks:
[~] Parser supports BEGIN/START/COMMIT/ROLLBACK/SAVEPOINT/RELEASE/SET TRANSACTION.
[ ] Spec requires SBLR3_TXN_* opcodes; emitter uses SBLR3_START_TRANSACTION/COMMIT/ROLLBACK/SAVEPOINT/ROLLBACK_TO_SAVEPOINT/RELEASE_SAVEPOINT/SET_TRANSACTION; TXN_* opcodes not in registry.
[ ] Spec-defined error codes not surfaced.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/UPDATE.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_update_report.md`.
Tasks:
[ ] Spec requires SBLR3_DML_UPDATE with DML_UPDATE payload; emitter uses SBLR3_UPDATE with SCHEMA_UPDATE.
[ ] Spec-defined error codes not surfaced.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/UTILITY_COPY.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_utility_copy_report.md`.
Tasks:
[*] Parsing: detect SELECT form vs table form; enforce COPY (SELECT ...) TO only; parse FROM/TO and STDIN/STDOUT or file literal; parse optional column list; see `src/parser/parser_v3.cpp:7152`
[*] Options: FORMAT/DELIMITER/NULL/HEADER/QUOTE/ESCAPE/ENCODING/BATCH_SIZE/MAX_ERRORS/ON_ERROR plus shorthand CSV/TEXT/BINARY; see `src/parser/parser_v3.cpp:7217` and `include/scratchbird/parser/ast_v3.h:3466`
[~] Emission: spec requires `SBLR3_UTILITY_COPY` + `UTILITY_COPY`; implementation emits `SBLR3_COPY` + `SCHEMA_COPY` with `has_query/query/target_table/direction/filename/format/options`; see `src/parser/v3_emitter.cpp:641` and `src/sblr/v3_schema.generated.cpp:241`
[~] Executor: COPY handled in V3 executor with CSV/TEXT/BINARY and options; opcode handled is `SBLR3_COPY`; see `src/sblr/executor.cpp:42918` and `:51510`
[ ] Errors: spec error codes `ERR_COPY_MISSING_TARGET/ERR_COPY_INVALID_FORMAT/ERR_COPY_UNSUPPORTED_OPTION` not mapped; code uses string errors

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/V2_PARSER_FIREBIRD_ALIGNMENT_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflicting internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_v2_parser_firebird_alignment_report.md`.
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md` (internal label conflicts).
[~] V2 parser alignment plan only; no V3 code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/V2_PARSER_INDEX_TYPE_COMPLETENESS.md
Status: [~]
Spec: Non-authoritative (historical V2 report). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_v2_parser_index_type_completeness_report.md`.
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] V2 parser completeness report only; no V3 code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/V3_SERVER_SPEC_INDEX.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_v3_server_spec_index_report.md`.
Tasks:
[~] Link integrity check: all listed paths resolve except one.
[ ] Missing file referenced by index: `/docs/specifications/parser/v3/security/README.md` not found in repo.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/V3_SINGLE_PATH_IMPLEMENTATION_GUIDE.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_v3_single_path_implementation_guide_report.md`.
Tasks:
[*] Reference integrity: parser grammar references exist under `docs/specifications/parser/v3/parser/`.
[~] Guidance conflicts with current opcode usage (QUERY/DML/TXN opcode naming mismatches noted in other reports).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/V3_ZERO_AMBIGUITY_BUILD_CHECKLIST.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_zero_ambiguity_build_checklist_report.md`.
Tasks:
[~] Checklist references numerous specs; no code-level verification performed.
[ ] `security/` specs referenced appear missing in repo (no `docs/specifications/parser/v3/security/`).
[~] Declared holes in storage, checksums, collation runtime format, server lifecycle coverage noted (per checklist).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/WINDOWING.md
Status: [~]
Spec: Authoritative. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_windowing_report.md`.
Tasks:
[~] Parser supports OVER/window frame syntax; window functions require OVER.
[ ] EXCLUDE and named windows are not explicitly rejected in parser.
[ ] V3 emitter does not emit SBLR3_WIN_* / WINDOW_SPEC / FRAME_* opcodes or SCHEMA_WINDOW_CALL payloads.
[ ] Executor window logic is legacy opcode-based, not V3 schema-based.
[ ] Spec error codes not surfaced.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/admin/README.md
Status: [~]
Spec: Non-authoritative README reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_admin_readme_report.md`.
Tasks:
[ ] SB_ADMIN_CLI_SPECIFICATION.md (not verified here).
[ ] SB_SERVER_NETWORK_CLI_SPECIFICATION.md (not verified here).
[ ] deployment/ (not verified here).
[ ] operations/ (not verified here).
[ ] Security Design Specification/ (not verified here).
[ ] catalog/ (not verified here).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/admin/SB_ADMIN_CLI_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (CLI tool spec). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sb_admin_cli_spec_report.md`.
Tasks:
[ ] Global options parsing (host/port/user/password/db/config/json/timeout/version/help).
[ ] Server command group (status/start/stop/restart/reload/info/config/connections/kill/terminate).
[ ] Database command group (list/create/drop/info/size/sweep/analyze/check).
[ ] Cluster command group (status/init/join/leave/nodes/promote/demote/failover/rebalance/sync-status).
[ ] User command group (list/create/drop/alter/password/roles/grant/revoke).
[ ] Backup command group (create/list/info/restore/verify/delete/schedule/export + create options).
[ ] Restore command group (full/pitr/table/status/cancel).
[ ] Diagnostics command group (health; diag: slow-queries/locks/bloat/cache/io/wait-events/activity/explain; logs: tail/search/errors/stats).
[ ] Monitoring integration (Nagios checks + exit codes, Prometheus metrics, SNMP).
[ ] Maintenance command group (sweep/vacuum alias, sweep status, reindex, maintenance mode).
[ ] Security command group (audit/ssl/keys/firewall).
[ ] Config file format support and defaults.
[ ] Exit code mapping.
[ ] Environment variables support.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/admin/SB_SERVER_NETWORK_CLI_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (CLI flags). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sb_server_network_cli_spec_report.md`.
Tasks:
[~] Listener executables exist (sb_listener_*); sb_server spawns them.
[~] Listener enable/disable flags implemented (`--enable-*`/`--disable-*`).
[~] Port override flags implemented, but naming differs (`--pg-port`/`--fb-port` vs spec `--postgres-port`/`--firebird-port`).
[~] Bind address override flags implemented.
[~] Parser pool min/max flags implemented.
[ ] CLI `--daemon` flag (spec) not implemented; `--foreground` used instead.
[ ] CLI `--log-level` flag (spec) not implemented; log level is config/env.
[ ] Precedence order (CLI > env > config > defaults) not explicitly validated.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/api/CONNECTION_POOLING_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (design). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_connection_pooling_spec_report.md`.
Tasks:
[~] Pool manager/database pool classes exist in `src/pool/connection_pool.cpp` (feature-rich) and `src/core/connection_pool.cpp` (engine pool).
[~] Statement cache and result cache components exist in `src/pool/statement_cache.cpp`, `src/pool/result_cache.cpp`, `src/sblr/query_result_cache.cpp`.
[ ] Pool config sections (`[pool]`, per-db overrides) not parsed in server config.
[ ] SQL interface for pool configuration (ALTER USER pool_*, etc.) not implemented in V3 parser/emitter.
[ ] Connection factory details (real protocol/TLS/auth) not implemented in `src/pool/connection_pool.cpp` (TODO placeholders).
[ ] Health checking/load balancing/user/application pool policies not verified.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/api/README.md
Status: [~]
Spec: Non-authoritative README reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_api_readme_report.md`.
Tasks:
[ ] CLIENT_LIBRARY_API_SPECIFICATION.md (in ScratchBird-driver; not verified here).
[~] CONNECTION_POOLING_SPECIFICATION.md (tracked separately).
[ ] network/ (not verified here).
[ ] wire_protocols/ (not verified here).
[ ] Security Design Specification/ (not verified here).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/00-Implementation-Roadmap.md
Status: [~]
Spec: Non-authoritative roadmap. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_00_implementation_roadmap_report.md`.
Tasks:
[ ] Roadmap milestones (Phase 1-3+) not verified here.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/01-Architecture-Overview.md
Status: [~]
Spec: Non-authoritative architecture overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_01_architecture_overview_report.md`.
Tasks:
[ ] Architecture reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/02-Clock-Synchronization-Specification.md
Status: [~]
Spec: Non-authoritative draft spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_02_clock_synchronization_spec_report.md`.
Tasks:
[ ] Clock synchronization/heartbeat protocol not found in codebase (not verified).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/03-Distributed-MVCC-Specification.md
Status: [~]
Spec: Non-authoritative draft spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_03_distributed_mvcc_spec_report.md`.
Tasks:
[~] UUID v7 support exists in core (`src/core/uuidv7.cpp`).
[ ] Distributed MVCC mechanisms (cluster clock, distributed conflict resolution) not verified.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/04-Replication-Protocol-Specification.md
Status: [~]
Spec: Non-authoritative draft spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_04_replication_protocol_spec_report.md`.
Tasks:
[ ] Replication protocol implementation not found in repo (not verified).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/05-Wire-Protocol-Integration-Specification.md
Status: [~]
Spec: Non-authoritative draft spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_05_wire_protocol_integration_spec_report.md`.
Tasks:
[~] Protocol adapters exist for native/pg/mysql/firebird.
[~] Listener executables exist and are spawned by `sb_server`.
[ ] MSSQL/TDS (Beta) not found in repo.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/06-Ingestion-Layer.md
Status: [~]
Spec: Non-authoritative draft spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_06_ingestion_layer_report.md`.
Tasks:
[ ] Ingestion layer implementation not found in repo (not verified).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/07-OLAP-Tier.md
Status: [~]
Spec: Non-authoritative draft spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_07_olap_tier_report.md`.
Tasks:
[ ] OLAP tier implementation not found in repo (not verified).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/08-Deployment-Guide.md
Status: [~]
Spec: Non-authoritative deployment guide. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_08_deployment_guide_report.md`.
Tasks:
[ ] Deployment/ops guidance only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/09-Monitoring-Observability.md
Status: [~]
Spec: Non-authoritative monitoring guide. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_09_monitoring_observability_report.md`.
Tasks:
[~] Server stats tracked; stats config has `prometheus_port`.
[ ] Prometheus exporter/log/trace integrations not found in repo (not verified).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/10-UDR-System-Specification.md
Status: [~]
Spec: Non-authoritative draft spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_10_udr_system_spec_report.md`.
Tasks:
[~] UDR connector code exists in `src/udr/` (ScratchBird/Firebird/PostgreSQL/MySQL).
[ ] Full UDR plugin API lifecycle/ABI not verified.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/11-Remote-Database-UDR-Specification.md
Status: [~]
Spec: Non-authoritative draft spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_11_remote_database_udr_spec_report.md`.
Tasks:
[~] Remote UDR connectors exist for PostgreSQL/MySQL/Firebird/ScratchBird.
[~] UDR connection pool exists in `src/udr/connection_pool.cpp`.
[ ] MSSQL/ODBC/JDBC connectors not found in repo.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/11a-Connection-Pool-Implementation.md
Status: [~]
Spec: Non-authoritative but canonical UDR pool behavior. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_11a_connection_pool_implementation_report.md`.
Tasks:
[~] UDR pool exists in `src/udr/connection_pool.cpp`.
[ ] Pool keying/isolation by server/user/tls/session options not represented.
[ ] Config fields differ (no `max_idle`, `reset_on_release`, `max_in_flight_per_conn`, backoff settings).
[ ] Release does not perform rollback/reset sequence; no protocol-specific reset.
[ ] Health check loop does not ping/validate idle connections.
[ ] Metrics set differs from spec; no last_error fields.
[ ] Cancellation integration not present.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/11b-PostgreSQL-Client-Implementation.md
Status: [~]
Spec: Non-authoritative client spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_11b_postgresql_client_implementation_report.md`.
Tasks:
[~] PostgreSQL UDR client implemented in `src/udr/postgresql_udr.cpp`.
[~] Parse/Bind/Execute and COPY flows present.
[ ] CancelRequest support not found in UDR client.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/11c-MySQL-Client-Implementation.md
Status: [~]
Spec: Non-authoritative client spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_11c_mysql_client_implementation_report.md`.
Tasks:
[~] MySQL UDR client implemented in `src/udr/mysql_udr.cpp`.
[ ] Cancellation support (KILL/COM_PROCESS_KILL) not found in UDR client.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/11d-MSSQL-Client-Implementation.md
Status: [~]
Spec: Non-authoritative client spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_11d_mssql_client_implementation_report.md`.
Tasks:
[ ] MSSQL/TDS UDR client not found in repo.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/11e-Firebird-Client-Implementation.md
Status: [~]
Spec: Non-authoritative client spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_11e_firebird_client_implementation_report.md`.
Tasks:
[~] Firebird UDR client implemented in `src/udr/firebird_udr.cpp`.
[ ] Cancellation support (op_cancel) not found in UDR client.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/11f-ODBC-Client-Implementation.md
Status: [~]
Spec: Non-authoritative client spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_11f_odbc_client_implementation_report.md`.
Tasks:
[ ] ODBC UDR connector not found in repo.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/11g-JDBC-Client-Implementation.md
Status: [~]
Spec: Non-authoritative client spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_11g_jdbc_client_implementation_report.md`.
Tasks:
[ ] JDBC UDR connector not found in repo.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/11h-Live-Migration-Emulated-Listener.md
Status: [~]
Spec: Non-authoritative migration spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_11h_live_migration_emulated_listener_report.md`.
Tasks:
[ ] Live migration router/emulated listener behavior not found in repo.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/11i-ScratchBird-Client-Implementation.md
Status: [~]
Spec: Non-authoritative client spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_11i_scratchbird_client_implementation_report.md`.
Tasks:
[~] ScratchBird UDR client implemented in `src/udr/scratchbird_udr.cpp`.
[ ] Cancellation support not clearly implemented/verified.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/Discussion_Notes.md
Status: [~]
Spec: Non-authoritative discussion notes. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_discussion_notes_report.md`.
Tasks:
[ ] Notes only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/README.md
Status: [~]
Spec: Non-authoritative README reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_alpha_phase2_readme_report.md`.
Tasks:
[ ] Index/overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/cluster/scratch_bird_cluster_architecture_security_specifications_draft.md
Status: [~]
Spec: Non-authoritative historical draft. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_cluster_architecture_security_draft_report.md`.
Tasks:
[ ] Draft only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/00_SECURITY_SPEC_INDEX.md
Status: [~]
Spec: Non-authoritative security index. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_00_index_report.md`.
Tasks:
[ ] Index/overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/01_SECURITY_ARCHITECTURE.md
Status: [~]
Spec: Non-authoritative security architecture. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_01_architecture_report.md`.
Tasks:
[ ] Architecture reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/02_IDENTITY_AUTHENTICATION.md
Status: [~]
Spec: Non-authoritative identity/auth spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_02_identity_authentication_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/03_AUTHORIZATION_MODEL.md
Status: [~]
Spec: Non-authoritative authorization model. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_03_authorization_model_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/04_ENCRYPTION_KEY_MANAGEMENT.md
Status: [~]
Spec: Non-authoritative encryption/key mgmt spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_04_encryption_key_management_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/05_IPC_SECURITY.md
Status: [~]
Spec: Non-authoritative IPC security spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_05_ipc_security_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/06_CLUSTER_SECURITY.md
Status: [~]
Spec: Non-authoritative cluster security spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_06_cluster_security_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/07_NETWORK_PRESENCE_BINDING.md
Status: [~]
Spec: Non-authoritative NPB spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_07_network_presence_binding_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/08_AUDIT_COMPLIANCE.md
Status: [~]
Spec: Non-authoritative audit/compliance spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_08_audit_compliance_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/09_SECURITY_LEVELS.md
Status: [~]
Spec: Non-authoritative security levels spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_09_security_levels_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/Beta Task -Distributed Secret Sharing Implementation Specification.md
Status: [~]
Spec: Non-authoritative (despite "implementation spec"). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_beta_task_distributed_secret_sharing_report.md`.
Tasks:
[ ] Network Presence Binding / secret sharing implementation not found in repo.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/Engine Internal Security.md
Status: [~]
Spec: Non-authoritative security model. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_engine_internal_security_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/SECURITY_IMPLIMENTATION_DETAILS.md
Status: [~]
Spec: Non-authoritative legacy security doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_implementation_details_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/SECURITY_SYSTEM_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative security system spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_system_spec_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/Security Hardening Guide.md
Status: [~]
Spec: Non-authoritative hardening guide. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_hardening_guide_report.md`.
Tasks:
[ ] Guidance only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/security/draft_security_architecture_specification.md
Status: [~]
Spec: Non-authoritative draft architecture. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_archive_security_draft_architecture_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/audit/IMPROVEMENT_OPPORTUNITIES.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_audit_improvement_opportunities_report.md`.
Tasks:
[ ] Placeholder; document not authored.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/audit/after_transaction_documentation_work.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_audit_after_transaction_documentation_work_report.md`.
Tasks:
[ ] Placeholder; document not authored.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/audit/after_transaction_work.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_audit_after_transaction_work_report.md`.
Tasks:
[ ] Placeholder; document not authored.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/audits/audit_2025_10_06.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_audits_audit_2025_10_06_report.md`.
Tasks:
[ ] Placeholder; document not authored.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/audits/error_handling_audit_2025_10_07.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_audits_error_handling_audit_2025_10_07_report.md`.
Tasks:
[ ] Placeholder; document not authored.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/00_DRIVERS_AND_INTEGRATIONS_INDEX.md
Status: [~]
Spec: Non-authoritative index. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_00_drivers_integrations_index_report.md`.
Tasks:
[ ] Index/overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/COMPLETION_STATUS.md
Status: [~]
Spec: Non-authoritative status snapshot. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_completion_status_report.md`.
Tasks:
[ ] Status snapshot only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/Cluster Specification Work/README.md
Status: [~]
Spec: Non-authoritative pointer. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_cluster_spec_readme_report.md`.
Tasks:
[ ] Pointer only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/DIRECTORY_STRUCTURE_CREATED.md
Status: [~]
Spec: Non-authoritative directory structure summary. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_directory_structure_created_report.md`.
Tasks:
[ ] Summary only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/README.md
Status: [~]
Spec: Non-authoritative index/overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/Security Design Specification/README.md
Status: [~]
Spec: Non-authoritative pointer. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_security_design_spec_readme_report.md`.
Tasks:
[ ] Pointer only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/ai-ml/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_ai_ml_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/ai-ml/haystack/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_ai_ml_haystack_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/ai-ml/langchain/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_ai_ml_langchain_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/ai-ml/vector-apis/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_ai_ml_vector_apis_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/applications/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_applications_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/applications/drupal/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_applications_drupal_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/applications/geoserver/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_applications_geoserver_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/applications/joomla/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_applications_joomla_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/applications/magento/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_applications_magento_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/applications/mattermost/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_applications_mattermost_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/applications/metabase/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_applications_metabase_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/applications/odoo-erp/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_applications_odoo_erp_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/applications/qgis/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_applications_qgis_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/applications/woocommerce/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_applications_woocommerce_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/applications/wordpress/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_applications_wordpress_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/big-data-streaming/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_big_data_streaming_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/big-data-streaming/apache-flink/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_big_data_streaming_apache_flink_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/big-data-streaming/apache-kafka/README.md
Status: [~]
Spec: Non-authoritative overview (conflicts with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_big_data_streaming_apache_kafka_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/big-data-streaming/apache-kafka/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_big_data_streaming_apache_kafka_spec_report.md`.
Tasks:
[ ] Kafka broadcaster/consumer implementation not verified in codebase.
[ ] CDC/DDL/audit topics and payload formats not verified.
[ ] Metrics/offset persistence not verified.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/big-data-streaming/apache-spark/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_big_data_streaming_apache_spark_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/big-data-streaming/etl-platforms/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_big_data_streaming_etl_platforms_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/big-data-streaming/hadoop-ecosystem/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_big_data_streaming_hadoop_ecosystem_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/builds/00_BUILD_REQUIREMENTS_INDEX.md
Status: [~]
Spec: Non-authoritative index. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_builds_00_index_report.md`.
Tasks:
[ ] Index/overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/builds/01_LINUX_NATIVE.md
Status: [~]
Spec: Non-authoritative build requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_builds_01_linux_native_report.md`.
Tasks:
[ ] Build requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/builds/02_WINDOWS_NATIVE.md
Status: [~]
Spec: Non-authoritative build requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_builds_02_windows_native_report.md`.
Tasks:
[ ] Build requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/builds/03_MACOS_NATIVE.md
Status: [~]
Spec: Non-authoritative build requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_builds_03_macos_native_report.md`.
Tasks:
[ ] Build requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/builds/10_LINUX_TO_WINDOWS.md
Status: [~]
Spec: Non-authoritative build requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_builds_10_linux_to_windows_report.md`.
Tasks:
[ ] Build requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/builds/11_LINUX_TO_MACOS.md
Status: [~]
Spec: Non-authoritative build requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_builds_11_linux_to_macos_report.md`.
Tasks:
[ ] Build requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/builds/12_WINDOWS_TO_LINUX.md
Status: [~]
Spec: Non-authoritative build requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_builds_12_windows_to_linux_report.md`.
Tasks:
[ ] Build requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/builds/20_APPIMAGE.md
Status: [~]
Spec: Non-authoritative packaging requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_builds_20_appimage_report.md`.
Tasks:
[ ] Packaging requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/builds/23_DEB.md
Status: [~]
Spec: Non-authoritative packaging requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_builds_23_deb_report.md`.
Tasks:
[ ] Packaging requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/builds/24_RPM.md
Status: [~]
Spec: Non-authoritative packaging requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_builds_24_rpm_report.md`.
Tasks:
[ ] Packaging requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/builds/27_BREW.md
Status: [~]
Spec: Non-authoritative packaging requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_builds_27_brew_report.md`.
Tasks:
[ ] Packaging requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/builds/30_DOCKER.md
Status: [~]
Spec: Non-authoritative Docker build requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_builds_30_docker_report.md`.
Tasks:
[ ] Docker build/packaging requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/builds/40_GITHUB_ACTIONS.md
Status: [~]
Spec: Non-authoritative CI/CD requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_builds_40_github_actions_report.md`.
Tasks:
[ ] CI/CD requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/builds/COMPLETE_BUILD_ENVIRONMENT_SETUP.md
Status: [~]
Spec: Non-authoritative build environment setup requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_builds_complete_build_environment_setup_report.md`.
Tasks:
[ ] Environment setup requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/cloud-container/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_cloud_container_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/cloud-container/docker/README.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_cloud_container_docker_readme_report.md`.
Tasks:
[ ] Requirements doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/cloud-container/docker/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_cloud_container_docker_spec_report.md`.
Tasks:
[ ] Docker image requirements not verified in repo (no Dockerfiles/CI reviewed).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/cloud-container/helm-charts/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_cloud_container_helm_charts_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/cloud-container/kubernetes/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_cloud_container_kubernetes_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/cloud-container/terraform/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_cloud_container_terraform_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/connectivity/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_connectivity_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/connectivity/jdbc/README.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_connectivity_jdbc_readme_report.md`.
Tasks:
[ ] Requirements overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/connectivity/odbc/README.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_connectivity_odbc_readme_report.md`.
Tasks:
[ ] Requirements doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/connectivity/odbc/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_connectivity_odbc_spec_report.md`.
Tasks:
[ ] Requirements doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/DRIVER_BASELINE_SPEC.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_driver_baseline_spec_report.md`.
Tasks:
[ ] Requirements doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/cpp/API_REFERENCE.md
Status: [~]
Spec: Non-authoritative API reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_cpp_api_reference_report.md`.
Tasks:
[ ] API reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/cpp/COMPATIBILITY_MATRIX.md
Status: [~]
Spec: Non-authoritative compatibility overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_cpp_compatibility_matrix_report.md`.
Tasks:
[ ] Compatibility overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/cpp/IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Non-authoritative implementation plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_cpp_implementation_plan_report.md`.
Tasks:
[ ] Plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/cpp/MIGRATION_GUIDE.md
Status: [~]
Spec: Non-authoritative migration guidance. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_cpp_migration_guide_report.md`.
Tasks:
[ ] Migration guidance only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/cpp/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_cpp_specification_report.md`.
Tasks:
[ ] Driver requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/cpp/TESTING_CRITERIA.md
Status: [~]
Spec: Non-authoritative testing criteria. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_cpp_testing_criteria_report.md`.
Tasks:
[ ] Testing criteria only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/dotnet-csharp/API_REFERENCE.md
Status: [~]
Spec: Non-authoritative API reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_dotnet_csharp_api_reference_report.md`.
Tasks:
[ ] API reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/dotnet-csharp/COMPATIBILITY_MATRIX.md
Status: [~]
Spec: Non-authoritative compatibility overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_dotnet_csharp_compatibility_matrix_report.md`.
Tasks:
[ ] Compatibility overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/dotnet-csharp/IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Non-authoritative implementation plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_dotnet_csharp_implementation_plan_report.md`.
Tasks:
[ ] Plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/dotnet-csharp/MIGRATION_GUIDE.md
Status: [~]
Spec: Non-authoritative migration guidance. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_dotnet_csharp_migration_guide_report.md`.
Tasks:
[ ] Migration guidance only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/dotnet-csharp/README.md
Status: [~]
Spec: Non-authoritative overview (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_dotnet_csharp_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/dotnet-csharp/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_dotnet_csharp_specification_report.md`.
Tasks:
[ ] Driver requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/dotnet-csharp/TESTING_CRITERIA.md
Status: [~]
Spec: Non-authoritative testing criteria. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_dotnet_csharp_testing_criteria_report.md`.
Tasks:
[ ] Testing criteria only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/golang/API_REFERENCE.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_golang_api_reference_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/golang/COMPATIBILITY_MATRIX.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_golang_compatibility_matrix_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/golang/IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Non-authoritative implementation plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_golang_implementation_plan_report.md`.
Tasks:
[ ] Plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/golang/MIGRATION_GUIDE.md
Status: [~]
Spec: Non-authoritative migration guidance. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_golang_migration_guide_report.md`.
Tasks:
[ ] Migration guidance only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/golang/README.md
Status: [~]
Spec: Non-authoritative overview (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_golang_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/golang/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_golang_specification_report.md`.
Tasks:
[ ] Driver requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/golang/TESTING_CRITERIA.md
Status: [~]
Spec: Non-authoritative testing criteria. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_golang_testing_criteria_report.md`.
Tasks:
[ ] Testing criteria only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/java-jdbc/API_REFERENCE.md
Status: [~]
Spec: Non-authoritative API reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_java_jdbc_api_reference_report.md`.
Tasks:
[ ] API reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/java-jdbc/COMPATIBILITY_MATRIX.md
Status: [~]
Spec: Non-authoritative compatibility overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_java_jdbc_compatibility_matrix_report.md`.
Tasks:
[ ] Compatibility overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/java-jdbc/IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Non-authoritative implementation plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_java_jdbc_implementation_plan_report.md`.
Tasks:
[ ] Plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/java-jdbc/MIGRATION_GUIDE.md
Status: [~]
Spec: Non-authoritative migration guidance. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_java_jdbc_migration_guide_report.md`.
Tasks:
[ ] Migration guidance only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/java-jdbc/README.md
Status: [~]
Spec: Non-authoritative overview (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_java_jdbc_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/java-jdbc/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_java_jdbc_specification_report.md`.
Tasks:
[ ] Driver requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/java-jdbc/TESTING_CRITERIA.md
Status: [~]
Spec: Non-authoritative testing criteria. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_java_jdbc_testing_criteria_report.md`.
Tasks:
[ ] Testing criteria only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/nodejs-typescript/API_REFERENCE.md
Status: [~]
Spec: Non-authoritative API reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_nodejs_typescript_api_reference_report.md`.
Tasks:
[ ] API reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/nodejs-typescript/COMPATIBILITY_MATRIX.md
Status: [~]
Spec: Non-authoritative compatibility overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_nodejs_typescript_compatibility_matrix_report.md`.
Tasks:
[ ] Compatibility overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/nodejs-typescript/IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Non-authoritative implementation plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_nodejs_typescript_implementation_plan_report.md`.
Tasks:
[ ] Plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/nodejs-typescript/MIGRATION_GUIDE.md
Status: [~]
Spec: Non-authoritative migration guidance. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_nodejs_typescript_migration_guide_report.md`.
Tasks:
[ ] Migration guidance only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/nodejs-typescript/README.md
Status: [~]
Spec: Non-authoritative overview (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_nodejs_typescript_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/nodejs-typescript/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_nodejs_typescript_specification_report.md`.
Tasks:
[ ] Driver requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/nodejs-typescript/TESTING_CRITERIA.md
Status: [~]
Spec: Non-authoritative testing criteria. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_nodejs_typescript_testing_criteria_report.md`.
Tasks:
[ ] Testing criteria only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/pascal-delphi/API_REFERENCE.md
Status: [~]
Spec: Non-authoritative API reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_pascal_delphi_api_reference_report.md`.
Tasks:
[ ] API reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/pascal-delphi/COMPATIBILITY_MATRIX.md
Status: [~]
Spec: Non-authoritative compatibility overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_pascal_delphi_compatibility_matrix_report.md`.
Tasks:
[ ] Compatibility overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/pascal-delphi/IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Non-authoritative implementation plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_pascal_delphi_implementation_plan_report.md`.
Tasks:
[ ] Plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/pascal-delphi/MIGRATION_GUIDE.md
Status: [~]
Spec: Non-authoritative migration guidance. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_pascal_delphi_migration_guide_report.md`.
Tasks:
[ ] Migration guidance only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/pascal-delphi/README.md
Status: [~]
Spec: Non-authoritative overview (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_pascal_delphi_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/pascal-delphi/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_pascal_delphi_specification_report.md`.
Tasks:
[ ] Driver requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/pascal-delphi/TESTING_CRITERIA.md
Status: [~]
Spec: Non-authoritative testing criteria. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_pascal_delphi_testing_criteria_report.md`.
Tasks:
[ ] Testing criteria only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/php/API_REFERENCE.md
Status: [~]
Spec: Non-authoritative API reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_php_api_reference_report.md`.
Tasks:
[ ] API reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/php/COMPATIBILITY_MATRIX.md
Status: [~]
Spec: Non-authoritative compatibility overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_php_compatibility_matrix_report.md`.
Tasks:
[ ] Compatibility overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/php/IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Non-authoritative implementation plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_php_implementation_plan_report.md`.
Tasks:
[ ] Plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/php/MIGRATION_GUIDE.md
Status: [~]
Spec: Non-authoritative migration guidance. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_php_migration_guide_report.md`.
Tasks:
[ ] Migration guidance only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/php/README.md
Status: [~]
Spec: Non-authoritative overview (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_php_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/php/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_php_specification_report.md`.
Tasks:
[ ] Driver requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/php/TESTING_CRITERIA.md
Status: [~]
Spec: Non-authoritative testing criteria. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_php_testing_criteria_report.md`.
Tasks:
[ ] Testing criteria only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/python/API_REFERENCE.md
Status: [~]
Spec: Non-authoritative API reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_python_api_reference_report.md`.
Tasks:
[ ] API reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/python/COMPATIBILITY_MATRIX.md
Status: [~]
Spec: Non-authoritative compatibility overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_python_compatibility_matrix_report.md`.
Tasks:
[ ] Compatibility overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/python/IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Non-authoritative implementation plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_python_implementation_plan_report.md`.
Tasks:
[ ] Plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/python/MIGRATION_GUIDE.md
Status: [~]
Spec: Non-authoritative migration guidance. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_python_migration_guide_report.md`.
Tasks:
[ ] Migration guidance only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/python/README.md
Status: [~]
Spec: Non-authoritative overview (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_python_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/python/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_python_specification_report.md`.
Tasks:
[ ] Driver requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/python/TESTING_CRITERIA.md
Status: [~]
Spec: Non-authoritative testing criteria. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_python_testing_criteria_report.md`.
Tasks:
[ ] Testing criteria only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/r/API_REFERENCE.md
Status: [~]
Spec: Non-authoritative API reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_r_api_reference_report.md`.
Tasks:
[ ] API reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/r/COMPATIBILITY_MATRIX.md
Status: [~]
Spec: Non-authoritative compatibility overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_r_compatibility_matrix_report.md`.
Tasks:
[ ] Compatibility overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/r/IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Non-authoritative implementation plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_r_implementation_plan_report.md`.
Tasks:
[ ] Plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/r/MIGRATION_GUIDE.md
Status: [~]
Spec: Non-authoritative migration guidance. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_r_migration_guide_report.md`.
Tasks:
[ ] Migration guidance only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/r/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_r_specification_report.md`.
Tasks:
[ ] Driver requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/r/TESTING_CRITERIA.md
Status: [~]
Spec: Non-authoritative testing criteria. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_r_testing_criteria_report.md`.
Tasks:
[ ] Testing criteria only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/ruby/API_REFERENCE.md
Status: [~]
Spec: Non-authoritative API reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_ruby_api_reference_report.md`.
Tasks:
[ ] API reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/ruby/COMPATIBILITY_MATRIX.md
Status: [~]
Spec: Non-authoritative compatibility overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_ruby_compatibility_matrix_report.md`.
Tasks:
[ ] Compatibility overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/ruby/IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Non-authoritative implementation plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_ruby_implementation_plan_report.md`.
Tasks:
[ ] Plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/ruby/MIGRATION_GUIDE.md
Status: [~]
Spec: Non-authoritative migration guidance. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_ruby_migration_guide_report.md`.
Tasks:
[ ] Migration guidance only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/ruby/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_ruby_specification_report.md`.
Tasks:
[ ] Driver requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/ruby/TESTING_CRITERIA.md
Status: [~]
Spec: Non-authoritative testing criteria. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_ruby_testing_criteria_report.md`.
Tasks:
[ ] Testing criteria only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/rust/API_REFERENCE.md
Status: [~]
Spec: Non-authoritative API reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_rust_api_reference_report.md`.
Tasks:
[ ] API reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/rust/COMPATIBILITY_MATRIX.md
Status: [~]
Spec: Non-authoritative compatibility overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_rust_compatibility_matrix_report.md`.
Tasks:
[ ] Compatibility overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/rust/IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Non-authoritative implementation plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_rust_implementation_plan_report.md`.
Tasks:
[ ] Plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/rust/MIGRATION_GUIDE.md
Status: [~]
Spec: Non-authoritative migration guidance. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_rust_migration_guide_report.md`.
Tasks:
[ ] Migration guidance only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/rust/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_rust_specification_report.md`.
Tasks:
[ ] Driver requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/drivers/rust/TESTING_CRITERIA.md
Status: [~]
Spec: Non-authoritative testing criteria. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_drivers_rust_testing_criteria_report.md`.
Tasks:
[ ] Testing criteria only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/indexes/README.md
Status: [~]
Spec: Non-authoritative pointer to canonical index specs. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_indexes_readme_report.md`.
Tasks:
[ ] Pointer/overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/NOSQL_CATALOG_MODEL_SPEC.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_nosql_catalog_model_spec_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/NOSQL_CONSOLIDATION_AND_GAP_MATRIX.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_nosql_consolidation_and_gap_matrix_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/NOSQL_ENGINE_TYPE_OVERVIEW.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_nosql_engine_type_overview_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/NOSQL_LANGUAGE_SPEC_TRACKER.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_nosql_language_spec_tracker_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/NOSQL_SCHEMA_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_nosql_schema_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/NOSQL_STORAGE_STRUCTURES_REPORT.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_nosql_storage_structures_report_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/arangodb_aql/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_arangodb_aql_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/cassandra_cql/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_cassandra_cql_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/couchbase_n1ql/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_couchbase_n1ql_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/couchdb_mango/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_couchdb_mango_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/cypher/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_cypher_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/elasticsearch_dsl/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_elasticsearch_dsl_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/flux/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_flux_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/gremlin/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_gremlin_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/hbase_shell/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_hbase_shell_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/influxql/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_influxql_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/lucene_query_syntax/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_lucene_query_syntax_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/milvus_query/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_milvus_query_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/mongodb_mql/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_mongodb_mql_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/promql/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_promql_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/redis_resp/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_redis_resp_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/sparql/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_sparql_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/nosql/languages/weaviate_graphql/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_nosql_languages_weaviate_graphql_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/optional/AUDIT_TEMPORAL_HISTORY_ARCHIVE.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_optional_audit_temporal_history_archive_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/optional/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_optional_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/optional/STORAGE_ENCODING_OPTIMIZATIONS.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_optional_storage_encoding_optimizations_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/optional/TABLESPACE_SHRINK_COMPACTION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_optional_tablespace_shrink_compaction_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/orms-frameworks/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_orms-frameworks_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/orms-frameworks/cypher-opencypher/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_orms-frameworks_cypher-opencypher_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/orms-frameworks/dapper/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_orms-frameworks_dapper_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/orms-frameworks/django-orm/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_orms-frameworks_django-orm_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/orms-frameworks/entity-framework-core/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_orms-frameworks_entity-framework-core_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/orms-frameworks/gremlin-tinkerpop/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_orms-frameworks_gremlin-tinkerpop_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/orms-frameworks/hibernate-jpa/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_orms-frameworks_hibernate-jpa_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/orms-frameworks/laravel-eloquent/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_orms-frameworks_laravel-eloquent_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/orms-frameworks/prisma/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_orms-frameworks_prisma_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/orms-frameworks/rails-active-record/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_orms-frameworks_rails-active-record_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/orms-frameworks/sequelize/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_orms-frameworks_sequelize_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/orms-frameworks/sqlalchemy/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_orms-frameworks_sqlalchemy_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/orms-frameworks/typeorm/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_orms-frameworks_typeorm_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/BETA_REPLICATION_ARCHITECTURE_FINDINGS.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_beta_replication_architecture_findings_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/REPLICATION_AND_SHADOW_PROTOCOLS.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_replication_and_shadow_protocols_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/WAL_IMPLEMENTATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_wal_implementation_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/00_BETA_REPLICATION_INDEX.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_uuidv7-optimized_00_beta_replication_index_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/00_REPLICATION_INDEX.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_uuidv7-optimized_00_replication_index_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/01_CORE_ARCHITECTURE.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_uuidv7-optimized_01_core_architecture_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/01_UUIDV8_HLC_ARCHITECTURE.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_uuidv7-optimized_01_uuidv8_hlc_architecture_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/02_LEADERLESS_QUORUM_REPLICATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_uuidv7-optimized_02_leaderless_quorum_replication_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/03_SCHEMA_DRIVEN_COLOCATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_uuidv7-optimized_03_schema_driven_colocation_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/04_TIME_PARTITIONED_MERKLE_FOREST.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_uuidv7-optimized_04_time_partitioned_merkle_forest_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/05_MGA_INTEGRATION.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_uuidv7-optimized_05_mga_integration_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/06_IMPLEMENTATION_PHASES.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_uuidv7-optimized_06_implementation_phases_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/07_TESTING_STRATEGY.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_uuidv7-optimized_07_testing_strategy_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/08_MIGRATION_OPERATIONS.md
Status: [~]
Spec: Non-authoritative operations guide. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_replication_uuidv7_optimized_08_migration_operations_report.md`.
Tasks:
[ ] Operations guidance only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/tools/README.md
Status: [~]
Spec: Non-authoritative tools overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_tools_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/tools/datagrip/README.md
Status: [~]
Spec: Non-authoritative integration requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_tools_datagrip_readme_report.md`.
Tasks:
[ ] Integration requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/tools/dbeaver/README.md
Status: [~]
Spec: Non-authoritative integration requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_tools_dbeaver_readme_report.md`.
Tasks:
[ ] Integration requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/tools/excel/README.md
Status: [~]
Spec: Non-authoritative integration requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_tools_excel_readme_report.md`.
Tasks:
[ ] Integration requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/tools/grafana/README.md
Status: [~]
Spec: Non-authoritative integration requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_tools_grafana_readme_report.md`.
Tasks:
[ ] Integration requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/tools/metabase/README.md
Status: [~]
Spec: Non-authoritative integration requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_tools_metabase_readme_report.md`.
Tasks:
[ ] Integration requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/tools/mysql-workbench/README.md
Status: [~]
Spec: Non-authoritative integration requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_tools_mysql-workbench_readme_report.md`.
Tasks:
[ ] Integration requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/tools/pgadmin/README.md
Status: [~]
Spec: Non-authoritative integration requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_tools_pgadmin_readme_report.md`.
Tasks:
[ ] Integration requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/tools/power-bi/README.md
Status: [~]
Spec: Non-authoritative integration requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_tools_power-bi_readme_report.md`.
Tasks:
[ ] Integration requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/tools/prometheus/README.md
Status: [~]
Spec: Non-authoritative integration requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_tools_prometheus_readme_report.md`.
Tasks:
[ ] Integration requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/tools/qlik/README.md
Status: [~]
Spec: Non-authoritative integration requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_tools_qlik_readme_report.md`.
Tasks:
[ ] Integration requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/beta_requirements/tools/tableau/README.md
Status: [~]
Spec: Non-authoritative integration requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_beta_requirements_tools_tableau_readme_report.md`.
Tasks:
[ ] Integration requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/catalog/CATALOG_CORRECTION_PLAN.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_catalog_catalog_correction_plan_report.md`.
Tasks:
[ ] Plan/strategy only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/catalog/COMPONENT_MODEL_AND_RESPONSIBILITIES.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_catalog_component_model_and_responsibilities_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/catalog/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_catalog_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/catalog/SCHEMA_PATH_RESOLUTION.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_catalog_schema_path_resolution_report.md`.
Tasks:
[ ] Requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/catalog/SCHEMA_PATH_SECURITY_DEFAULTS.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_catalog_schema_path_security_defaults_report.md`.
Tasks:
[ ] Requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_DDL_SBDB.md
Status: [~]
Spec: Non-authoritative catalog DDL. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_catalog_system_catalog_ddl_sbdb_report.md`.
Tasks:
[ ] Catalog DDL only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_DOMAIN_MAP.md
Status: [~]
Spec: Non-authoritative (not in inventory; internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_catalog_system_catalog_domain_map_report.md`.
Tasks:
[ ] Domain map only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_STRUCTURE.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_catalog_system_catalog_structure_report.md`.
Tasks:
[ ] Catalog structure only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/catalog/UUID_LIFECYCLE_RULES.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_catalog_uuid_lifecycle_rules_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/compression/COMPRESSION_FRAMEWORK.md
Status: [~]
Spec: Non-authoritative framework spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_compression_framework_report.md`.
Tasks:
[ ] Framework overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/compression/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_compression_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/CACHE_AND_BUFFER_ARCHITECTURE.md
Status: [*]
Spec: Cache and Buffer Architecture (non-authoritative; conflicts with inventory)
Tasks:
[*] Authoritative status check: header says non-authoritative; not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`; internal `Status: Authoritative (V3)` conflicts.
[*] Conformance items captured for cross-reference to authoritative specs (buffer pool, LSM cache, statement/plan cache, result cache, translation cache, invalidation rules, monitoring, config).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/CORE_IMPLEMENTATION_SPECS_SUMMARY.md
Status: [~]
Spec: Core Implementation Specs Summary (non-authoritative index)
Tasks:
[~] Authoritative status check - non-authoritative; not in inventory
[ ] Validate index implementation claims against index specs
[ ] Validate network layer claims against NETWORK_LAYER_SPEC
[ ] Validate optimizer claims against QUERY_OPTIMIZER_SPEC
[ ] Validate storage engine claims against storage specs
[ ] Validate transaction/lock claims against transaction specs
[ ] Verify reserved feature rejection policy and error codes in authoritative specs

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/ENGINE_CORE_UNIFIED_SPEC.md
Status: [*]
Spec: Engine Core Unified Spec (non-authoritative summary)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[*] Captured conformance areas to verify in authoritative specs (storage, catalog, txn/lock/GC, DDL/DML, indexes, optimizer, types, security, monitoring, backup, scheduler, UDR).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/GIT_METADATA_INTEGRATION_SPECIFICATION.md
Status: [*]
Spec: Git Metadata Integration (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[*] Captured cross-reference requirements (SQL surface, deterministic export rules, config keys, system tables, DDL tracking, migrations, conflicts, security, error codes, libgit2-only constraint).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/IMPLEMENTATION_RECOMMENDATIONS.md
Status: [*]
Spec: Implementation Recommendations (non-authoritative guidance)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[*] Captured advisory areas for cross-reference (indexes, network/pool, optimizer, buffer pool/storage, transactions/locks).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/INTERNAL_FUNCTIONS.md
Status: [~]
Spec: Internal Functions (header claims authoritative but not in inventory)
Tasks:
[~] Authoritative status check - header conflict; not in inventory
[~] Temporal functions (NOW/CURRENT_DATE/CURRENT_TIME/DATE_ADD/SUB/DIFF) implemented in evaluator
[~] DIV/STARTING WITH/CONTAINING parsed + V3 opcodes emitted
[ ] Function matrix mapping to V3 opcodes (LTRIM/RTRIM/CONCAT_WS/REPLACE/ENDS_WITH/TO_CHAR/TO_DATE/TO_TIMESTAMP/LEAST/GREATEST/ARRAY_POSITION/ARRAY_SLICE/JSON_EXISTS/JSON_HAS_KEY) - mostly missing in V3 emitter
[~] Extended opcode implementations exist in executor for many functions (legacy path)
[ ] V3 executor handling for SBLR3_FUNC_* opcodes and SBLR3_EXPR_FUNCTION_CALL (currently minimal)
[ ] Stored/UDR expression calls via EXT_EXPR_FUNCTION_CALL in V3 path

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/LIVE_MIGRATION_PASSTHROUGH_SPECIFICATION.md
Status: [*]
Spec: Live Migration Passthrough (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[*] Captured cross-reference requirements (state machine, routing/CDC/dual-write, SQL surface, system tables, monitoring, security, config, error/recovery).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/NAMESPACE_FUNCTION_MAP.md
Status: [~]
Spec: Namespace and Function/Procedure Map (generated index; non-authoritative)
Tasks:
[~] Authoritative status check - not in inventory
[ ] Use as navigation/reference only (no conformance requirements)

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/README.md
Status: [*]
Spec: Core README (non-authoritative index)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[*] Navigation-only; no conformance requirements.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/THREAD_SAFETY.md
Status: [*]
Spec: Thread Safety Specification (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[*] Captured advisory items for cross-reference (component contracts, lock ordering, critical sections, documentation requirement).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/Y_VALVE_ARCHITECTURE.md
Status: [~]
Spec: Listener/Parser Pool Architecture (Legacy Y-Valve, non-authoritative)
Tasks:
[~] Authoritative status check - non-authoritative; not in inventory
[ ] Validate listener/pool implementation against NETWORK_LISTENER_AND_PARSER_POOL_SPEC
[ ] Treat legacy Y-Valve content as reference-only (no conformance)

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/design_limits.md
Status: [*]
Spec: Design Limits and Maximum Sizes (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[*] Captured limits for cross-reference (page sizes/headers, FSM chaining, buffer pool defaults, tuple/TOAST limits, catalog identifier lengths, limit error handling).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/02_DDL_STATEMENTS_OVERVIEW.md
Status: [*]
Spec: DDL Statements Overview (non-authoritative index)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[*] Navigation-only; no conformance requirements.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/09_DDL_FOREIGN_DATA.md
Status: [*]
Spec: DDL Foreign Data (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[*] Captured cross-reference DDL surface (FDW, SERVER, USER MAPPING, FOREIGN TABLE, sys.* passthrough routines).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/CASCADE_DROP_SPECIFICATION.md
Status: [*]
Spec: CASCADE DROP (non-authoritative; RESTRICT-only policy)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[*] Captured cross-reference drop semantics (owned vs dependent, error format, dependency queries, drop flow).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_DATABASES.md
Status: [~]
Spec: DDL Databases (non-authoritative)
Tasks:
[~] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Native CREATE DATABASE options (page size/charset/collation/encrypted/owner) not parsed or emitted in V3 path.
[~] CREATE DATABASE EMULATED parsing exists, but emitter does not serialize path/source/options/aliases for executor.
[~] ALTER DATABASE SET DEFAULT CHARACTER SET/COLLATE/SWEEP INTERVAL not parsed or emitted; executor only supports emulated rename/owner/alias/options.
[~] DROP DATABASE CASCADE maps to FORCE semantics (superuser) rather than full CASCADE behavior.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_EVENTS.md
Status: [~]
Spec: DDL Events (non-authoritative)
Tasks:
[~] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Parser + emitter exist for POST_EVENT, but emitter requires literal only and payload is event_name only.
[ ] Executor handling for `SBLR3_PSQL_POST_EVENT` not found (delivery likely unimplemented).
[ ] ON COMMIT/IMMEDIATE and MESSAGE payload not represented in AST/emitter/executor.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_EXCEPTIONS.md
Status: [~]
Spec: DDL Exceptions (non-authoritative)
Tasks:
[~] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] CREATE EXCEPTION implemented as name + message only (no params/templates/builder/options).
[ ] Missing spec features: parameter list, message template/builder, options (severity/sqlstate/hint/detail).
[~] DROP EXCEPTION implemented with IF EXISTS and catalog drop.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_FUNCTIONS.md
Status: [~]
Spec: DDL Functions (non-authoritative)
Tasks:
[~] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] CREATE FUNCTION supports params/RETURNS/DETERMINISTIC/SQL SECURITY + body; language hardcoded to SQL.
[ ] Missing spec features: LANGUAGE, volatility classes, PARALLEL, COST/ROWS, RETURNS SETOF/TABLE/CURSOR.
[~] ALTER FUNCTION per spec not parsed; executor supports flags but no V3 DDL wiring.
[~] DROP FUNCTION by name only; no signature resolution or CASCADE/RESTRICT handling.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_INDEXES.md
Status: [~]
Spec: DDL Indexes (non-authoritative)
Tasks:
[~] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Parser supports UNIQUE/CONCURRENTLY/IF NOT EXISTS/USING/expr/INCLUDE/WHERE/TABLESPACE/WITH options.
[ ] Emitter does not serialize unique/concurrent/tablespace/options flags; index_name optional in parser but required in emitter.
[ ] Executor expects different byte layout (name/table/flags/expr/predicate blobs) than V3 payload schema.
[ ] REINDEX not implemented; ALTER INDEX limited to bloom options/active state.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_PACKAGES.md
Status: [~]
Spec: DDL Packages (non-authoritative)
Tasks:
[~] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] CREATE PACKAGE/BODY implemented via text capture; OR REPLACE supported by drop+create.
[ ] ALTER PACKAGE COMPILE not implemented.
[ ] DROP PACKAGE BODY not supported; DROP PACKAGE removes full package only.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_PROCEDURES.md
Status: [~]
Spec: DDL Procedures (non-authoritative)
Tasks:
[~] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] CREATE PROCEDURE supports params/SQL SECURITY/body; language hardcoded to SQL.
[ ] Missing spec features: LANGUAGE, SET config, SECURITY DEFINER/INVOKER syntax.
[~] ALTER PROCEDURE per spec not parsed; executor supports flags but no V3 DDL wiring.
[~] DROP PROCEDURE by name only; no signature resolution or CASCADE/RESTRICT handling.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_ROLES_AND_GROUPS.md
Status: [~]
Spec: DDL Roles and Groups (non-authoritative)
Tasks:
[~] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] CREATE USER supports password/superuser only; CREATE ROLE/GROUP name only.
[ ] Missing role options: LOGIN/NOLOGIN, PASSWORD for roles, CREATEDB/CREATEROLE, INHERIT, IN ROLE/ROLE, ADMIN.
[~] ALTER ROLE/USER per spec not parsed (executor supports rename + user password/superuser only).
[ ] Role membership GRANT/REVOKE not implemented in V3 access control path.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_ROW_LEVEL_SECURITY.md
Status: [~]
Spec: DDL Row-Level Security (non-authoritative)
Tasks:
[~] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[*] ALTER TABLE ENABLE/DISABLE/FORCE RLS implemented in parser/emitter/executor.
[~] CREATE/ALTER/DROP POLICY parsed and enforced, but emitter/executor payloads are mismatched (roles/permissive/flags/table+policy name ordering).
[ ] RESTRICTIVE policy semantics not implemented (treated as permissive).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_SCHEMAS.md
Status: [~]
Spec: DDL Schemas (non-authoritative)
Tasks:
[~] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] CREATE SCHEMA supports IF NOT EXISTS + AUTHORIZATION; no explicit PATH clause parsing.
[*] ALTER SCHEMA RENAME/OWNER/SET PATH implemented.
[*] DROP SCHEMA supports IF EXISTS and CASCADE/RESTRICT.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_SEQUENCES.md
Status: [~]
Spec: DDL Sequences (non-authoritative)
Tasks:
[~] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] CREATE SEQUENCE parsed with start/increment/min/max/cache/cycle/OWNED BY; AS <type> not parsed.
[ ] ALTER SEQUENCE not parsed (emitter/executor exist but no AST).
[ ] Emitter payload does not match executor byte layout; OWNED BY and flags not serialized.
[ ] NEXT VALUE FOR / CURRENT VALUE FOR / SET ... TO syntax not parsed in V3.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_TABLES.md
Status: [~]
Spec: DDL Tables (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] CREATE TABLE parser covers IF NOT EXISTS, columns/constraints, TABLESPACE, INHERITS, PARTITION BY, ON COMMIT, CTAS (AS SELECT); lacks WITH DATA/NO DATA.
[ ] Table/column storage parameters (WITH/ALTER ... SET (...)) not parsed or represented in AST.
[ ] Temp table semantics: AST records `temp_type`/`on_commit`, but emitter/executor ignore temp flags and on-commit behavior.
[ ] CTAS not emitted/executed in V3 (no `SBLR3_CREATE_TABLE_AS` emission).
[ ] Column GENERATED AS IDENTITY parsed but identity spec not serialized; executor ignores identity.
[ ] Column/generated STORED vs VIRTUAL not parsed; emitter maps `generated_always` to stored flag.
[ ] Column constraints PRIMARY KEY/UNIQUE/REFERENCES dropped in emitter for CREATE/ALTER TABLE ADD COLUMN.
[ ] Table constraints emitted but ignored by V3 executor in CREATE TABLE.
[ ] Column CHECK constraints mismatch: emitter uses `check_expr`, schema `COLUMN_DEF` lacks it.
[ ] ALTER TABLE OWNER TO not parsed.
[ ] DROP TABLE emitter drops only first table; IF EXISTS/CASCADE/RESTRICT flags not serialized or honored in executor.
[ ] TRUNCATE TABLE: parser ignores RESTRICT; emitter/executor ignore RESTART/CONTINUE IDENTITY and CASCADE.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_TABLE_PARTITIONING.md
Status: [~]
Spec: Table Partitioning (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] CREATE TABLE PARTITION BY parsed into AST (`partition_by`, `partition_columns`).
[ ] V3 emitter does not serialize partitioning fields for CREATE TABLE; schema `partitioning` unused.
[ ] V3 executor does not persist partitioning metadata on CREATE TABLE.
[ ] CREATE TABLE child PARTITION OF ... FOR VALUES / DEFAULT not parsed in V3.
[~] ALTER TABLE ATTACH/DETACH PARTITION parsed/emitted in V3.
[ ] V3 ATTACH PARTITION stores raw bounds string only; no validation of range/list/default constraints.
[ ] DML routing/migration semantics (INSERT/UPDATE/DELETE) and partition error codes not implemented in V3 path.
[ ] DROP TABLE does not check attached partition status before dropping.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_TEMPORAL_TABLES.md
Status: [~]
Spec: Temporal Tables (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[ ] CREATE TABLE WITH SYSTEM VERSIONING (ROW START/ROW END, PERIOD FOR SYSTEM_TIME) not parsed.
[ ] No AST fields for temporal metadata (history table, system-time period columns).
[ ] No V3 emitter/executor support for temporal table creation or history table management.
[ ] SELECT time-travel clauses (FOR SYSTEM_TIME AS OF/BETWEEN/ALL) not parsed/emitted.
[ ] ALTER TABLE ADD/DROP SYSTEM VERSIONING not parsed/executed.
[ ] DROP TABLE does not cascade to history tables.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_TRIGGERS.md
Status: [~]
Spec: DDL Triggers (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] CREATE TRIGGER (table-level DML) parsed with BEFORE/AFTER/INSTEAD OF and INSERT/UPDATE/DELETE event mask; FOR EACH ROW/STATEMENT supported.
[ ] Database-level triggers (ON CONNECT/DISCONNECT/TRANSACTION, AFTER CREATE/ALTER/DROP) not parsed.
[ ] UPDATE OF column list, REFERENCING OLD/NEW, WHEN clause, POSITION, EXECUTE proc/function name+args not parsed.
[ ] V3 emitter does not populate `when` expression field and has no structured call target.
[ ] V3 executor has no `SBLR3_CREATE_TRIGGER` or `SBLR3_DROP_TRIGGER` handling (legacy EXT_* only).
[ ] ALTER TRIGGER (ENABLE/DISABLE/RENAME) not parsed/emitted.
[ ] DROP TRIGGER lacks ON <table> parsing; IF EXISTS/CASCADE/RESTRICT flags not serialized.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_TYPES.md
Status: [~]
Spec: DDL Types (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Parser supports CREATE TYPE (ENUM/RECORD/RANGE/BASE/SHELL) + WITH DIALECT/COMPAT + COMMENT.
[ ] V3 emitter ignores type details and emits empty `SBLR3_CREATE_TYPE` payload.
[ ] No V3 executor handling for `SBLR3_CREATE_TYPE`.
[~] Parser supports ALTER TYPE actions (RENAME/SET SCHEMA/ADD VALUE/RENAME VALUE/SET options/FINALIZE).
[ ] V3 emitter uses wrong opcode (`SBLR3_ALTER_DOMAIN` vs `SBLR3_ALTER_TYPE`).
[ ] No V3 executor handling for type alteration actions.
[~] Parser supports DROP TYPE IF EXISTS and CASCADE/RESTRICT (AST).
[ ] V3 emitter maps DROP TYPE to `SBLR3_DROP_DOMAIN`, drops only first type, and ignores flags.
[ ] SHOW CREATE TYPE / EXTRACT TYPE not parsed/emitted.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_USER_DEFINED_RESOURCES.md
Status: [~]
Spec: User-Defined Resources (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[ ] CREATE LIBRARY / DROP LIBRARY not parsed or emitted in V3.
[ ] CREATE FUNCTION AS EXTERNAL NAME ... LANGUAGE ... not parsed/emitted.
[~] Separate `CREATE UDR`/`DROP UDR` exists but does not implement library binding semantics in this spec.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_VIEWS.md
Status: [~]
Spec: DDL Views (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] CREATE VIEW parsed with column list and WITH CHECK OPTION.
[ ] CREATE VIEW lacks full `WITH [LOCAL|CASCADED] CHECK OPTION` ordering support.
[ ] Temp/materialized flags captured but not emitted or executed in V3.
[ ] V3 emitter omits check option and WITH DATA flags.
[ ] V3 executor has no `SBLR3_CREATE_VIEW` handling.
[ ] ALTER VIEW not parsed/emitted.
[ ] DROP VIEW: emitter drops only first view and ignores flags; executor has no `SBLR3_DROP_VIEW` handling.
[ ] CREATE MATERIALIZED VIEW / REFRESH MATERIALIZED VIEW not parsed/emitted/executed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/EXTRACT_AND_ALTER_ELEMENT.md
Status: [~]
Spec: EXTRACT and ALTER_ELEMENT (authoritative status conflict)
Tasks:
[~] Spec header marks non-authoritative, but file claims "Status: Authoritative (V3)" - needs resolution.
[*] Parser supports EXTRACT(...) and ALTER_ELEMENT(...) with element selectors.
[~] V3 emitter outputs SBLR3_EXTRACT/SBLR3_ALTER_ELEMENT.
[ ] V3 executor has no handlers for SBLR3_EXTRACT/SBLR3_ALTER_ELEMENT; only legacy EXT_* path exists.
[ ] Element catalog semantics, temporal normalization, and error mapping not enforced in V3 path.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/README.md
Status: [~]
Spec: DDL README (non-authoritative index)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Index/overview only; no direct implementation items. References MGA/lock/GC docs and other specs.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/deployment/INSTALLATION_AND_BUILD_SPECIFICATION.md
Status: [~]
Spec: Installation and Build (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Build/install packaging spec only; no parser/engine implementation review performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/deployment/INSTALLER_FEATURES_AND_CONFIG_GENERATOR.md
Status: [~]
Spec: Installer Features and Config Generator (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Deployment/packaging spec only; no parser/engine implementation review performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/deployment/README.md
Status: [~]
Spec: Deployment README (non-authoritative index)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Index/overview only; no direct implementation items.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/deployment/SYSTEMD_SERVICE_SPECIFICATION.md
Status: [~]
Spec: Systemd Service Specification (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Deployment/service design doc only; no parser/engine implementation review performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/deployment/WINDOWS_CROSS_COMPILE_SPECIFICATION.md
Status: [~]
Spec: Windows Cross-Compile (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Build/packaging spec only; no parser/engine implementation review performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/ARCHITECTURE_CLARIFICATION.md
Status: [~]
Spec: Architecture Clarification (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Architecture guidance only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/ARCHITECTURE_GOALS.md
Status: [~]
Spec: Architecture Goals (non-authoritative roadmap)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Future-vision design doc; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/CLAUDE_DESIGN_PROPOSAL.md
Status: [~]
Spec: Claude Design Proposal (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Proposal-only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/Design_Decisions_Report.md
Status: [~]
Spec: Design Decisions Report (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Proposal/status doc; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/GARBAGE_COLLECTION_DESIGN.md
Status: [~]
Spec: Garbage Collection Design (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Design-only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/ISOLATION_LEVELS_DESIGN.md
Status: [~]
Spec: Isolation Levels Design (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Design-only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/PAGE_SIZE_PERFORMANCE_CONSIDERATIONS.md
Status: [~]
Spec: Page Size Performance Considerations (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Performance guidance only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/SWEEP_MECHANISM_DESIGN.md
Status: [~]
Spec: Sweep Mechanism Design (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Design-only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/TRANSACTION_MANAGEMENT_DESIGN.md
Status: [~]
Spec: Transaction Management Design (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Design-only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/alpha_1_05_design_synthesis.md
Status: [~]
Spec: Alpha 1.05 Design Synthesis (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Design synthesis only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/alpha_1_05_outstanding_decisions.md
Status: [~]
Spec: Alpha 1.05 Outstanding Decisions (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Proposal/decision list only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/alpha_1_05_sblr_examples.md
Status: [~]
Spec: Alpha 1.05 SBLR Examples (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Example-only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/alpha_1_05_sql_parser_design_decisions.md
Status: [~]
Spec: Alpha 1.05 SQL Parser Design Decisions (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Design-only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/btree_index_design.md
Status: [~]
Spec: B-Tree Index Design (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Design/implementation notes only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/materialized-views-foundation.md
Status: [~]
Spec: Materialized Views Foundation (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Design/future plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/design/thread_safety.md
Status: [~]
Spec: Thread Safety (design, non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Design-only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/AI_CONTEXT_MEMORY_GUIDE.md
Status: [~]
Spec: AI Context Memory Guide (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Process guidance only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/AI_PARALLEL_DEVELOPMENT_GUIDE.md
Status: [~]
Spec: AI Parallel Development Guide (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Process guidance only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/BUILD_FIX_TODO_LIST.md
Status: [~]
Spec: Build Fix TODO List (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] TODO/action list only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/BUILD_INSTRUCTIONS.md
Status: [~]
Spec: Build Instructions (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Build guidance only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/CATALOG_DESIGN_REQUIREMENTS.md
Status: [~]
Spec: Catalog Design Requirements (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[*] UUID-based references in catalog records (Schema/Table/Constraint/User/Role/Group use `owner_id`/UUIDs).
[*] Schema hierarchy: `parent_schema_id` and 18-schema bootstrap tree created.
[~] Root schema parent is zero UUID (spec says database is parent); root schema_id is DB UUID.
[*] Dependency table + CRUD (`DependencyRecord`).
[~] TOAST activation: comments/views/constraints verified; ACL/storage params/defaults/checks not fully traced.
[~] Index types: header enum lists many; on-disk IndexRecord enum is limited; full implementation not verified.
[ ] ObjectType enum values/types do not match spec list or numbering (extra types + shifted values).
[*] Procedure/Function table with selectable flag + procedure parameters table.
[~] Constraints table supports PK/UNIQUE/FK/CHECK/NOT NULL/DEFAULT/EXCLUSION; missing IN/NOT IN subquery constraint types and `in_subquery_oid`.
[*] Comments table and TOAST comment storage.
[*] Security tables for users/roles/groups/role memberships exist + bootstrap.
[~] Emulation tables exist; emulation-view creation not found.
[~] Domains table not present (handled by DomainManager), conflicting with spec.
[*] `search_path_oid` removed from SchemaRecord.
[ ] Default search_path is `public` only; spec calls for current → sys → public.
[ ] Parser absolute path semantics use no prefix; leading `.` means CURRENT, not ABSOLUTE (spec example `.root...` not supported).

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/CODING_STANDARDS.md
Status: [~]
Spec: Coding Standards (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Standards/audit guidance only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/DOCUMENTATION_CORRECTIONS_SUMMARY.md
Status: [~]
Spec: Documentation Corrections Summary (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Documentation meta-summary only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/DOCUMENTATION_REORGANIZATION_PLAN.md
Status: [~]
Spec: Documentation Reorganization Plan (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Documentation move/reorg plan only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/EXTERNAL_PARSER_GUIDE.md
Status: [~]
Spec: External Parser/Client Guide (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Guidance only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/INDEX_FACTORY_FIX_NOTES.md
Status: [~]
Spec: Index Factory Fix Notes (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Notes only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/PARSER_V2_MIGRATION.md
Status: [~]
Spec: Parser V2 Guide (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Guidance only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/PROCESS_AND_AGENTS.md
Status: [~]
Spec: Process and Agents (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Process guidance only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/TEST_COMPLIANCE_IMPLEMENTATION_SUMMARY.md
Status: [~]
Spec: Test Compliance Implementation Summary (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Summary report only; no code-level or test execution verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/TEST_SUITE_COMPLIANCE_AUDIT.md
Status: [~]
Spec: Test Suite Compliance Audit (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Audit/report only; no code-level or test execution verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/TODO.md
Status: [~]
Spec: Development TODO (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Project TODO list only; no code-level verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/Test Suite Specification.md
Status: [~]
Spec: Test Suite Specification (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[~] Specification/roadmap only; no code-level or test execution verification performed.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/UUID_ARCHITECTURE_AUDIT_AND_FIXES.md
Status: [~]
Spec: UUID Architecture Audit and Fixes (non-authoritative)
Tasks:
[*] Authoritative status check: explicitly non-authoritative and not listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
[*] B-Tree index column IDs use UUIDs (`std::vector<ID>`).
[~] Root schema UUID aligns with DB UUID; other base schema UUIDs generated per database (not fixed constants).
[~] Base schemas in system catalog entries: 8 bracketed names; differs from 18-schema bootstrap tree.
[ ] Claims about fixed system UUID constants and system catalog tables having UUIDs are not supported by current code (system_uuids.h absent; tables tracked by page IDs).
[ ] Build/test success claims not verified here.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/diagrams/component_model_diagrams.md
Status: [~]
Spec: Non-authoritative diagrams. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_diagrams_component_model_diagrams_report.md`.
Tasks:
[ ] Diagram reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/diagrams/component_responsibility_matrix.md
Status: [~]
Spec: Non-authoritative diagrams. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_diagrams_component_responsibility_matrix_report.md`.
Tasks:
[ ] Diagram reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/dml/04_DML_STATEMENTS_OVERVIEW.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_dml_04_dml_statements_overview_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/dml/DML_COPY.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_dml_copy_report.md`.
Tasks:
[ ] Requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/dml/DML_DELETE.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_dml_delete_report.md`.
Tasks:
[ ] Requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/dml/DML_INSERT.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_dml_insert_report.md`.
Tasks:
[ ] Requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/dml/DML_MERGE.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_dml_merge_report.md`.
Tasks:
[ ] Requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/dml/DML_SELECT.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_dml_select_report.md`.
Tasks:
[ ] Requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/dml/DML_UPDATE.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_dml_update_report.md`.
Tasks:
[ ] Requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/dml/DML_XML_JSON_TABLES.md
Status: [~]
Spec: Non-authoritative requirements. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_dml_xml_json_tables_report.md`.
Tasks:
[ ] Requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/dml/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_dml_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/findings/DIALECT_GAP_EXAMPLES.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_findings_dialect_gap_examples_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/findings/NO_GREY_AREAS_GATE.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_findings_no_grey_areas_gate_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/findings/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_findings_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/findings/SBLR_TYPE_OPCODE_GAPS.md
Status: [~]
Spec: Non-authoritative audit overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_findings_sblr_type_opcode_gaps_report.md`.
Tasks:
[ ] Audit overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/guides/CONCURRENCY_PATTERNS.md
Status: [~]
Spec: Non-authoritative guide. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_guides_concurrency_patterns_report.md`.
Tasks:
[ ] Guide only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/guides/ERROR_HANDLING_GUIDE.md
Status: [~]
Spec: Non-authoritative guide. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_guides_error_handling_guide_report.md`.
Tasks:
[ ] Guide only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/guides/LOCKING_PROTOCOL.md
Status: [~]
Spec: Non-authoritative guide. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_guides_locking_protocol_report.md`.
Tasks:
[ ] Guide only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/guides/PHASE_1_5_COMPLETION_GUIDE.md
Status: [~]
Spec: Non-authoritative migration guide. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_guides_phase_1_5_completion_guide_report.md`.
Tasks:
[ ] Guide only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/guides/PHASE_1_5_FINAL_STEPS.md
Status: [~]
Spec: Non-authoritative migration guide. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_guides_phase_1_5_final_steps_report.md`.
Tasks:
[ ] Guide only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/guides/PHASE_1_5_MIGRATION_GUIDE.md
Status: [~]
Spec: Non-authoritative migration guide. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_guides_phase_1_5_migration_guide_report.md`.
Tasks:
[ ] Guide only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/guides/RESOURCE_MANAGEMENT.md
Status: [~]
Spec: Non-authoritative guide. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_guides_resource_management_report.md`.
Tasks:
[ ] Guide only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/guides/SECURITY_SYSTEM_USAGE_GUIDE.md
Status: [~]
Spec: Non-authoritative guide. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_guides_security_system_usage_guide_report.md`.
Tasks:
[ ] Guide only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/AdaptiveRadixTreeIndex.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_adaptive_radix_tree_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/AdvancedIndexes.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_advanced_indexes_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/BITMAP_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_bitmap_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/BRIN_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_brin_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/BTREE_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_btree_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/BloomFilterIndex.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_bloom_filter_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/COLUMNSTORE_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_columnstore_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/CountMinSketchIndex.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_count_min_sketch_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/FSTIndex.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_fst_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/GIN_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_gin_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/GIST_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_gist_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/GeohashS2Index.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_geohash_s2_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/HASH_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_hash_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/HNSW_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_hnsw_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/HyperLogLogIndex.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_hyperloglog_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/INDEX_ARCHITECTURE.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_index_architecture_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/INDEX_COMPLETION_CHECKLIST.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_index_completion_checklist_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/INDEX_GC_PROTOCOL.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_index_gc_protocol_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_GUIDE.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_index_implementation_guide_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_REFERENCE.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_index_implementation_reference_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_index_implementation_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/IVFIndex.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_ivf_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/InvertedIndex.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_inverted_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/JSONPathIndex.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_json_path_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/LOW_LEVEL_SPECIFICATION_GIN_INDEX.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_low_level_specification_gin_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/LSMTimeSeriesIndex.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_lsm_time_series_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/LSM_TREE_ARCHITECTURE.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_lsm_tree_architecture_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/LSM_TREE_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_lsm_tree_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/LearnedIndex.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_learned_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/QuadtreeOctreeIndex.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_quadtree_octree_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/README.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_readme_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/RTREE_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_rtree_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/SPGIST_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_spgist_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/SuffixIndex.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_suffix_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/ZOrderIndex.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_zorder_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/indexes/ZoneMapsIndex.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_indexes_zone_maps_index_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/issues/ALPHA_1_2_REQUIREMENTS.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_issues_alpha_1_2_requirements_report.md`.
Tasks:
[ ] Placeholder only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/network/CONTROL_PLANE_PROTOCOL_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_network_control_plane_protocol_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/network/DIALECT_AUTH_MAPPING_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_network_dialect_auth_mapping_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_network_engine_parser_ipc_contract_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/network/NETWORK_LAYER_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_network_network_layer_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_network_network_listener_and_parser_pool_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/network/PARSER_AGENT_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_network_parser_agent_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/network/README.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_network_readme_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/network/WIRE_PROTOCOL_SPECIFICATIONS.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_network_wire_protocol_specifications_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/network/Y_VALVE_DESIGN_PRINCIPLES.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_network_y_valve_design_principles_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/network/ipc_message_table.json
Status: [~]
Spec: Non-authoritative message table. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_network_ipc_message_table_json_report.md`.
Tasks:
[ ] Message table only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/network/ipc_message_table.yaml
Status: [~]
Spec: Non-authoritative message table. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_network_ipc_message_table_yaml_report.md`.
Tasks:
[ ] Message table only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/operations/LISTENER_POOL_METRICS.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_operations_listener_pool_metrics_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/operations/MONITORING_DIALECT_MAPPINGS.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_operations_monitoring_dialect_mappings_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/operations/MONITORING_SQL_VIEWS.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_operations_monitoring_sql_views_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/operations/OID_MAPPING_STRATEGY.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_operations_oid_mapping_strategy_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/operations/PROMETHEUS_METRICS_REFERENCE.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_operations_prometheus_metrics_reference_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/operations/README.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_operations_readme_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/parser/01_SQL_DIALECT_OVERVIEW.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_01_sql_dialect_overview_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/parser/05_PSQL_PROCEDURAL_LANGUAGE.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_05_psql_procedural_language_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/parser/08_PARSER_AND_DEVELOPER_EXPERIENCE.md
Status: [~]
Spec: Non-authoritative guide. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_08_parser_and_developer_experience_report.md`.
Tasks:
[ ] Guide only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_emulated_database_parser_specification_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/parser/MYSQL_PARSER_SPECIFICATION.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_mysql_parser_specification_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/parser/POSTGRESQL_PARSER_IMPLEMENTATION.md
Status: [~]
Spec: Non-authoritative implementation guide. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_postgresql_parser_implementation_report.md`.
Tasks:
[ ] Implementation guide only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/parser/POSTGRESQL_PARSER_SPECIFICATION.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_postgresql_parser_specification_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/parser/PSQL_CURSOR_HANDLES.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_psql_cursor_handles_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/parser/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_scratchbird_sql_complete_bnf_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_CORE_LANGUAGE.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_scratchbird_sql_core_language_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/parser/SCRATCHBIRD_UNIFIED_NOSQL_EXTENSIONS.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_scratchbird_unified_nosql_extensions_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/parser/ScratchBird Master Grammar Specification v2.0.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_master_grammar_spec_v2_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/parser/ScratchBird SQL Language Specification - Master Document.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_parser_sql_language_master_document_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/EXTERNAL_AGENTS_API_AUDIT_AND_REMEDIATION_PLAN.md
Status: [~]
Spec: Non-authoritative planning doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_external_agents_api_audit_and_remediation_plan_report.md`.
Tasks:
[ ] Planning doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/EXTERNAL_AGENTS_QUICK_FIX_GUIDE.md
Status: [~]
Spec: Non-authoritative planning doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_external_agents_quick_fix_guide_report.md`.
Tasks:
[ ] Planning doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/GAP_AUDIT_REMEDIATION_PLAN_2026-02-07.md
Status: [~]
Spec: Non-authoritative planning doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_gap_audit_remediation_plan_2026-02-07_report.md`.
Tasks:
[ ] Planning doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/IMPROVEMENTS_P0_CRITICAL_PLAN.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_improvements_p0_critical_plan_report.md`.
Tasks:
[ ] Placeholder only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/IPC_ENGINE_CHANGES_SUMMARY.md
Status: [~]
Spec: Non-authoritative planning doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_ipc_engine_changes_summary_report.md`.
Tasks:
[ ] Planning doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/IPC_ENGINE_QUICK_FIX_REFERENCE.md
Status: [~]
Spec: Non-authoritative planning doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_ipc_engine_quick_fix_reference_report.md`.
Tasks:
[ ] Planning doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/IPC_ENGINE_REMEDIATION_PLAN_2026-02-06.md
Status: [~]
Spec: Non-authoritative planning doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_ipc_engine_remediation_plan_2026-02-06_report.md`.
Tasks:
[ ] Planning doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/IPC_ENGINE_VERIFICATION_SUMMARY_2026-02-06.md
Status: [~]
Spec: Non-authoritative planning doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_ipc_engine_verification_summary_2026-02-06_report.md`.
Tasks:
[ ] Planning doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/OFFLINE_TABLE_MIGRATION_DESIGN.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_offline_table_migration_design_report.md`.
Tasks:
[ ] Placeholder only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/OFFLINE_TABLE_MIGRATION_TODOS.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_offline_table_migration_todos_report.md`.
Tasks:
[ ] Placeholder only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/PHASE5_1_HEAP_PAGE_MIGRATION.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_phase5_1_heap_page_migration_report.md`.
Tasks:
[ ] Placeholder only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/PHASE5_FULL_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_phase5_full_implementation_plan_report.md`.
Tasks:
[ ] Placeholder only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/PHASE5_TASK5_1_1_HEAP_PAGE_ENUMERATION.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_phase5_task5_1_1_heap_page_enumeration_report.md`.
Tasks:
[ ] Placeholder only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/PHASE5_TASK5_1_2_PAGE_COPYING_TID_REMAPPING.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_phase5_task5_1_2_page_copying_tid_remapping_report.md`.
Tasks:
[ ] Placeholder only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/PHASE5_TASK5_1_3_TOAST_HANDLING.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_phase5_task5_1_3_toast_handling_report.md`.
Tasks:
[ ] Placeholder only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/PHASE5_TASK5_1_4_TRANSACTION_ROLLBACK.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_phase5_task5_1_4_transaction_rollback_report.md`.
Tasks:
[ ] Placeholder only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/PHASE5_TASK5_2_BTREE_TID_UPDATES.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_phase5_task5_2_btree_tid_updates_report.md`.
Tasks:
[ ] Placeholder only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/PHASE5_TASK5_3_OTHER_INDEX_TID_UPDATES.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_phase5_task5_3_other_index_tid_updates_report.md`.
Tasks:
[ ] Placeholder only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/PHASE5_TASK5_4_ONLINE_MIGRATION_ANALYSIS.md
Status: [~]
Spec: Non-authoritative placeholder. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_phase5_task5_4_online_migration_analysis_report.md`.
Tasks:
[ ] Placeholder only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/PLAN_ALPHA_COMPLETION.md
Status: [~]
Spec: Non-authoritative planning doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_plan_alpha_completion_report.md`.
Tasks:
[ ] Planning doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/TABLESPACE_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Non-authoritative planning doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_tablespace_implementation_plan_report.md`.
Tasks:
[ ] Planning doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/TRACKER_ALPHA_COMPLETION.md
Status: [~]
Spec: Non-authoritative planning doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_tracker_alpha_completion_report.md`.
Tasks:
[ ] Planning doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_CPP.md
Status: [~]
Spec: Non-authoritative archive plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_plan_driver_cpp_report.md`.
Tasks:
[ ] Archive plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_DOTNET.md
Status: [~]
Spec: Non-authoritative archive plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_plan_driver_dotnet_report.md`.
Tasks:
[ ] Archive plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_FOUNDATION_LIBSCRATCHBIRD.md
Status: [~]
Spec: Non-authoritative archive plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_plan_driver_foundation_libscratchbird_report.md`.
Tasks:
[ ] Archive plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_GOLANG.md
Status: [~]
Spec: Non-authoritative archive plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_plan_driver_golang_report.md`.
Tasks:
[ ] Archive plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_JDBC.md
Status: [~]
Spec: Non-authoritative archive plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_plan_driver_jdbc_report.md`.
Tasks:
[ ] Archive plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_NODEJS_TYPESCRIPT.md
Status: [~]
Spec: Non-authoritative archive plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_plan_driver_nodejs_typescript_report.md`.
Tasks:
[ ] Archive plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_ODBC.md
Status: [~]
Spec: Non-authoritative archive plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_plan_driver_odbc_report.md`.
Tasks:
[ ] Archive plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_PASCAL_DELPHI.md
Status: [~]
Spec: Non-authoritative archive plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_plan_driver_pascal_delphi_report.md`.
Tasks:
[ ] Archive plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_PHP.md
Status: [~]
Spec: Non-authoritative archive plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_plan_driver_php_report.md`.
Tasks:
[ ] Archive plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_PYTHON.md
Status: [~]
Spec: Non-authoritative archive plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_plan_driver_python_report.md`.
Tasks:
[ ] Archive plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_R.md
Status: [~]
Spec: Non-authoritative archive plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_plan_driver_r_report.md`.
Tasks:
[ ] Archive plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_RUBY.md
Status: [~]
Spec: Non-authoritative archive plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_plan_driver_ruby_report.md`.
Tasks:
[ ] Archive plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_RUST.md
Status: [~]
Spec: Non-authoritative archive plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_plan_driver_rust_report.md`.
Tasks:
[ ] Archive plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/TRACKER_CONTROL_PLANE_SOCKET.md
Status: [~]
Spec: Non-authoritative archive tracker. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_tracker_control_plane_socket_report.md`.
Tasks:
[ ] Archive tracker only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/TRACKER_DRIVERS.md
Status: [~]
Spec: Non-authoritative archive tracker. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_tracker_drivers_report.md`.
Tasks:
[ ] Archive tracker only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/TRACKER_DRIVER_BOOTSTRAP.md
Status: [~]
Spec: Non-authoritative archive tracker. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_tracker_driver_bootstrap_report.md`.
Tasks:
[ ] Archive tracker only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/archive/TRACKER_NETWORK_LISTENER_PARSER_BINARIES.md
Status: [~]
Spec: Non-authoritative archive tracker. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_archive_tracker_network_listener_parser_binaries_report.md`.
Tasks:
[ ] Archive tracker only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/ALPHA_CODE_TRUTH_AUDIT_2026-01-28_finished.md
Status: [~]
Spec: Non-authoritative completed plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_alpha_code_truth_audit_2026-01-28_report.md`.
Tasks:
[ ] Completed plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/ALPHA_COMPLETION_MASTER_PLAN_finished.md
Status: [~]
Spec: Non-authoritative completed plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_alpha_completion_master_plan_report.md`.
Tasks:
[ ] Completed plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/CACHE_AND_BUFFER_REMEDIATION_PLAN_finished.md
Status: [~]
Spec: Non-authoritative completed plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_cache_and_buffer_remediation_plan_report.md`.
Tasks:
[ ] Completed plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/CATALOG_EXPANSION_PLAN_finished.md
Status: [~]
Spec: Non-authoritative completed plan (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_catalog_expansion_plan_report.md`.
Tasks:
[ ] Completed plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/ENGINE_CORE_ALPHA_COMPLETION_PLAN_finished.md
Status: [~]
Spec: Non-authoritative completed plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_engine_core_alpha_completion_plan_report.md`.
Tasks:
[ ] Completed plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/OutstandingWork_Verification_2026-01-28_finished.md
Status: [~]
Spec: Non-authoritative completed plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_outstanding_work_verification_2026-01-28_report.md`.
Tasks:
[ ] Completed plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/OutstandingWork_finished.md
Status: [~]
Spec: Non-authoritative completed plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_outstanding_work_finished_report.md`.
Tasks:
[ ] Completed plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/PLAN_DRIVER_SERVER_FEATURES_ALPHA_finished.md
Status: [~]
Spec: Non-authoritative completed plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_plan_driver_server_features_alpha_report.md`.
Tasks:
[ ] Completed plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/PLAN_GIT_CONFIG_KEY_NORMALIZATION.md
Status: [~]
Spec: Non-authoritative completed plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_plan_git_config_key_normalization_report.md`.
Tasks:
[ ] Completed plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/PLAN_PYTHON_PSQL_PARITY_finished.md
Status: [~]
Spec: Non-authoritative completed plan (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_plan_python_psql_parity_report.md`.
Tasks:
[ ] Completed plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/PLAN_V2_PARSER_COMPLETION_finished.md
Status: [~]
Spec: Non-authoritative completed plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_plan_v2_parser_completion_report.md`.
Tasks:
[ ] Completed plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/RESOURCES_I18N_TIMEZONE_REMEDIATION_PLAN_finished.md
Status: [~]
Spec: Non-authoritative completed plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_resources_i18n_timezone_remediation_plan_report.md`.
Tasks:
[ ] Completed plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/SBLR_TYPE_OPCODE_REMEDIATION_PLAN.md
Status: [~]
Spec: Non-authoritative completed plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_sblr_type_opcode_remediation_plan_report.md`.
Tasks:
[ ] Completed plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/TABLESPACE_REMEDIATION_PLAN_finished.md
Status: [~]
Spec: Non-authoritative completed plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_tablespace_remediation_plan_report.md`.
Tasks:
[ ] Completed plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/TRACKER_FIREBIRD_PARSER_ALPHA.md
Status: [~]
Spec: Non-authoritative completed tracker. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_tracker_firebird_parser_alpha_report.md`.
Tasks:
[ ] Completed tracker only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/TRACKER_INDEX_SPEC_GAPS_UPDATE_2026-01-22_finished.md
Status: [~]
Spec: Non-authoritative completed tracker. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_tracker_index_spec_gaps_update_2026-01-22_report.md`.
Tasks:
[ ] Completed tracker only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/TRACKER_INDEX_SPEC_GAPS_finished.md
Status: [~]
Spec: Non-authoritative completed tracker. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_tracker_index_spec_gaps_report.md`.
Tasks:
[ ] Completed tracker only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/TRACKER_MYSQL_PARSER_ALPHA.md
Status: [~]
Spec: Non-authoritative completed tracker. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_tracker_mysql_parser_alpha_report.md`.
Tasks:
[ ] Completed tracker only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/TRACKER_OUTSTANDINGWORK_VERIFIED_2026-01-28.md
Status: [~]
Spec: Non-authoritative completed tracker. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_tracker_outstandingwork_verified_2026-01-28_report.md`.
Tasks:
[ ] Completed tracker only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/completed/TRACKER_PG_PARSER_ALPHA.md
Status: [~]
Spec: Non-authoritative completed tracker. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_completed_tracker_pg_parser_alpha_report.md`.
Tasks:
[ ] Completed tracker only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/implemented/PHASE_2_COMPLETE.md
Status: [~]
Spec: Non-authoritative completion note. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_implemented_phase_2_complete_report.md`.
Tasks:
[ ] Completion note only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/planning/implemented/PHASE_3_COMPLETE.md
Status: [~]
Spec: Non-authoritative completion note. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_planning_implemented_phase_3_complete_report.md`.
Tasks:
[ ] Completion note only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/project/reviews/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_project_reviews_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/query/PARALLEL_EXECUTION_ARCHITECTURE.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_query_parallel_execution_architecture_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/query/QUERY_OPTIMIZER_SPEC.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_query_query_optimizer_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/query/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_query_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/README.md
Status: [~]
Spec: Non-authoritative reference index. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_readme_report.md`.
Tasks:
[ ] Reference index only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/UUIDv7 Replication System Design.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_uuidv7_replication_system_design_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/2_newtransactionfeatures.pdf
Status: [~]
Spec: Non-authoritative reference PDF. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_2_newtransactionfeatures_pdf_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/FirebirdReferenceDocument.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_reference_document_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/README.md
Status: [~]
Spec: Non-authoritative reference index. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_readme_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird-50-language-reference.pdf
Status: [~]
Spec: Non-authoritative reference PDF. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_firebird_50_language_reference_pdf_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird-isql.pdf
Status: [~]
Spec: Non-authoritative reference PDF. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_firebird_isql_pdf_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/00_Preface_and_ToC.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_00_preface_and_toc_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/01_About_Firebird_5.0.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_01_about_firebird_5_0_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/02_SQL_Language_Structure.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_02_sql_language_structure_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/03_Data_Types_and_Subtypes.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_03_data_types_and_subtypes_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/04_Common_Language_Elements.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_04_common_language_elements_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/05_DDL_Statements.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_05_ddl_statements_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/06_DML_Statements.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_06_dml_statements_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/07_PSQL_Statements.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_07_psql_statements_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/08_Built_in_Scalar_Functions.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_08_built_in_scalar_functions_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/09_Aggregate_Functions.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_09_aggregate_functions_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/10_Window_Functions.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_10_window_functions_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/11_System_Packages.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_11_system_packages_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/12_Context_Variables.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_12_context_variables_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/13_Transaction_Control.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_13_transaction_control_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/14_Security.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_14_security_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/15_Management_Statements.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_15_management_statements_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_A_Supplementary_Info.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_app_a_supplementary_info_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_B_Exception_Codes.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_app_b_exception_codes_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_C_Reserved_Words.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_app_c_reserved_words_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_D_System_Tables.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_app_d_system_tables_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_E_Monitoring_Tables.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_app_e_monitoring_tables_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_F_Security_Tables.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_app_f_security_tables_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_G_Plugin_Tables.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_app_g_plugin_tables_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_H_Charsets_and_Collations.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_app_h_charsets_and_collations_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_I_License.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_app_i_license_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_J_Document_History.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_reference_firebird_docs_split_app_j_document_history_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/remote_database_udr/01-CORE_TYPES.md
Status: [~]
Spec: Non-authoritative UDR spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_remote_database_udr_01_core_types_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/remote_database_udr/02-CONNECTION_POOL.md
Status: [~]
Spec: Non-authoritative UDR spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_remote_database_udr_02_connection_pool_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/remote_database_udr/03-POSTGRESQL_ADAPTER.md
Status: [~]
Spec: Non-authoritative UDR spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_remote_database_udr_03_postgresql_adapter_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/remote_database_udr/04-MYSQL_ADAPTER.md
Status: [~]
Spec: Non-authoritative UDR spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_remote_database_udr_04_mysql_adapter_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/remote_database_udr/05-MSSQL_FIREBIRD_ADAPTERS.md
Status: [~]
Spec: Non-authoritative UDR spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_remote_database_udr_05_mssql_firebird_adapters_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/remote_database_udr/06-QUERY_EXECUTION.md
Status: [~]
Spec: Non-authoritative UDR spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_remote_database_udr_06_query_execution_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/remote_database_udr/07-SCHEMA_INTROSPECTION.md
Status: [~]
Spec: Non-authoritative UDR spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_remote_database_udr_07_schema_introspection_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/remote_database_udr/08-SQL_SYNTAX.md
Status: [~]
Spec: Non-authoritative UDR spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_remote_database_udr_08_sql_syntax_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/remote_database_udr/09-MIGRATION_WORKFLOWS.md
Status: [~]
Spec: Non-authoritative UDR spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_remote_database_udr_09_migration_workflows_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/remote_database_udr/README.md
Status: [~]
Spec: Non-authoritative UDR overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_remote_database_udr_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/sblr/Appendix_A_SBLR_BYTECODE.md
Status: [~]
Spec: Non-authoritative legacy SBLR. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_appendix_a_sblr_bytecode_report.md`.
Tasks:
[ ] Legacy/appendix only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/sblr/FIREBIRD_BLR_FIXTURES.md
Status: [~]
Spec: Non-authoritative mapping fixtures. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_firebird_blr_fixtures_report.md`.
Tasks:
[ ] Mapping fixtures only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/sblr/FIREBIRD_BLR_TO_SBLR_MAPPING.md
Status: [~]
Spec: Non-authoritative mapping. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_firebird_blr_to_sblr_mapping_report.md`.
Tasks:
[ ] Mapping only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/sblr/FIREBIRD_TRANSACTION_MODEL_SPEC.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_firebird_transaction_model_spec_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/sblr/README.md
Status: [~]
Spec: Non-authoritative overview (legacy V2). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/sblr/SBLR_DOMAIN_PAYLOADS.md
Status: [~]
Spec: Non-authoritative legacy (v2). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_domain_payloads_report.md`.
Tasks:
[ ] Legacy spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/sblr/SBLR_EXECUTION_PERFORMANCE_ALPHA.md
Status: [~]
Spec: Non-authoritative perf spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_execution_performance_alpha_report.md`.
Tasks:
[ ] Perf spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/sblr/SBLR_EXECUTION_PERFORMANCE_BETA.md
Status: [~]
Spec: Non-authoritative perf spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_execution_performance_beta_report.md`.
Tasks:
[ ] Perf spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/sblr/SBLR_EXECUTION_PERFORMANCE_RESEARCH.md
Status: [~]
Spec: Non-authoritative research. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_execution_performance_research_report.md`.
Tasks:
[ ] Research only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/sblr/SBLR_OPCODE_REGISTRY.md
Status: [~]
Spec: Non-authoritative registry. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_opcode_registry_report.md`.
Tasks:
[ ] Registry only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/sblr/SBLR_V3_BYTECODE_EXAMPLES.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_v3_bytecode_examples_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/sblr/SBLR_V3_TEST_VECTORS.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_v3_test_vectors_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/sblr/SBLR_V3_TEST_VECTORS_FULL.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_sblr_v3_test_vectors_full_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/scheduler/ALPHA_SCHEDULER_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_scheduler_alpha_scheduler_specification_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/scheduler/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_scheduler_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/scheduler/SCHEDULER_JOB_RUNNER_CANONICAL_SPEC.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_scheduler_job_runner_canonical_spec_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/ARCHITECTURE_CLARIFICATIONS.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_architecture_clarifications_report.md`.
Tasks:
[ ] Reference only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/BACKUP_AND_RESTORE.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_backup_and_restore_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/DATABASE_REGISTRY_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_database_registry_specification_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/DATABASE_REGISTRY_SPECIFICATION_CORRECTED.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_database_registry_specification_corrected_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/DRIVER_STREAMING_AND_PAGING.md
Status: [~]
Spec: Non-authoritative (conflict with internal "Authoritative" label). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_driver_streaming_and_paging_report.md`.
Tasks:
[ ] Requirements only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/INSTALLATION_AND_INITIALIZATION_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_installation_and_initialization_specification_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/MEMORY_MANAGEMENT.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_memory_management_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/PERFORMANCE_BENCHMARKS.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_performance_benchmarks_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/SCRATCHBIRD_ARCHITECTURE_OVERVIEW.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_scratchbird_architecture_overview_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/SCRATCHBIRD_CONNECTION_RECOVERY_MODEL.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_scratchbird_connection_recovery_model_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/SCRATCHBIRD_EMBEDDED_MODE_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_scratchbird_embedded_mode_specification_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/SCRATCHBIRD_SECURITY_AND_ACCESS_MODEL.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_scratchbird_security_and_access_model_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/SCRATCHBIRD_SERVER_ARCHITECTURE_CONSOLIDATED.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_scratchbird_server_architecture_consolidated_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/SERVER_ARCHITECTURE_AND_CONNECTION_LIFECYCLE.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_server_architecture_and_connection_lifecycle_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_server_lifecycle_and_startup_specification_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/server/TEMPORARY_TABLES_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_server_temporary_tables_specification_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/ALPHA_002_COMPLETE.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_alpha_002_complete_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/ALPHA_003_AUDIT_FINDINGS.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_alpha_003_audit_findings_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/ALPHA_003_GIN_PHASE_1_COMPLETE.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_alpha_003_gin_phase_1_complete_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/ALPHA_003_GIN_PHASE_2_COMPLETE.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_alpha_003_gin_phase_2_complete_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/ALPHA_003_GIN_PHASE_3_COMPLETE.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_alpha_003_gin_phase_3_complete_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/ALPHA_003_GIN_PHASE_4_COMPLETE.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_alpha_003_gin_phase_4_complete_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/ALPHA_003_PROGRESS.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_alpha_003_progress_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/CURRENT_STATUS.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_current_status_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/P0_CRITICAL_ISSUES_COMPLETION_SUMMARY.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_p0_critical_issues_completion_summary_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/PHASE_1_5_SUMMARY.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_phase_1_5_summary_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/PHASE_1_5_TID_MIGRATION_STATUS.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_phase_1_5_tid_migration_status_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/planning/TABLESPACE_IMPLEMENTATION_PLAN.md
Status: [~]
Spec: Non-authoritative planning doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_planning_tablespace_implementation_plan_report.md`.
Tasks:
[ ] Planning doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/sprints/CONSTRAINT_ENFORCEMENT_SUMMARY.md
Status: [~]
Spec: Non-authoritative sprint summary. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_sprints_constraint_enforcement_summary_report.md`.
Tasks:
[ ] Sprint summary only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/sprints/INDEX_IMPLEMENTATION_FINAL_STATUS.md
Status: [~]
Spec: Non-authoritative sprint summary. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_sprints_index_implementation_final_status_report.md`.
Tasks:
[ ] Sprint summary only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/sprints/INDEX_INTEGRATION_SPRINT3_COMPLETE.md
Status: [~]
Spec: Non-authoritative sprint summary. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_sprints_index_integration_sprint3_complete_report.md`.
Tasks:
[ ] Sprint summary only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/sprints/INDEX_INTEGRATION_SPRINT4_COMPLETE.md
Status: [~]
Spec: Non-authoritative sprint summary. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_sprints_index_integration_sprint4_complete_report.md`.
Tasks:
[ ] Sprint summary only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/sprints/SPRINT0_MGA_BUG_FIX.md
Status: [~]
Spec: Non-authoritative sprint summary. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_sprints_sprint0_mga_bug_fix_report.md`.
Tasks:
[ ] Sprint summary only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/sprints/SPRINT4_ONLINE_MIGRATION_INFRASTRUCTURE.md
Status: [~]
Spec: Non-authoritative sprint summary. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_sprints_sprint4_online_migration_infrastructure_report.md`.
Tasks:
[ ] Sprint summary only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/sprints/SPRINT5_EXECUTION_ENGINE.md
Status: [~]
Spec: Non-authoritative sprint summary. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_sprints_sprint5_execution_engine_report.md`.
Tasks:
[ ] Sprint summary only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/sprints/SPRINT6_ONLINE_MIGRATION_POLISH.md
Status: [~]
Spec: Non-authoritative sprint summary. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_sprints_sprint6_online_migration_polish_report.md`.
Tasks:
[ ] Sprint summary only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE1_TASK1_1.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase1_task1_1_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE1_TASK1_1_DATA_STRUCTURES.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase1_task1_1_data_structures_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE1_TASK1_2_5_TID_ANALYSIS.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase1_task1_2_5_tid_analysis_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE1_TASK1_4.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase1_task1_4_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE1_TASK1_5.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase1_task1_5_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE1_TASK1_6.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase1_task1_6_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE2_TASK2_1.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase2_task2_1_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE2_TASK2_2.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase2_task2_2_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE2_TASK2_4.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase2_task2_4_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE2_TASK2_5.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase2_task2_5_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE2_TASK2_6.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase2_task2_6_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE3_AUTOEXTEND.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase3_autoextend_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE3_TASK3_1_ARCHITECTURAL_DECISION.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase3_task3_1_architectural_decision_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE3_TASK3_2_ARCHITECTURAL_ANALYSIS.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase3_task3_2_architectural_analysis_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE4A_TASK4A_1_BRIN_INDEX.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase4a_task4a_1_brin_index_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE4A_TASK4A_2_HNSW_INDEX.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase4a_task4a_2_hnsw_index_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE6_ATTACH_DETACH_COMPLETE.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase6_attach_detach_complete_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/tablespace/PHASE6_ATTACH_DETACH_PARTIAL.md
Status: [~]
Spec: Non-authoritative status report. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_status_tablespace_phase6_attach_detach_partial_report.md`.
Tasks:
[ ] Status report only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/storage/EXTENDED_PAGE_SIZES.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_storage_extended_page_sizes_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/storage/HEAP_TOAST_INTEGRATION.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_storage_heap_toast_integration_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/storage/MGA_IMPLEMENTATION.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_storage_mga_implementation_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/storage/ON_DISK_FORMAT.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_storage_on_disk_format_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/storage/PAGE_TYPES_AND_LAYOUTS.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_storage_page_types_and_layouts_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/storage/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_storage_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/storage/STORAGE_ENGINE_BUFFER_POOL.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_storage_engine_buffer_pool_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/storage/STORAGE_ENGINE_MAIN.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_storage_engine_main_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/storage/STORAGE_ENGINE_PAGE_MANAGEMENT.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_storage_engine_page_management_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/storage/TABLESPACE_ONLINE_MIGRATION.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_storage_tablespace_online_migration_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/storage/TABLESPACE_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_storage_tablespace_specification_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/storage/TOAST_LOB_STORAGE.md
Status: [~]
Spec: Non-authoritative spec. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_storage_toast_lob_storage_report.md`.
Tasks:
[ ] Spec only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/testing/ALPHA3_TEST_PLAN.md
Status: [~]
Spec: Non-authoritative test plan. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_testing_alpha3_test_plan_report.md`.
Tasks:
[ ] Test plan only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/testing/DIALECT_CONFORMANCE_ASSERTIONS.md
Status: [~]
Spec: Authoritative (in inventory). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_testing_dialect_conformance_assertions_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/testing/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_testing_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/testing/test_server/DRIVER_TESTING.md
Status: [~]
Spec: Non-authoritative test doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_testing_test_server_driver_testing_report.md`.
Tasks:
[ ] Test doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/testing/test_server/GUI_INTEGRATION.md
Status: [~]
Spec: Non-authoritative test doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_testing_test_server_gui_integration_report.md`.
Tasks:
[ ] Test doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/testing/test_server/OPERATIONS.md
Status: [~]
Spec: Non-authoritative test doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_testing_test_server_operations_report.md`.
Tasks:
[ ] Test doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/testing/test_server/README.md
Status: [~]
Spec: Non-authoritative test doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_testing_test_server_readme_report.md`.
Tasks:
[ ] Test doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/testing/test_server/SECURITY_TESTING.md
Status: [~]
Spec: Non-authoritative test doc. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_testing_test_server_security_testing_report.md`.
Tasks:
[ ] Test doc only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/tools/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_tools_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/tools/SB_BACKUP_CLI_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_tools_sb_backup_cli_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/tools/SB_BUILD_AND_TEST_CLI_SPEC.md
Status: [~]
Spec: Authoritative spec; code-level verification pending. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_tools_sb_build_and_test_cli_spec_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/tools/SB_ISQL_CLI_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_tools_sb_isql_cli_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/tools/SB_SECURITY_CLI_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_tools_sb_security_cli_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/tools/SB_TOOLING_NETWORK_SPEC.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_tools_sb_tooling_network_spec_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/tools/SB_VERIFY_CLI_SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_tools_sb_verify_cli_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/transaction/07_TRANSACTION_AND_SESSION_CONTROL.md
Status: [~]
Spec: Authoritative spec; code-level verification pending. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_transaction_07_transaction_and_session_control_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/transaction/FIREBIRD_CONSTANTS_REFERENCE.md
Status: [~]
Spec: Authoritative spec; code-level verification pending. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_transaction_firebird_constants_reference_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md
Status: [~]
Spec: Authoritative spec; code-level verification pending. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_transaction_firebird_gc_sweep_glossary_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/transaction/README.md
Status: [~]
Spec: Authoritative spec; code-level verification pending. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_transaction_readme_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/transaction/TRANSACTION_DISTRIBUTED.md
Status: [~]
Spec: Authoritative spec; code-level verification pending. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_transaction_transaction_distributed_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/transaction/TRANSACTION_LOCK_MANAGER.md
Status: [~]
Spec: Authoritative spec; code-level verification pending. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_transaction_transaction_lock_manager_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/transaction/TRANSACTION_MAIN.md
Status: [~]
Spec: Authoritative spec; code-level verification pending. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_transaction_transaction_main_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/transaction/TRANSACTION_MGA_CORE.md
Status: [~]
Spec: Authoritative spec; code-level verification pending. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_transaction_transaction_mga_core_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/triggers/README.md
Status: [~]
Spec: Trigger Specifications README (non-authoritative; not in AUTHORITATIVE_SPEC_INVENTORY)
Tasks:
[~] Authoritative status check - file header says non-authoritative; not in `docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`
[~] Verify core trigger support (BEFORE/AFTER, INSERT/UPDATE/DELETE, row/statement) - parser/executor exist
[ ] Verify SQL-level context variables listed (CURRENT_USER, TG_NAME, TG_WHEN, TG_LEVEL, TG_OP, TG_TABLE_NAME, TG_TABLE_SCHEMA) - not implemented
[ ] Verify TRIGGER_CONTEXT_VARIABLES.md linkage and implementation - non-authoritative and unimplemented

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/triggers/TRIGGER_CONTEXT_VARIABLES.md
Status: [~]
Spec: Trigger Context Variables (non-authoritative; not in AUTHORITATIVE_SPEC_INVENTORY)
Tasks:
[~] Authoritative status check - file header says non-authoritative; not in `docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`
[ ] Core trigger variables: GET TRIGGER_EVENT/TIMING/LEVEL/NAME/USER/etc
[ ] NEW/OLD row access via SQL variables and IS COLUMN CHANGED / CHANGED_COLUMNS
[ ] Column introspection functions: GET COLUMN_VALUE/GET COLUMN_CHANGE/etc
[ ] Statement-level variables: TRIGGER_ROW_COUNT, AFFECTED_IDS, OLD_TABLE/NEW_TABLE
[ ] Trigger interaction variables: TRIGGER_SHARED_DATA, TRIGGER_SKIP_REMAINING, TRIGGER_CANCEL, TRIGGER_OPERATION
[ ] WHEN clause variables: TRIGGER_WHEN_RESULT
[ ] Database-level trigger variables: TRIGGER_DATABASE_EVENT, SESSION_ID, PROTOCOL_VERSION
[ ] DDL trigger variables: TRIGGER_DDL_COMMAND/OBJECT/STATEMENT
[ ] Performance/debug variables: TRIGGER_EXECUTION_TIME, STACK_TRACE, DEBUG_INFO

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/03_TYPES_AND_DOMAINS.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_03_types_and_domains_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/BINARY_LAYOUT_ANNEX.md
Status: [~]
Spec: Authoritative spec; code-level verification pending. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_binary_layout_annex_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/CANONICALIZATION_RULES.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_canonicalization_rules_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/COLLATION_RUNTIME_FORMAT.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_collation_runtime_format_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/COLLATION_TAILORING_LOADER_SPEC.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_collation_tailoring_loader_spec_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/DATA_TYPE_PERSISTENCE_AND_CASTS.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_data_type_persistence_and_casts_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/DDL_DOMAINS_COMPREHENSIVE.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_ddl_domains_comprehensive_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/I18N_CANONICAL_LISTS.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_i18n_canonical_lists_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/MULTI_GEOMETRY_TYPES_SPEC.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_multi_geometry_types_spec_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/POSTGRESQL_ARRAY_TYPE_SPEC.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_postgresql_array_type_spec_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/SBLR_TYPE_MAP.md
Status: [~]
Spec: Authoritative spec; code-level verification pending. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_sblr_type_map_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/TIMEZONE_SYSTEM_CATALOG.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_timezone_system_catalog_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/UUID_IDENTITY_COLUMNS.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_uuid_identity_columns_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/VALUE_SPEC_STORAGE_ENCODINGS.md
Status: [~]
Spec: Authoritative spec; code-level verification pending. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_value_spec_storage_encodings_report.md`.
Tasks:
[ ] Authoritative spec; code-level verification pending.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/types/character_sets_and_collations.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_types_character_sets_and_collations_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/udr/10-UDR-System-Specification.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_udr_10-udr-system-specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/udr/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_udr_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/udr/UDR_PSQL_EXTENSION_LIBRARY.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_udr_udr_psql_extension_library_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/udr_connectors/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_udr_connectors_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/udr_connectors/UDR_CONNECTOR_BASELINE.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_udr_connectors_udr_connector_baseline_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/udr_connectors/firebird_udr/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_udr_connectors_firebird_udr_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/udr_connectors/jdbc_udr/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_udr_connectors_jdbc_udr_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/udr_connectors/local_files_udr/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_udr_connectors_local_files_udr_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/udr_connectors/local_scripts_udr/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_udr_connectors_local_scripts_udr_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/udr_connectors/mssql_udr/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_udr_connectors_mssql_udr_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/udr_connectors/mysql_udr/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_udr_connectors_mysql_udr_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/udr_connectors/odbc_udr/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_udr_connectors_odbc_udr_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/udr_connectors/postgresql_udr/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_udr_connectors_postgresql_udr_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/udr_connectors/scratchbird_udr/SPECIFICATION.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_udr_connectors_scratchbird_udr_specification_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/wire_protocols/FIREBIRD_EMULATION_BEHAVIOR.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_wire_protocols_firebird_emulation_behavior_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/wire_protocols/MYSQL_EMULATION_BEHAVIOR.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_wire_protocols_mysql_emulation_behavior_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/wire_protocols/POSTGRESQL_EMULATION_BEHAVIOR.md
Status: [~]
Spec: Non-authoritative reference (doc claims authoritative/normative). Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_wire_protocols_postgresql_emulation_behavior_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/wire_protocols/README.md
Status: [~]
Spec: Non-authoritative overview. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_wire_protocols_readme_report.md`.
Tasks:
[ ] Overview only; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/wire_protocols/firebird_wire_protocol.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_wire_protocols_firebird_wire_protocol_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/wire_protocols/mysql_wire_protocol.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_wire_protocols_mysql_wire_protocol_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/wire_protocols/postgresql_wire_protocol.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_wire_protocols_postgresql_wire_protocol_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/wire_protocols/scratchbird_native_wire_protocol.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_wire_protocols_scratchbird_native_wire_protocol_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

## /home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/wire_protocols/tds_wire_protocol.md
Status: [~]
Spec: Non-authoritative reference. Report: `/home/dcalford/CliWork/ScratchBird/docs/findings/v3_wire_protocols_tds_wire_protocol_report.md`.
Tasks:
[ ] Non-authoritative reference; no code-level verification.

