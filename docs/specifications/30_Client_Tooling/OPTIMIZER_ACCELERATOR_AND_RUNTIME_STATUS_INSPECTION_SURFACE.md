# Optimizer, Accelerator, and Runtime Status Inspection Surface

## Purpose

This document defines the required operator and driver inspection surfaces for:
- optimizer candidate bundles
- index-family runtime metrics
- resident-index state
- accelerator inventory and health
- warmup readiness and degraded fallback state

## Current code-backed authority

Current code-backed client-visible authority already includes:
- native attach-time `ParameterStatus` values for compact runtime health and
  derivative-lane summaries
- native `STATUS_RESPONSE` support for bounded key/value inspection
- ordinary result-set delivery for catalog, governance, and admin SQL surfaces

## Canonical inspection families

Client tooling shall expose the following logical inspection families:
- `SHOW INDEX FAMILY RUNTIME METRICS`
- `SHOW OPTIMIZER CANDIDATE BUNDLES`
- `SHOW RESIDENT INDEX STATUS`
- `SHOW ACCELERATOR STATUS`
- `SHOW WORKLOAD GOVERNANCE RUNTIME`
- `SHOW WARMUP READINESS`

These may be surfaced through:
- admin SQL
- native CLI commands
- driver convenience APIs
- service or support-bundle export commands

The surface may vary, but the underlying row contracts may not.

## Result-shape rule

The row contract is authoritative; the tool presentation is secondary.
A CLI table, JSON document, or driver object must preserve the same fields and
semantics as the canonical row model.

## Required tooling behavior

Tooling shall:
- preserve unknown future fields rather than silently discarding them
- preserve degraded and fallback labels
- distinguish `cold`, `warm`, and `fallback` benchmark or runtime states
- distinguish local MGA durability health from derivative and accelerator health
- allow machine-readable export for all inspection families

## Required inspection fields by family

### `SHOW INDEX FAMILY RUNTIME METRICS`

Must expose the canonical fields from section `20` for:
- declared type
- runtime family
- alias surface
- native metrics mode
- semantic contract state
- queryability state
- freshness and confidence

### `SHOW OPTIMIZER CANDIDATE BUNDLES`

Must expose:
- relation and statement identity
- candidate budget
- chosen family signature
- rejected count
- rejection reasons
- ordering, text-ranking, parameterization, and ANN-order requirements

### `SHOW RESIDENT INDEX STATUS`

Must expose:
- index identity
- resident class
- residency location
- warmup policy
- warmup state
- dirty refresh pending
- last load and refresh times
- last eviction reason

### `SHOW ACCELERATOR STATUS`

Must expose:
- device identity
- driver and runtime versions
- health state
- memory totals and pressure
- active and queued admissions
- resident index count
- fallback counts

### `SHOW WORKLOAD GOVERNANCE RUNTIME`

Must expose:
- class and policy identity
- queue depth
- active and queued queries
- resource tag
- accelerator profile
- fallback policy

### `SHOW WARMUP READINESS`

Must expose:
- readiness class
- current state
- blocking reason
- last transition time
- whether the current benchmark or operator-visible runtime is `cold`, `warm`,
  `degraded`, or `fallback`

## Required driver API posture

Drivers shall provide either:
- typed result models for these families
- or schema-preserving generic row access

Drivers may not collapse these inspection surfaces into free-form strings only.

## Required reconstructed behavior

The rebuild requires explicit tooling support for:
- resident-index readiness inspection
- accelerator health and device inventory inspection
- optimizer bundle rejection inspection
- machine-readable benchmark warmup and fallback labels

These are required canon even where the current driver fleet is only partially
aligned.

## Non-guarantees

Tooling does not guarantee:
- that every protocol emulation exposes every native inspection family
- that compact attach-time status alone is sufficient for deep diagnosis
- that degraded accelerator lanes remain queryable through emulated protocols

Native and admin-SQL surfaces remain the authoritative inspection contract.
