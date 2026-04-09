# Strict Implementation Workpack: Physical Page Contract Rewrite

## Mandatory Rules
1. Execute tickets in numeric order (`PP-001` to `PP-018`).
2. No ticket skipping.
3. No stage skipping.
4. No gate bypass.
5. Required evidence files per ticket:
- `RUN_MANIFEST.json`
- `SPEC_TRACEABILITY.csv`
- `TEST_RESULTS.md`
- `IMPLEMENTATION_NOTES.md`
- `CHECKSUMS.sha256`

## Stage Sequence
1. `S1`: `PP-001..PP-004` (`05` enums/header)
2. `S2`: `PP-005..PP-008` (`06` bootstrap pages)
3. `S3`: `PP-009..PP-014` (heap/TOAST/LOB)
4. `S4`: `PP-015..PP-018` (index base layout)

## Gate Sequence
1. `P-PAGE-GATE-01` after `S1`
2. `P-PAGE-GATE-02` after `S2`
3. `P-PAGE-GATE-03` after `S3`
4. `P-PAGE-GATE-04` after `S4`
5. `P-PAGE-GATE-05` final exit gate

## Stage Blockers
1. `S2` cannot start before `P-PAGE-GATE-01` passes.
2. `S3` cannot start before `P-PAGE-GATE-02` passes.
3. `S4` cannot start before `P-PAGE-GATE-03` passes.
4. Final promotion cannot start before `P-PAGE-GATE-04` passes.

## Canonical Specs
- `docs/specifications/05_Page_Taxonomy_and_Binary_Layouts/*`
- `docs/specifications/06_Fixed_Bootstrap_Page_Map/*`
- `docs/specifications/11_TOAST_and_LOB_Storage/*`
