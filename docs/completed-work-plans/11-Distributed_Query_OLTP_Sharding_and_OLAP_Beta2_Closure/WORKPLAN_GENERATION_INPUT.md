# Workplan Generation Input

## Intent

Drive a bounded research-to-canon program that closes the current Beta 2 gaps
for distributed query execution, high-performance OLTP, sharding, and OLAP or
cube support while preserving MGA-first architecture.

## Required input read order

1. `README.md`
2. `CANONICAL_GAP_REGISTER.md`
3. `DEFINITIVE_SPECSET_INDEX.md`
4. `CODE_AREA_OWNERSHIP_MAP.md`
5. `EVIDENCE_EXPECTATIONS.md`
6. `GATE_EVIDENCE_MATRIX.csv`
7. `RISK_DECISION_LOG.md`

## Execution rules

1. Work one topic family at a time.
2. Complete the research lane before drafting canonical specs for that family.
3. Use local ScratchBird specs and code first, then local donor and reference
   material, then web research.
4. Prefer primary sources: standards bodies, official vendor docs, official
   source repos, and original whitepapers.
5. Download every web source used into the canonical reference tree.
6. Emit a research packet under `docs/reference/reference_library/` and update
   the relevant workspace-library indexes.
7. Only after the research packet is complete, create or revise the canonical
   Beta 2 specs.
8. New canonical specs must be detailed enough for low-reasoning
   implementation.
9. Every design choice must be checked against MGA invariants:
   - no WAL-authoritative recovery truth
   - no donor-log replay as primary correctness model
   - no parser or client semantics leaking into core engine authority
10. When a numbered section changes, sync section READMEs with:
    `~/.codex/skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh /home/dcalford/CliWork/ScratchBird/docs/specifications`
11. Update `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md` for every new
    canonical file.

## Required output from execution work

- one completed research packet per topic family
- updated reference indexes and manifests
- one or more Beta 2 canonical specs per topic family
- updated section `README.md` files
- updated `AUTHORITATIVE_SPEC_INVENTORY.md`
- updated `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`, `MASTER_TRACKER.md`,
  `MASTER_TRACKER.csv`, and `RISK_DECISION_LOG.md`

## Non-negotiable constraints

- every created closure spec in this package is `Beta 2`
- all designs must leverage current ScratchBird substrate where it exists
- work-plan authority uses stable `path + unique_search_key` references, not
  line-number-only anchors
- no topic may be marked implemented or commercially ready without maintained
  proof
