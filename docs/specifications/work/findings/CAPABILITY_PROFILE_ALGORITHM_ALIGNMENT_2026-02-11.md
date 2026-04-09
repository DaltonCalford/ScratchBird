# Capability Profile Algorithm Alignment - 2026-02-11

## Scope
Section-28 hardening pass to make feature gating fully deterministic for low-capability implementation agents.

## Changes Applied

### 1. Added canonical build algorithm doc
New file:
- `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_BUILD_ALGORITHM.md`

This document now defines:
- required input sources
- exact output cardinality formula (`9 * feature_count`)
- required-engines token normalization rules
- group and alias expansion tables
- decision precedence and generation algorithm
- remap override application order
- precedence-rank formula
- deterministic build-failure conditions

### 2. Integrated algorithm into canonical capability entry spec
Updated:
- `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_ENTRIES_CANONICAL.md`

Added:
- canonical build reference
- explicit normalization step
- explicit row-count requirement (`1404` with current `156` features)
- additional validation requirement for unknown required-engine tokens

### 3. Updated section outline and test contract
Updated:
- `docs/specifications/28_Parser_Implementations/SPEC_OUTLINE.md`
- `docs/specifications/28_Parser_Implementations/TEST_CONTRACT.md`
- `docs/specifications/28_Parser_Implementations/README.md`

Added tests:
- full cross-product cardinality check
- token normalization rejection for unknown tokens
- normalized required-target expansion conformance

### 4. Added deterministic row serialization examples
New file:
- `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_ROW_SERIALIZATION_EXAMPLES.md`

This adds:
- canonical field order for checksum generation
- checksum formula
- precedence-rank formula
- representative generated rows for `native` and `postgresql` profiles
- canonical SQL insert shape for capability rows

Additional test clauses added for:
- row serialization and checksum conformance
- sample row conformance for native/postgresql

## Validation
1. Phrase inventory check:
- all unique `required_engines` phrases currently present in section-21 matrix are covered by the phrase table used by the algorithm.
2. Placeholder scan in section-28 canonical docs:
- no canonical placeholder markers; one expected README hit from legacy link path (`TODO.md`).
3. README index sync executed.

## Related Alignment Context
- Section-21 and section-22 feature parity remains exact:
  - `156` feature keys in section 21
  - `156` feature keys in section 22
  - no missing and no extra keys

## Next Continuation Target
- Build the explicit section-28 profile-entry generation examples (sample output rows) for one full target profile (`native`) and one emulated profile (`postgresql`) so low-capability implementation can produce identical catalog rows.

## Follow-up Correction
### 5. Decision projection parity fix
Updated:
- `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_DECISION_TABLE.csv`
- `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_BUILD_ALGORITHM.md`
- `docs/specifications/28_Parser_Implementations/TEST_CONTRACT.md`
- `docs/specifications/28_Parser_Implementations/README.md`
- `docs/specifications/28_Parser_Implementations/SPEC_OUTLINE.md`

Applied:
- Added missing `F_MONGO_FIND` row to decision projection CSV.
- Corrected `FG_MONGO` precedence ordering to keep deterministic rank sequence.
- Declared decision-projection cardinality and section-21 parity as mandatory checks.
- Added explicit tests for projection row count and feature parity.
