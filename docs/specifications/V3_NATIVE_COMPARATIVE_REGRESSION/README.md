# V3 Native Comparative Regression

Status: work artifact only. Excluded from git via `.git/info/exclude`.

This directory defines the one-time comparative regression effort that derives
native `v3` ScratchBird regression suites from donor-engine regression intent.
It exists because `local_work` is temporarily out of bounds and this task still
needs a full harness, tracker, and evidence model.

## Purpose

Build three native `v3` comparative regression suite families:

- `firebird_native_v3_comparative`
- `mysql_native_v3_comparative`
- `postgresql_native_v3_comparative`

Each family must preserve donor regression intent while expressing the test in
canonical ScratchBird `v3` dialect and validation style.

## Non-Negotiable Rules

- Donor regression suites remain the behavior source, not the donor dialect.
- Native comparative tests must use canonical `v3` syntax, not emulation SQL.
- Donor-to-native translation is a one-time authoring step. Comparative runs may
  only execute frozen on-disk SQL artifacts and may not perform runtime dialect
  translation or regeneration.
- Donor and native comparative outputs must share the same normalized result
  schema so timing, pass/fail, and assertion counts are directly comparable.
- Each translated case must record its donor source path and the exact behavior
  being preserved.
- This effort extends native `v3` coverage; it does not replace emulation
  compatibility suites.
- These files are temporary work artifacts and are not canonical specification
  authority.

## Primary Deliverables

- Translation contract for donor-to-`v3` comparative cases
- Shared harness contract for donor and native comparative outputs
- Three curated native comparative suites derived from the donor curated lists
- Metrics normalization that compares:
  - donor original-engine run vs native `v3` comparative run
  - current native `v3` run vs prior native `v3` comparative runs

## Scope Boundary

This effort is explicitly a practical one-time harness and evidence program. It
is not a commitment to permanent long-term in-repo planning authority.

## Translation Freeze Boundary

Allowed runtime rewriting is limited to isolated namespace substitution for
temporary object names. Any future runtime dialect translation, corpus
generation, or donor SQL rewriting is out of contract for this suite family.
