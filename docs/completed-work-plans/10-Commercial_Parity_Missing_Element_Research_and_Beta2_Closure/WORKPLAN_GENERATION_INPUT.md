# Workplan Generation Input

## Intent

Drive a bounded research-to-canon program that closes the currently known
commercial-parity gaps in the ScratchBird spec tree while preserving MGA-first
architecture.

## Required Input Read Order

1. `README.md`
2. `CANONICAL_GAP_REGISTER.md`
3. `DEFINITIVE_SPECSET_INDEX.md`
4. `CODE_AREA_OWNERSHIP_MAP.md`
5. `EVIDENCE_EXPECTATIONS.md`
6. `GATE_EVIDENCE_MATRIX.csv`
7. `RISK_DECISION_LOG.md`

## Execution Rules

1. Work one gap family at a time.
2. For each gap, complete the research lane before drafting canonical specs.
3. Use local ScratchBird specs and code first, then local donor clones, then
   web research.
4. Prefer primary sources: standards bodies, official vendor docs, official
   source repos, and whitepapers from the original authors.
5. Download every web source used into the canonical reference tree.
6. Emit a research packet under `docs/reference/reference_library/` and update
   the relevant workspace-library indexes.
7. Only after the research packet is complete, create or revise the canonical
   Beta 2 specs.
8. New canonical specs must include enough detail that implementation can be
   delegated to a low-reasoning agent without design guessing.
9. Every design choice must be checked against MGA invariants:
   - no WAL-authoritative core recovery truth
   - no donor-log replay as primary correctness model
   - no parser or client semantics leaking into core engine authority
10. When a numbered section changes, sync section READMEs with:
    `~/.codex/skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh /home/dcalford/CliWork/ScratchBird/docs/specifications`
11. Update `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md` for every new
    canonical file.

## Required Output From Execution Work

- one completed research packet per gap
- updated reference indexes and manifests
- one or more Beta 2 canonical specs per gap
- updated section `README.md` files
- updated `AUTHORITATIVE_SPEC_INVENTORY.md`
- updated `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`, `MASTER_TRACKER.md`,
  `MASTER_TRACKER.csv`, and `RISK_DECISION_LOG.md`

## Non-Negotiable Constraints

- every created missing-element closure spec is `Beta 2`
- all designs must support and leverage MGA rather than bypass it
- work-plan authority uses stable `path + unique_search_key` references, not
  line-number-only anchors
- no unsupported commercial feature may be claimed as already implemented
- if research shows a listed gap decomposes into multiple canonical specs, that
  split is allowed, but the parent gap ticket remains the control point
