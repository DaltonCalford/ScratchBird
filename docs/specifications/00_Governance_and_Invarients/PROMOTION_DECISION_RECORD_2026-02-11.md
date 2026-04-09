# Promotion Decision Record - 2026-02-11

## Decision
Promote the canonical specification tree under `docs/specifications/00_*` through `docs/specifications/31_*` to authoritative implementation status.

## Effective Date
2026-02-11

## Promotion Version
v1.0.0

## Scope Included
- Canonical section files in `docs/specifications/00_*` through `docs/specifications/31_*`.
- Governance and inventory control files required to enforce authoritative status.

## Scope Excluded
- `docs/specifications/work/`
- `docs/specifications/library/`
- `docs/specifications/resources/`
- `docs/specifications/beta_specifications/`
- `docs/specifications/*/legacy_imports/`

## Promotion Criteria
1. File is in a canonical section directory (`00_*` to `31_*`) and listed in `AUTHORITATIVE_SPEC_INVENTORY.md`.
2. Section README status is `Authoritative - approved for implementation.`.
3. Engine/parser boundary and MGA invariants in section 00 remain unchanged.
4. Language workpack gates `P21-LANG-GATE-01..04` have passing evidence artifacts.

## Governance Notes
- Normative checklists may contain unchecked implementation items; those are execution tasks, not authority blockers.
- Any change that affects behavior, on-disk format, catalog schema, or protocol contracts requires inventory-coordinated review under section 00 governance.
