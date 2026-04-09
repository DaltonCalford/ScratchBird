# Targeted Sweep: Parser SQL Gap Closure (2026-02-11)

## Scope
- Canonical specs under `docs/specifications/` only.
- Excluded: `legacy_imports`, `source_copies`, `work`, `library`, `skills`, `beta_specifications`.
- Focus: missing native SQL language for already-defined canonical behaviors.

## Gap Summary Before Patch
1. Configuration SQL in section 21 lacked explicit `CONFIG HISTORY` and `RELOAD CONFIG` feature entries.
2. Domain SQL in section 21 lacked explicit native forms for `AS RECORD`, `AS ENUM`, `AS SET OF`, `AS RANGE OF`, and `ALTER DOMAIN`.
3. System visibility SQL was not defined as a deterministic parser surface.
4. Index management SQL verbs existed in section 18 but were not fully represented in section 21 feature matrix.
5. Text search pipeline was detailed in section 18, but SQL DDL/load statements were not fully defined in section 21.
6. Cluster/admission/alert/healing/job/shard/cube/security behavior had canonical requirements but no complete native SQL surface in section 21.
7. Operator handling in parser AST lacked explicit operator-node contract tied to section 13 coercion rules.

## Applied Fixes
- Added `docs/specifications/21_V3_Dialect_Surface/NATIVE_INFRASTRUCTURE_SQL.md` with deterministic SQL contracts for:
  - config/system visibility,
  - domain variants,
  - index management,
  - text search DDL/data loading,
  - cluster routing/admission/state,
  - alerting/healing,
  - scheduler/job controls,
  - sharding/cube controls,
  - security/encryption/certificate controls.
- Expanded `docs/specifications/21_V3_Dialect_Surface/NATIVE_SUPERSET_COMPATIBILITY_MATRIX.md` with explicit feature keys for the above surfaces.
- Expanded `docs/specifications/21_V3_Dialect_Surface/NATIVE_PARSER_FEATURE_FAMILIES.md`:
  - added `FG_TEXT_SEARCH`, `FG_CLUSTER_CONTROL`, `FG_JOB_CONTROL`, `FG_SECURITY_ADMIN`,
  - added explicit expression/operator AST contract and deterministic operator resolution rules.
- Updated `docs/specifications/21_V3_Dialect_Surface/NATIVE_SQL_SURFACE.md` to reference the new infrastructure SQL contracts.
- Updated `docs/specifications/21_V3_Dialect_Surface/TEST_CONTRACT.md` and `docs/specifications/28_Parser_Implementations/TEST_CONTRACT.md` to include coverage requirements for the added parser surfaces.

## Second Pass Fixes (Post-Closure)
1. Added dedicated config feature keys in matrix:
   - `F_CONFIG_SHOW`, `F_CONFIG_SET`, `F_CONFIG_RESET`.
2. Added dedicated index feature keys in matrix:
   - `F_IDX_ANALYZE`, `F_IDX_SHOW_HEALTH`.
3. Added dedicated cluster/alert/healing show feature keys in matrix:
   - `F_CLUSTER_SHOW_ROUTING_PLAN`
   - `F_CLUSTER_SHOW_ADMISSION_STATUS`
   - `F_ALERT_SHOW`
   - `F_HEALING_SHOW_RUNS`
4. Extended parser family syntax contracts for those commands in `NATIVE_PARSER_FEATURE_FAMILIES.md`.
5. Removed an ambiguous partitioning phrase in `NATIVE_SQL_SURFACE.md` and replaced it with deterministic gate behavior (`FEATURE_DISABLED` when not enabled).
6. Tightened normative language:
   - `DECISION_RECORD.md` now requires native full-matrix exposure and strict profile gating for emulated parsers.
   - `SYSTEM_COLUMNS.md` now requires profile-driven exposure behavior for emulated parsers.

## Residual Findings (Canonical Tree)
1. `## Open Questions` markers remain in canonical docs: `81` occurrences.
   - These are design placeholders and represent unresolved decisions, not parser-language ambiguity by themselves.
2. `TODO/TBD/XXX` markers in canonical tree: `1` occurrence.
   - Location: `docs/specifications/28_Parser_Implementations/README.md` line referencing a legacy-import file path containing `TODO.md`.
   - This is a legacy-import index reference, not canonical requirement text.
3. Deferred SBLR mapping statement count in sections 21/22: `1` occurrence.
   - `docs/specifications/21_V3_Dialect_Surface/NATIVE_DIAGNOSTICS_SQL.md` defers opcode identifiers until finalized surface lock.

## Sweep Verdict
- Section 21 now has explicit native SQL forms for the previously uncovered canonical feature areas.
- Remaining implementation ambiguity is concentrated in cross-section open-question placeholders and deferred opcode numbering, not in parser SQL statement coverage.
- Strict check in `21_V3_Dialect_Surface` + `25_Runtime_Modes` now returns zero `XXX/TODO/TBD/if implemented` markers in canonical content files (excluding legacy imports).
