# Execution Cache, Plan, and JIT Observability

Status: current_authority_with_reconstructed_expansion

## 1. Scope

This file defines the public observability lanes for:

- generic statement and result caches
- VNext plan-cache activity
- translation cache activity
- JIT/native artifact execution and performance

These lanes are related but are not interchangeable.

## 2. Public cache telemetry currently present

### 2.1 Generic cache counters

Current telemetry registration includes the following counters:

- `scratchbird_statement_cache_hits_total`
- `scratchbird_statement_cache_misses_total`
- `scratchbird_statement_cache_evictions_total`
- `scratchbird_result_cache_hits_total`
- `scratchbird_result_cache_misses_total`
- `scratchbird_result_cache_evictions_total`
- translation-cache counters

These counters are public operator metrics.

### 2.2 System catalog visibility

The system catalog also exposes cache-family rows for:

- `statement_cache`
- `result_cache`
- `translation_cache`

These are operator-facing summary lanes and must remain consistent with the registered telemetry families.

## 3. Plan-cache observability

### 3.1 Public event counter

Current public plan-cache activity is emitted through the VNext optimizer event model, not through the generic cache counter families.

Metric name:

- `scratchbird_vnext_optimizer_events_total`

Required labels:

- `event`
- `outcome`
- `code`

### 3.2 Current violation code already used

Current code already uses cache-related optimizer event code:

- `UDR_1511`

### 3.3 Interpretation rule

Operators must not diagnose VNext plan-cache health from the generic statement-cache or result-cache counters. The optimizer event stream is the authoritative public lane for VNext plan-cache anomalies and invalidation-related outcomes.

## 4. JIT and native artifact observability

### 4.1 Runtime performance snapshots

Current JIT runtime code already tracks performance snapshots including:

- compile queue depth
- compile successes and failures
- hotness counts
- native execution count
- native execution CPU microseconds
- fallback count
- load-failure count
- retired-unusable-artifact count

### 4.2 Catalog-backed artifact stats

Per-artifact and per-object native execution stats are also recorded through the catalog artifact-stats mutation path.

### 4.3 Required interpretation rule

JIT observability is a third lane beside generic cache telemetry and VNext optimizer events. JIT artifact performance must not be flattened into generic cache-hit language.

## 5. Result-cache observability caveat

Current code increments the generic result-cache counters from both:

- executor-side `sblr::QueryResultCache`
- pool-side `pool::DatabaseResultCache`

This means the public generic result-cache metrics currently combine two different caches.

Required operator rule:

- treat generic result-cache counters as combined result-cache activity
- use subsystem-local statistics and configuration context to determine whether behavior came from the engine cache or the pool cache
- do not assume generic result-cache counters describe one single cache family

## 6. Minimum operator outputs required

A conforming observability surface must let an operator distinguish:

- statement-cache activity
- combined public result-cache activity
- plan-cache event activity
- translation-cache activity
- JIT compile/execution/fallback activity
- catalog-backed artifact performance and retirement state

## 7. Non-authority and rejection rules

The following claims are incorrect:

- generic result-cache counters describe only one cache implementation
- VNext plan-cache activity is fully represented by statement-cache counters
- JIT native-artifact health can be inferred from generic cache-hit counters alone
- system catalog cache rows eliminate the need for optimizer event interpretation

## 8. Implementation requirements

A conforming implementation must:

- preserve the distinct public lane for VNext optimizer events
- preserve the distinct JIT/runtime and artifact-stats lane
- keep system catalog cache summaries aligned with registered cache families
- document the current result-cache counter conflation until public counters are split
