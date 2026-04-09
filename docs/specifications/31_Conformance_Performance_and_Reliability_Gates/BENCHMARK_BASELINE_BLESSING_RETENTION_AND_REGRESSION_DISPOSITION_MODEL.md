Status: reconstructed_required

# Benchmark Baseline Blessing Retention and Regression Disposition Model

## Purpose

This document defines how benchmark baselines become blessed comparison points, how long they are retained, and how regressions are classified.

## Canonical Rule

A benchmark baseline becomes authoritative for regression comparison only when it is explicitly blessed. Unblessed runs are historical evidence but not default regression anchors.

## Baseline States

The canonical baseline states are:

- `CANDIDATE`
- `BLESSED`
- `SUPERSEDED`
- `RETIRED`

## Blessing Rule

Blessing shall preserve:

- run identifier
- workload and hardware profile
- reason for blessing
- actor or process blessing it
- superseded baseline if any

## Retention Rule

Blessed baselines shall be retained long enough to support:

- current release comparison
- recent regression analysis
- machine-profile comparison
- major runtime or hardware transitions

Retirement shall preserve historical metadata even if full raw artifacts age out.

## Regression Disposition Classes

Regression findings shall be classified as:

- `CONFIRMED_REGRESSION`
- `EXPECTED_VARIANT`
- `ENVIRONMENTAL_DRIFT`
- `INSUFFICIENT_EVIDENCE`
- `IMPROVEMENT`

## Comparison Rule

Only runs comparable under the baseline-comparison rules may be used to assign `CONFIRMED_REGRESSION` without additional qualification.

## Non-Guarantees

This file does not require one central baseline service. It requires explicit baseline state and regression disposition.
