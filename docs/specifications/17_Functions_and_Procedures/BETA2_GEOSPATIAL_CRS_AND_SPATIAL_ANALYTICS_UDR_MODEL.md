# Beta 2 Geospatial CRS And Spatial Analytics UDR Model

## Purpose

This document defines the geospatial UDR family for geometry, geography,
coordinate-reference-system transforms, spatial predicates, and admitted
spatial analytics.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `PostGIS`, `GDAL`, and `PROJ`.

## Owning package

- `sb_pkg_geo_udr`

## Dependencies

This package depends on:

- `sb_pkg_num_array_udr`
- `sb_pkg_graph_udr`
- `sb_pkg_units_udr`

## Mandatory surfaces

The package shall provide:

- geometry and geography construction
- CRS-aware transform operations
- spatial predicates
- intersection, union, difference, and buffer for the admitted geometry set
- distance, area, length, centroid, envelope, and simplification helpers
- point-in-polygon, nearest-neighbor prep, and route-prep helpers
- bounded raster metadata and coordinate helpers for the admitted subset

## Required routine families

- `sb_geo.make_*`
- `sb_geo.transform(...)`
- `sb_geo.intersects(...)`
- `sb_geo.contains(...)`
- `sb_geo.buffer(...)`
- `sb_geo.distance(...)`
- `sb_geo.area(...)`
- `sb_geo.length(...)`
- `sb_geo.centroid(...)`

## Example contract

```sql
select sb_geo.distance(
    a => sb_geo.point(-79.3832, 43.6532, 'EPSG:4326'),
    b => sb_geo.point(-73.5673, 45.5017, 'EPSG:4326')
);
```

## Operational rules

1. Every geospatial object shall retain CRS identity.
2. Operations that require common CRS shall reject mismatched CRS unless an
   explicit transform policy is supplied.
3. Geometry/geography semantics must be explicit and may not be conflated.
4. Raster support is metadata-first in Beta 2; full raster processing is not
   required.

## Explicit exclusions

- map rendering
- live remote geocoding services
- unrestricted GIS toolchain embedding
