# Beta 2 Labeled N-D Array And Coordinate UDR Model

## Purpose

This document defines the labeled N-dimensional array, coordinate-aware data,
and dataset-style analytical UDR family for ScratchBird.

This group is the ScratchBird-native replacement target for the highest-value
analytical portions of `xarray`.

## Owning package

- `sb_pkg_nd_udr`

## Dependencies

This package depends on:

- `sb_pkg_num_array_udr`
- `sb_pkg_arrow_udr`
- `sb_pkg_expr_udr`

## Mandatory surfaces

The package shall provide:

- labeled N-D array construction
- named dimensions
- coordinate labels
- dataset objects with multiple aligned labeled arrays
- alignment by dimension name and coordinate value
- broadcast and arithmetic over aligned labeled arrays
- coordinate-aware slicing, selection, and resampling for the admitted subset
- dimension reduction by name
- dataset merge and join for the admitted subset

## Required routine families

At minimum the following families shall exist:

- `sb_nd.array(...)`
- `sb_nd.dataset(...)`
- `sb_nd.align(...)`
- `sb_nd.select(...)`
- `sb_nd.reduce_*`
- `sb_nd.merge(...)`
- `sb_nd.to_arrow(...)`
- `sb_nd.from_arrow(...)`

## Example contract

```sql
select sb_nd.reduce_mean(
    dataset_obj => :cube,
    dimension_name => 'time'
);
```

## Operational rules

1. Dimension names and coordinate labels shall be first-class metadata, not
   comments or display-only strings.
2. Alignment must be explicit and deterministic.
3. Misaligned dimensions shall fail closed unless an explicit join/alignment
   policy is supplied.
4. Labeled N-D objects may be transient or stored as typed artifacts.

## Explicit exclusions

- arbitrary distributed array fabrics
- visualization or plotting surfaces
- unrestricted dask-style remote execution
