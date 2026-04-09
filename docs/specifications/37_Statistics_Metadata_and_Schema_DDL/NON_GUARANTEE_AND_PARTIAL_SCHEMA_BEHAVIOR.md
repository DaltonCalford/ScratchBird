# Non-Guarantee and Partial Schema Behavior

## Explicit non-guarantees

- no mature full statistics subsystem promise
- no broad cost-model-ready statistics promise
- no universal metadata cache coherence guarantee
- no broad transactional DDL promise without proof
- no online schema change guarantee outside the bounded phase model in ONLINE_SCHEMA_CHANGE_AND_BACKFILL_MODEL.md
- no donor-equivalent concurrent DDL/DML framework promise
- no widened dependency invalidation guarantee beyond explicit ownership surfaces

## Partial-status rule

Section 37 is intentionally bounded. It exists to answer statistics, metadata, and schema audit questions directly without overstating maturity or breadth.
