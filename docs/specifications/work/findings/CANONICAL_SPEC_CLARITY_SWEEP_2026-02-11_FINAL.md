# Canonical Spec Clarity Sweep (Final) - 2026-02-11

## Scope
- Tree scanned: `docs/specifications/[0-3][0-9]_*`
- Excluded: `legacy_imports`, `source_copies`
- Focus: ambiguity, missing decisions, placeholder drift, and implementation-readiness for low-capability non-reasoning AI.

## Commands Used
1. Placeholder scan (strict, non-README):
- `rg -n '\bTBD\b|\bTODO\b|\bXXX\b|\bFIXME\b' docs/specifications/[0-3][0-9]_* --glob '!**/legacy_imports/**' --glob '!**/source_copies/**' --glob '!**/README.md'`
2. Placeholder scan (including README):
- `rg -n '\bTBD\b|\bTODO\b|\bXXX\b|\bFIXME\b' docs/specifications/[0-3][0-9]_* --glob '!**/legacy_imports/**' --glob '!**/source_copies/**'`
3. Open-questions resolution check:
- first non-empty line after each `## Open Questions` / `## Open Items` heading across canonical section files.
4. Ambiguity phrase scan:
- `rg -n 'Open Questions For Review|implementation-defined|to be decided|deferred until requirement closure' docs/specifications/[0-3][0-9]_* --glob '!**/legacy_imports/**' --glob '!**/source_copies/**'`

## Results
- Canonical markdown files scanned: `369`
- Placeholder hits (strict, non-README): `0`
- Placeholder hits (including README): `1`
- Unresolved `Open Questions` / `Open Items` blocks: `0`
- Residual ambiguity phrases: `0`

## Single Remaining Placeholder Hit (Expected, Non-Canonical)
1. `docs/specifications/28_Parser_Implementations/README.md`
- Contains legacy-import link path with filename `TODO.md`.
- This is a provenance link only, not a canonical requirement gap.

## Corrections Applied During This Sweep
1. Removed false-positive placeholder wording in:
- `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/TEST_CONTRACT.md`
2. Resolved and normalized configuration mutability/hot-reload wording:
- `docs/specifications/01_Configuration_Subsystem/DECISION_RECORD.md`
- `docs/specifications/01_Configuration_Subsystem/CONFIG_CATALOG_AND_BOOTSTRAP.md`
3. Converted previously open review points into explicit resolved decisions:
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_OBJECT_SCHEMA_BRANCH_ASSIGNMENT.md`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SCHEMA_BOOTSTRAP_ORDER_AND_INVARIANTS.md`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SCHEMA_TREE_CANONICAL.md`
- `docs/specifications/28_Parser_Implementations/SCHEMA_VISIBILITY_AND_TRANSLATION_MATRIX.md`
4. Replaced implementation-defined corpus root with fixed canonical path:
- `docs/specifications/28_Parser_Implementations/PARSER_CONFORMANCE_CORPUS_INDEX.md`
5. Removed open-ended opcode wording in section 22/23 decision framing:
- `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/DECISION_RECORD.md`
- `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/SPEC_OUTLINE.md`
- `docs/specifications/23_SBLR_VM_Compiler_and_Executor/DECISION_RECORD.md`
6. README index sync run after updates:
- `docs/specifications/skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`

## Readiness Statement
Canonical section documents are now structurally clean for the next phase (`admin` / `ddl` / `dml` / `psql` / `tsql` language definition) with no unresolved open-question blocks and no canonical placeholder markers in non-README files.
