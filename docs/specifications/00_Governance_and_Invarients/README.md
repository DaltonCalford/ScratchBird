# Section 00: Governance and Invariants

Status: current_authority

Section 00 defines the governing rules that all other canonical sections inherit.

## Current code-backed themes

- ScratchBird engine execution is owned by scratchbird_core plus scratchbird_sblr.
- SQL parsers are front-door compilers; the engine executes SBLR and internal procedures rather than raw SQL.
- Internal durable identity is UUIDv7-backed through shared ID aliases.
- Transaction visibility and recovery governance are MGA and TIP and OIT and OAT and OST based, not WAL-redo based.
- Catalog artifact metadata preserves source dialect, SBLR bytecode, and native artifact provenance.

## Core invariants

- the engine is not a SQL parser
- parser dialects lower into shared SBLR
- direct parser-side execution authority is forbidden
- durable internal identity is UUIDv7-backed
- MGA governs visibility, publication, horizon management, commit, rollback, and restart behavior
- COMMIT and ROLLBACK immediately begin the next transaction
- DDL and DML follow the same transaction rules
- WAL is not authoritative recovery truth

## Scope

Section 00 owns:
- subsystem governance and ownership boundaries
- execution-path authority
- durable identity and naming authority
- anti-WAL and MGA governance invariants
- lowering and artifact-provenance governance
- section-level proof and gate obligations for those invariants

## Non-guarantees

- this section does not claim that one machine-readable governance registry already exists in code
- this section does not claim every historical governance note remains current authority
- this section does not claim every parser or dialect surface has been exhaustively re-audited in this pass

## File Index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [CANONICAL_STATUS_VOCABULARY_AND_NORMALIZATION_RULE.md](CANONICAL_STATUS_VOCABULARY_AND_NORMALIZATION_RULE.md)
- [COMMERCIAL_GRADE_SPECIFICATION_MATURITY_REQUIREMENTS.md](COMMERCIAL_GRADE_SPECIFICATION_MATURITY_REQUIREMENTS.md)
- [CROSS_SECTION_PRECEDENCE_AND_CONTRADICTION_RESOLUTION_RULE.md](CROSS_SECTION_PRECEDENCE_AND_CONTRADICTION_RESOLUTION_RULE.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [IMPLEMENTATION_READY_REBUILD_COMPLETION_AND_FREEZE_RULE.md](IMPLEMENTATION_READY_REBUILD_COMPLETION_AND_FREEZE_RULE.md)
- [PROMOTION_DECISION_RECORD_2026-02-11.md](PROMOTION_DECISION_RECORD_2026-02-11.md)
- [RECONSTRUCTED_REQUIRED_BEHAVIOR_AND_IMPLEMENTATION_DRIFT_RULE.md](RECONSTRUCTED_REQUIRED_BEHAVIOR_AND_IMPLEMENTATION_DRIFT_RULE.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [SUBSYSTEM_CORRECTNESS_INVARIANT_CATALOG.md](SUBSYSTEM_CORRECTNESS_INVARIANT_CATALOG.md)
- [SUBSYSTEM_OWNERSHIP_AND_DEPENDENCY_BOUNDARIES.md](SUBSYSTEM_OWNERSHIP_AND_DEPENDENCY_BOUNDARIES.md)
- [SYSTEM_OBJECT_NAMING.md](SYSTEM_OBJECT_NAMING.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
- [WORK_PLAN_MANAGEMENT_STANDARD_AND_LIFECYCLE.md](WORK_PLAN_MANAGEMENT_STANDARD_AND_LIFECYCLE.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
