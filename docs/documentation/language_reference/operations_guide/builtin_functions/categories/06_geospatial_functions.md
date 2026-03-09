# Geospatial Functions

[Categories README](./README.md)

## Synopsis

Functions for working with spatial data and geometries.

## Geometry Types

| Type | Description | Example |
|------|-------------|---------|
| `POINT` | Single point | `POINT(0 0)` |
| `LINESTRING` | Line segment | `LINESTRING(0 0, 1 1, 2 2)` |
| `POLYGON` | Closed polygon | `POLYGON((0 0, 1 0, 1 1, 0 1, 0 0))` |
| `MULTIPOINT` | Multiple points | `MULTIPOINT(0 0, 1 1)` |
| `MULTILINESTRING` | Multiple lines | - |
| `MULTIPOLYGON` | Multiple polygons | - |
| `GEOMETRYCOLLECTION` | Mixed collection | - |

## Creation Functions

| Function | Description | Example |
|----------|-------------|---------|
| `ST_MakePoint(x, y)` | Create point | `ST_MakePoint(10.5, 20.3)` |
| `ST_MakeLine(geom1, geom2)` | Create line | `ST_MakeLine(pt1, pt2)` |
| `ST_MakePolygon(linestring)` | Create polygon | `ST_MakePolygon(ring)` |
| `ST_GeomFromText(wkt)` | Parse WKT | `ST_GeomFromText('POINT(0 0)')` |
| `ST_GeomFromGeoJSON(json)` | Parse GeoJSON | `ST_GeomFromGeoJSON('{"type":"Point"}')` |

## Measurement Functions

| Function | Description | Units |
|----------|-------------|-------|
| `ST_Distance(geom1, geom2)` | Distance between geometries | Coordinate system units |
| `ST_Length(geom)` | Length of line | Coordinate units |
| `ST_Area(geom)` | Area of polygon | Square coordinate units |
| `ST_Perimeter(geom)` | Perimeter | Coordinate units |

## Relationship Functions

| Function | Description |
|----------|-------------|
| `ST_Contains(geom1, geom2)` | Returns true if geom1 contains geom2 |
| `ST_Within(geom1, geom2)` | Returns true if geom1 is within geom2 |
| `ST_Intersects(geom1, geom2)` | Geometries intersect |
| `ST_Disjoint(geom1, geom2)` | Geometries don't intersect |
| `ST_Touches(geom1, geom2)` | Boundaries touch |
| `ST_Overlaps(geom1, geom2)` | Geometries overlap |
| `ST_Crosses(geom1, geom2)` | Lines cross |
| `ST_Equals(geom1, geom2)` | Geometries are equal |

## Transformation Functions

| Function | Description |
|----------|-------------|
| `ST_Transform(geom, srid)` | Reproject to different SRID |
| `ST_SetSRID(geom, srid)` | Set coordinate system |
| `ST_SRID(geom)` | Get coordinate system |

## Examples

```sql
-- Create point
SELECT ST_GeomFromText('POINT(-122.4194 37.7749)', 4326);

-- Distance calculation
SELECT ST_Distance(
    ST_MakePoint(0, 0),
    ST_MakePoint(1, 1)
);

-- Find points within radius
SELECT * FROM places
WHERE ST_DWithin(
    location,
    ST_MakePoint(-122.4194, 37.7749),
    1000  -- meters
);

-- Containment query
SELECT * FROM regions
WHERE ST_Contains(boundary, ST_MakePoint(10, 20));

-- Create spatial index
CREATE INDEX idx_places_location ON places USING RTREE (location);
```

## Spatial Indexing

```sql
-- RTREE index for bounding box queries
CREATE INDEX idx_geom ON table_name USING RTREE (geom_column);

-- GIST index for complex geometries
CREATE INDEX idx_geom_gist ON table_name USING GIST (geom_column);
```
