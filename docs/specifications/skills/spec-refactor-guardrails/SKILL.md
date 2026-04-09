---
name: spec-refactor-guardrails
description: Use when drafting, editing, reviewing, or migrating ScratchBird specifications to enforce architecture invariants, anti-WAL constraints for Alpha, and fixed stage-gate workflow.
---

# Spec Refactor Guardrails

## Trigger
Use this skill for any work inside `docs/specifications/` during the refactor and migration from legacy specs.

## Directory Layout
- Canonical layout reference: `references/specifications_directory_layout.md`
- Keep the section tree under `docs/specifications/` aligned with that reference.
- Keep `docs/specifications/README.md` aligned with current section directories.
- Keep reference material under `docs/specifications/library/`.
- Keep audits/planning/trackers/findings under `docs/specifications/work/`.

## Non-Negotiable Invariants
- ScratchBird engine is not a SQL parser.
- Engine executes SBLR and internal procedures only.
- Internal object identity is UUID-based; names are user-layer concerns.
- Parser dialects map SQL to shared SBLR and UUID-resolved operations.
- For Alpha recovery, ScratchBird uses Firebird-style MGA semantics.
- WAL is not the Alpha recovery mechanism.
- Any WAL mention is optional extension scope only (replication/PITR), not core Alpha recovery.

## Hard Rules
- Do not replace MGA/TIP/OIT/OAT/OST semantics with WAL or PostgreSQL MVCC semantics.
- Do not introduce parser-specific behavior into core engine semantics.
- Do not mark a spec authoritative without explicit approval and inventory update.
- If a requested change conflicts with invariants, stop and request human decision.
- Do not place audits, trackers, planning docs, or findings inside numbered section directories.

## File Placement Policy
- `docs/specifications/[00-31]_*/`:
  - Canonical spec documents only.
  - Section `README.md` is mandatory.
- `docs/specifications/library/`:
  - External references (manuals, whitepapers, technical specs, environment requirements).
- `docs/specifications/work/`:
  - Audits, implementation tracks, planning artifacts, findings, migration notes.
- `docs/specifications/skills/`:
  - Guardrail skill, scripts, and layout references.

## Required Workflow Per Spec
1. Create or update section `README.md` first.
2. Write a decision record in the section before normative spec text.
3. Draft canonical spec with explicit scope, dependencies, algorithms, invariants, and test contract.
4. Add a legacy mapping table from `docs/specifications_old` source files.
5. Add acceptance tests and stage-gate checks.
6. Update section `README.md` file index by running `scripts/sync_section_readmes.sh`.
7. Run architecture drift checklist before finalizing.
8. Update root index and section links.

## README Maintenance
- Every section directory must contain a `README.md`.
- Every section `README.md` must include the auto-generated file index markers:
  - `<!-- AUTO-GENERATED:FILE-LIST:START -->`
  - `<!-- AUTO-GENERATED:FILE-LIST:END -->`
- After any add/delete/rename in a section, run:
  - `./skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`
  - Run from `docs/specifications/` or pass that path as argument.

## Git Hook Enforcement
- Hook path: `.githooks/pre-commit`
- Hook behavior: auto-runs section README sync when changes touch numbered section directories.
- Hook validation: commit fails if any numbered section is missing `README.md` or missing file-index markers.
- Hook validation: commit fails if work-area directories are placed inside numbered section directories.
- Activation: repository `core.hooksPath` must be `.githooks`.

## Architecture Drift Checklist
- Confirms MGA/TIP visibility model is preserved.
- Confirms GC horizon logic uses transaction state/locks, not WAL replay semantics.
- Confirms UUID identity model is preserved.
- Confirms parser-to-SBLR mapping boundary is preserved.
- Confirms dialect-specific behavior is adapter-level only.

## Authoritative Control
- Source of truth: `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md`.
- A file is authoritative only if listed there.
- Self-labeled "authoritative" text in a file is not sufficient.

## References To Load As Needed
- `docs/ARCHITECTURAL_LAYERS.md`
- `docs/specifications/library/README.md`
- `docs/specifications/work/README.md`
- `docs/specifications_old/transaction/README.md`
- `docs/specifications_old/transaction/TRANSACTION_MGA_CORE.md`
- `docs/specifications_old/transaction/TRANSACTION_LOCK_MANAGER.md`
- `docs/specifications_old/transaction/FIREBIRD_CONSTANTS_REFERENCE.md`
- `docs/specifications_old/transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md`
