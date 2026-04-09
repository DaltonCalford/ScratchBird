# Comparative Suite Contract

## Objective

For each donor engine family, prove that ScratchBird native `v3` can execute
the same tasks and validate the same behavioral outcomes as the donor suite,
without relying on emulation syntax.

## Comparative Unit

A comparative case is complete only when all of the following exist:

- donor source reference
- donor converted/emulation reference path
- native `v3` script
- normalized expected output or deterministic `.checks`
- normalized run-manifest record

## Required Metadata

Each native comparative case must record:

- `comparison_family`: `firebird`, `mysql`, or `postgresql`
- `donor_source_path`
- `donor_converted_path`
- `native_v3_case_id`
- `behavior_class`
- `translation_mode`: `mechanical`, `adapted`, or `manual`
- `result_contract`: `exact_output`, `checks`, or `negative_error`

## Required Result Schema

Both donor and native comparative summaries must emit the same top-level fields:

- `suite_family`
- `engine_family`
- `parser_mode`
- `case_count`
- `pass_count`
- `fail_count`
- `skip_count`
- `duration_ms`
- `assertion_count`
- `exact_output_count`
- `checks_count`
- `negative_case_count`
- `artifact_root`

Pairwise comparison output must also include:

- `donor_run_id`
- `native_run_id`
- `comparability_verdict`
- `schema_match`
- `case_set_match`
- `expectation_mode_match`
- `duration_ratio`

## Translation Rules

- Preserve the donor behavior under test, not donor-specific syntax.
- Reuse converted donor SQL when it captures the behavior cleanly.
- Rewrite unstable donor text comparisons into deterministic `ASSERT|...` or
  `.checks` form where possible.
- Perform donor-to-native translation once and preserve the resulting native
  `v3` SQL as a frozen on-disk artifact.
- Runtime execution may only substitute isolated namespace tokens; it may not
  regenerate SQL, re-run dialect conversion, or synthesize native statements.
- Mark intentionally unsupported or not-yet-translated donor cases explicitly;
  never silently drop them from the comparative inventory.

## Prohibited Shortcuts

- No direct claim of comparability when donor and native outputs use different
  schemas.
- No timing comparisons across mismatched case sets.
- No native case without a donor provenance link.
- No donor case counted as covered when only parser acceptance is tested and
  result semantics are missing.
- No runtime dialect translator in the comparative harness.
- No generated native SQL stored outside the frozen comparative corpus tree.
