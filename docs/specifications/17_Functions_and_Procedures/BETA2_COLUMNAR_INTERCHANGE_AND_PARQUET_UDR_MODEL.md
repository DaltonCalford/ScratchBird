# Beta 2 Columnar Interchange And Parquet UDR Model

## Purpose

This document defines the columnar interchange, Arrow memory, and Parquet
artifact surfaces used for high-throughput analytical exchange with external
tools and internal analytical UDR packages.

This group is the ScratchBird-native replacement target for the highest-value
portions of `PyArrow`.

## Owning package

- `sb_pkg_arrow_udr`

## Mandatory surfaces

The package shall provide:

- Arrow array creation and inspection
- Arrow table creation from rowsets
- Arrow table projection, filter, and batch iteration helpers
- Parquet read
- Parquet write
- zero-copy or minimal-copy interchange with admitted in-engine columnar
  buffers when representation-compatible
- explicit schema inspection and conversion helpers

## Required routine families

At minimum the following routine families shall exist:

- `sb_arrow.array_from_rowset(...)`
- `sb_arrow.table_from_query(...)`
- `sb_arrow.schema_of(...)`
- `sb_arrow.to_parquet(...)`
- `sb_arrow.from_parquet(...)`
- `sb_arrow.to_ipc(...)`
- `sb_arrow.from_ipc(...)`

## Example contract

```sql
call sb_arrow.to_parquet(
    source_query => 'select * from analytics.fact_orders',
    target_path => '/var/lib/scratchbird/export/orders.parquet'
);
```

## File and path rules

1. File-oriented routines shall obey explicit path policy and may not read or
   write arbitrary locations.
2. Allowed import/export roots shall be policy-controlled.
3. All file writes shall be auditable and shall record bytes written, row
   count, schema hash, and elapsed time.

## Integration rules

1. `sb_pkg_num_array_udr`, `sb_pkg_sci_udr`, `sb_pkg_stats_udr`, and
   `sb_pkg_ml_udr` shall be able to consume Arrow-derived arrays and tables
   without forced text conversion.
2. Representation-compatible columnar buffers shall use zero-copy or
   ownership-transfer paths where possible.
3. Representation-incompatible flows shall use explicit translated copy paths.

## Explicit exclusions

- general cloud-storage connectors as a baseline requirement
- unrestricted remote object-store access
- graphics/visualization export
