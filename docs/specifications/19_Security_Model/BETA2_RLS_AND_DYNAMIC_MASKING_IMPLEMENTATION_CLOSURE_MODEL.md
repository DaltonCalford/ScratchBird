# Beta 2 RLS And Dynamic Masking Implementation Closure Model

## Purpose

Close the implementation gap in the current row-level security and masking
canon by freezing policy ordering, cache invalidation, and mixed-policy rules.

## Governing rules

1. RLS filtering occurs before masking.
2. Policy cache invalidation is epoch-based.
3. Forced RLS removes privileged bypass before masking decisions are evaluated.
4. Masking never grants access to rows filtered by RLS.

## Required closure items

- policy evaluation cache key
- epoch invalidation rules
- mixed `USING` and `WITH CHECK` failure behavior
- masking plus column-grant conflict rules
- explain or diagnostics markers for policy participation

## Refusal rules

- `SECURITY_POLICY_CACHE_STALE`
- `RLS_WITH_CHECK_REFUSED`
- `MASKING_POLICY_CONFLICT`
- `COLUMN_DISCLOSURE_REFUSED`

## Metrics

- RLS rows filtered
- masking applications
- privileged bypass count
- policy cache invalidations

## Example

```sql
alter table payroll force row level security;
create masking policy pii_full_mask on domain ssn_domain;
```

## Cross-section requirements

- section `19` owns policy order and cache invalidation
- section `31` owns policy-conformance testing
