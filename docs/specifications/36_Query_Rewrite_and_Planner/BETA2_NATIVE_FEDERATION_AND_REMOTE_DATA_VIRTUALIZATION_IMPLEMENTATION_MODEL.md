# Beta 2 Native Federation And Remote Data Virtualization Implementation Model

## Purpose

Close the implementation gap between the existing federation canon and a
deliverable native ScratchBird remote-query product.

## Governing rules

1. Federation uses the current connector and distributed-query canon.
2. Remote objects are governed catalog objects, not ad-hoc pass-through strings.
3. Pushdown classes are explicit and explainable.
4. Remote statistics are advisory and must publish freshness.

## Required metadata

- `sb_remote_source`
  - `source_uuid`
  - `connector_uuid`
  - `source_name`
  - `capability_profile`
- `sb_remote_object_binding`
  - `binding_uuid`
  - `source_uuid`
  - `local_name`
  - `remote_locator`
  - `pushdown_policy`
- `sb_remote_stats_snapshot`
  - `snapshot_uuid`
  - `binding_uuid`
  - `rows_estimate`
  - `size_estimate`
  - `freshness_epoch`

## Implementation closure requirements

- remote schema introspection cache
- pushdown classification per operator family
- remote statistics refresh worker
- remote error normalization into ScratchBird error UUIDs
- explain output that shows pushdown and residual local work

## Pushdown classes

- `NONE`
- `FILTER_PROJECTION_ONLY`
- `PARTIAL_AGGREGATE`
- `TOP_N`
- `REMOTE_JOIN_ALLOWED`

## Refusal rules

- `FEDERATION_SOURCE_DISABLED`
- `FEDERATION_PUSHDOWN_REFUSED`
- `FEDERATION_REMOTE_SCHEMA_DRIFT`
- `FEDERATION_REMOTE_STATS_STALE`

## Example

```sql
create remote source erp via connector erp_odbc;
create foreign binding erp_customers remote 'dbo.customers';
select * from erp_customers where status = 'ACTIVE';
```

## Cross-section requirements

- section `36` owns pushdown classification and explain output
- section `17` owns connector invocation routines
- section `24` owns remote binding metadata
