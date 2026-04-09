# Low-Capability Implementability Sweep (Pass 2)

Date: 2026-02-11
Scope: `docs/specifications/00_*` through `docs/specifications/31_*` (canonical tree)

## Objective
Confirm canonical specifications are explicit enough for a low-capability, non-reasoning implementation model without external inference.

## Checks Performed
- Placeholder markers scan (`TBD/TODO/FIXME/XXX`) across canonical sections.
- Open-questions normalization check (`## Open Questions` blocks must be `- None.`).
- Internal reference existence check for explicit `docs/specifications/...*.md` and `*.sql` paths.
- Normative-language hardening for residual `MAY/SHOULD` wording in section 24 and section 28 high-risk protocol files.
- Section README sync via:
  - `docs/specifications/skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`

## Corrections Applied In This Pass
- Hardened visibility rule in:
  - `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SYSTEM_OBJECT_VISIBILITY_AND_INSTALLATION.md`
- Hardened Bolt error-path rule in:
  - `docs/specifications/28_Parser_Implementations/NORMATIVE_WIRE_PROTOCOL_NEO4J_BOLT_CHECKLIST.md`
- Hardened Firebird protocol-version acceptance rule in:
  - `docs/specifications/28_Parser_Implementations/NORMATIVE_WIRE_PROTOCOL_FIREBIRD_CHECKLIST.md`
- Created missing evidence artifact templates referenced by canonical specs under:
  - `docs/specifications/work/conformance/...`
  - `docs/specifications/work/migration_notes/LEGACY_SCHEMA_TREE_DELTA.md`

## Tracker State
Source: `docs/specifications/work/audits/LOW_CAPABILITY_IMPLEMENTABILITY_TRACKER_2026-02-11.csv`

- `ready=456`
- `pending=0`
- `open_questions_non_none=0`
- `placeholder_markers=0`
- `dead_refs=0`

## Notes
- Residual lexical "ambiguity" counters in the tracker are non-blocking wording flags, not unresolved design gaps.
- All canonical section entries (`00` to `31`) are marked `ready` after this pass.
