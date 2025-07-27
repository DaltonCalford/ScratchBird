-- ScratchBird Spatial Functions Test
-- This file contains test cases for the new spatial functions

-- Sample table creation
CREATE TABLE spatial_features (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    geometry BLOB SUB_TYPE GEOMETRY,
    boundary BLOB SUB_TYPE GEOMETRY
);

-- Create spatial index for efficient spatial queries
CREATE INDEX idx_features_geom ON spatial_features USING RTREE (geometry) (SRID = 4326);

-- Spatial predicate functions (return boolean)
SELECT name FROM spatial_features 
WHERE ST_CONTAINS(boundary, geometry);

SELECT name FROM spatial_features f1, spatial_features f2
WHERE ST_INTERSECTS(f1.geometry, f2.geometry) AND f1.id != f2.id;

SELECT name FROM spatial_features
WHERE ST_WITHIN(geometry, ST_GEOMFROMTEXT('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))', 4326));

SELECT name FROM spatial_features f1, spatial_features f2
WHERE ST_TOUCHES(f1.boundary, f2.boundary);

SELECT name FROM spatial_features f1, spatial_features f2
WHERE ST_CROSSES(f1.geometry, f2.geometry);

SELECT name FROM spatial_features f1, spatial_features f2
WHERE ST_OVERLAPS(f1.geometry, f2.geometry);

SELECT name FROM spatial_features f1, spatial_features f2
WHERE ST_EQUALS(f1.geometry, f2.geometry);

SELECT name FROM spatial_features f1, spatial_features f2
WHERE ST_DISJOINT(f1.geometry, f2.geometry);

-- Distance functions
SELECT name, ST_DISTANCE(geometry, ST_GEOMFROMTEXT('POINT(0 0)', 4326)) as distance
FROM spatial_features
ORDER BY distance;

SELECT name FROM spatial_features
WHERE ST_DWITHIN(geometry, ST_GEOMFROMTEXT('POINT(5 5)', 4326), 100.0);

-- Measurement functions
SELECT name, ST_AREA(boundary) as area_sqm
FROM spatial_features
WHERE ST_AREA(boundary) > 1000.0;

SELECT name, ST_LENGTH(geometry) as length_m
FROM spatial_features
WHERE ST_LENGTH(geometry) > 50.0;

-- Geometric property functions
SELECT name, ST_ASTEXT(ST_CENTROID(geometry)) as center_point
FROM spatial_features;

SELECT name, ST_ASTEXT(ST_ENVELOPE(geometry)) as bounding_box
FROM spatial_features;

-- Geometric operation functions
SELECT ST_ASTEXT(ST_BUFFER(geometry, 10.0)) as buffered_geom
FROM spatial_features
WHERE id = 1;

SELECT ST_ASTEXT(ST_INTERSECTION(f1.geometry, f2.geometry)) as intersection_geom
FROM spatial_features f1, spatial_features f2
WHERE ST_INTERSECTS(f1.geometry, f2.geometry) AND f1.id < f2.id;

SELECT ST_ASTEXT(ST_UNION(f1.geometry, f2.geometry)) as union_geom
FROM spatial_features f1, spatial_features f2
WHERE f1.id = 1 AND f2.id = 2;

SELECT ST_ASTEXT(ST_DIFFERENCE(f1.geometry, f2.geometry)) as difference_geom
FROM spatial_features f1, spatial_features f2
WHERE ST_OVERLAPS(f1.geometry, f2.geometry) AND f1.id < f2.id;

-- Geometry conversion functions
INSERT INTO spatial_features (id, name, geometry) VALUES 
(1, 'Test Point', ST_GEOMFROMTEXT('POINT(10 20)', 4326));

INSERT INTO spatial_features (id, name, geometry) VALUES 
(2, 'Test Polygon', ST_GEOMFROMTEXT('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))', 4326));

-- WKB conversion
SELECT ST_ASTEXT(ST_GEOMFROMWKB(ST_ASBINARY(geometry))) as roundtrip_text
FROM spatial_features
WHERE id = 1;

-- Complex spatial query example
SELECT f1.name as feature1, f2.name as feature2, 
       ST_DISTANCE(f1.geometry, f2.geometry) as distance
FROM spatial_features f1, spatial_features f2
WHERE f1.id != f2.id 
  AND ST_DWITHIN(f1.geometry, f2.geometry, 1000.0)
  AND ST_AREA(ST_INTERSECTION(f1.boundary, f2.boundary)) > 100.0
ORDER BY distance;