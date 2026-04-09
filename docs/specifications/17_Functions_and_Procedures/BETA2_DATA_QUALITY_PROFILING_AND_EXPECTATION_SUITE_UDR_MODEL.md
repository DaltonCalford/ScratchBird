# Beta 2 Data Quality Profiling And Expectation Suite UDR Model

## Purpose

This document defines the data-quality UDR family for profiling, expectations,
validation batches, and drift detection.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `Great Expectations`.

## Owning package

- `sb_pkg_quality_udr`

## Dependencies

This package depends on:

- `sb_pkg_contract_udr`
- `sb_pkg_stats_udr`

## Mandatory surfaces

The package shall provide:

- dataset profiling
- expectation-suite definition
- batch validation
- validation reports
- drift and distribution-change checks for the admitted subset
- quality scorecards

## Required routine families

- `sb_quality.profile(...)`
- `sb_quality.expectation_suite_define(...)`
- `sb_quality.validate_batch(...)`
- `sb_quality.validation_report(...)`
- `sb_quality.check_drift(...)`

## Example contract

```sql
select *
from sb_quality.validate_batch(
    suite_id => 'orders_ingest_suite',
    source_query => 'select * from staging.orders_raw'
);
```

## Operational rules

1. Expectation suites must be versioned and auditable.
2. Validation results shall be stored as structured records, not only text.
3. Drift checks shall declare baseline window and comparison policy
   explicitly.

## Explicit exclusions

- open-ended external observability integrations as a baseline requirement
- remote execution of validation code
