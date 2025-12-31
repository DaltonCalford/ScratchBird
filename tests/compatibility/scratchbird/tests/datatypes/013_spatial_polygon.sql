-- ScratchBird Native Test: Spatial Types - POLYGON
-- Test ID: datatypes_013
-- Description: Comprehensive testing of POLYGON geometry type

CREATE DATABASE test_spatial_polygon;
\c test_spatial_polygon;

-- ============================================================================
-- POLYGON Basic Testing
-- ============================================================================

CREATE TABLE test_polygon_basic (
    id INTEGER PRIMARY KEY,
    poly POLYGON,
    name VARCHAR(100)
);

-- Simple polygons
INSERT INTO test_polygon_basic VALUES (1,
    POLYGON((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
    'Square'
);

INSERT INTO test_polygon_basic VALUES (2,
    POLYGON((POINT(0, 0), POINT(10, 0), POINT(5, 10), POINT(0, 0))),
    'Triangle'
);

INSERT INTO test_polygon_basic VALUES (3,
    POLYGON((POINT(0, 0), POINT(20, 0), POINT(20, 5), POINT(0, 5), POINT(0, 0))),
    'Rectangle'
);

-- Pentagon
INSERT INTO test_polygon_basic VALUES (10,
    POLYGON((POINT(5, 0), POINT(10, 3.5), POINT(8, 9), POINT(2, 9), POINT(0, 3.5), POINT(5, 0))),
    'Pentagon'
);

SELECT id, ST_AsText(poly) AS polygon_wkt, name FROM test_polygon_basic ORDER BY id;

-- ============================================================================
-- POLYGON with Holes
-- ============================================================================

CREATE TABLE test_polygon_holes (
    id INTEGER PRIMARY KEY,
    poly POLYGON,
    description VARCHAR(100)
);

-- Polygon with one hole
INSERT INTO test_polygon_holes VALUES (1,
    ST_GeomFromText('POLYGON((0 0, 100 0, 100 100, 0 100, 0 0), (20 20, 80 20, 80 80, 20 80, 20 20))'),
    'Square with square hole'
);

-- Polygon with multiple holes
INSERT INTO test_polygon_holes VALUES (2,
    ST_GeomFromText('POLYGON((0 0, 200 0, 200 200, 0 200, 0 0), (20 20, 60 20, 60 60, 20 60, 20 20), (120 120, 180 120, 180 180, 120 180, 120 120))'),
    'Square with two square holes'
);

SELECT id, ST_AsText(poly) AS polygon_wkt, description FROM test_polygon_holes ORDER BY id;

-- Number of rings
SELECT id, description,
       ST_NumInteriorRings(poly) AS num_holes
FROM test_polygon_holes
ORDER BY id;

-- ============================================================================
-- POLYGON Input Formats
-- ============================================================================

CREATE TABLE test_polygon_formats (
    id INTEGER PRIMARY KEY,
    poly_geom POLYGON,
    format_description VARCHAR(50)
);

-- WKT format
INSERT INTO test_polygon_formats VALUES (1,
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
    'WKT format'
);

INSERT INTO test_polygon_formats VALUES (2,
    ST_PolygonFromText('POLYGON((0 0, 5 0, 5 5, 0 5, 0 0))'),
    'PolygonFromText'
);

-- From linestring (exterior ring)
INSERT INTO test_polygon_formats VALUES (10,
    ST_MakePolygon(LINESTRING(POINT(0, 0), POINT(20, 0), POINT(20, 20), POINT(0, 20), POINT(0, 0))),
    'ST_MakePolygon from linestring'
);

-- WKB round-trip
INSERT INTO test_polygon_formats VALUES (20,
    ST_GeomFromWKB(ST_AsBinary(POLYGON((POINT(30, 30), POINT(40, 30), POINT(40, 40), POINT(30, 40), POINT(30, 30))))),
    'WKB round-trip'
);

SELECT id, ST_AsText(poly_geom) AS wkt, format_description FROM test_polygon_formats ORDER BY id;

-- ============================================================================
-- POLYGON Properties
-- ============================================================================

CREATE TABLE test_polygon_properties (
    id INTEGER PRIMARY KEY,
    poly POLYGON
);

INSERT INTO test_polygon_properties VALUES (1, POLYGON((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))));
INSERT INTO test_polygon_properties VALUES (2, POLYGON((POINT(0, 0), POINT(15, 0), POINT(15, 5), POINT(0, 5), POINT(0, 0))));
INSERT INTO test_polygon_properties VALUES (3, POLYGON((POINT(0, 0), POINT(10, 0), POINT(5, 8.66), POINT(0, 0))));

-- Basic properties
SELECT id,
       ST_AsText(poly) AS polygon,
       ST_GeometryType(poly) AS geom_type,
       ST_Dimension(poly) AS dimension,
       ST_IsValid(poly) AS is_valid,
       ST_IsSimple(poly) AS is_simple,
       ST_IsClosed(poly) AS is_closed
FROM test_polygon_properties
ORDER BY id;

-- Area and perimeter
SELECT id,
       ST_AsText(poly) AS polygon,
       ST_Area(poly) AS area,
       ST_Perimeter(poly) AS perimeter
FROM test_polygon_properties
ORDER BY id;

-- Number of points in exterior ring
SELECT id,
       ST_NPoints(ST_ExteriorRing(poly)) AS exterior_points,
       ST_NumInteriorRings(poly) AS num_holes
FROM test_polygon_properties
ORDER BY id;

-- ============================================================================
-- POLYGON Ring Extraction
-- ============================================================================

CREATE TABLE test_polygon_rings (
    id INTEGER PRIMARY KEY,
    poly POLYGON
);

INSERT INTO test_polygon_rings VALUES (1,
    ST_GeomFromText('POLYGON((0 0, 100 0, 100 100, 0 100, 0 0), (20 20, 80 20, 80 80, 20 80, 20 20))')
);

-- Extract exterior ring
SELECT id,
       ST_AsText(ST_ExteriorRing(poly)) AS exterior_ring
FROM test_polygon_rings;

-- Extract interior rings (holes)
SELECT id,
       ST_AsText(ST_InteriorRingN(poly, 1)) AS first_hole
FROM test_polygon_rings;

-- All rings
SELECT id,
       0 AS ring_index,
       'Exterior' AS ring_type,
       ST_AsText(ST_ExteriorRing(poly)) AS ring_wkt
FROM test_polygon_rings
UNION ALL
SELECT id,
       n AS ring_index,
       'Interior' AS ring_type,
       ST_AsText(ST_InteriorRingN(poly, n)) AS ring_wkt
FROM test_polygon_rings,
     generate_series(1, ST_NumInteriorRings(poly)) AS n
ORDER BY id, ring_index;

-- ============================================================================
-- POLYGON Centroid and Representative Points
-- ============================================================================

CREATE TABLE test_polygon_centroid (
    id INTEGER PRIMARY KEY,
    poly POLYGON,
    name VARCHAR(50)
);

INSERT INTO test_polygon_centroid VALUES (1,
    POLYGON((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
    'Square'
);

INSERT INTO test_polygon_centroid VALUES (2,
    POLYGON((POINT(0, 0), POINT(20, 0), POINT(10, 15), POINT(0, 0))),
    'Triangle'
);

-- Centroid (geometric center)
SELECT id, name,
       ST_AsText(poly) AS polygon,
       ST_AsText(ST_Centroid(poly)) AS centroid
FROM test_polygon_centroid
ORDER BY id;

-- Point on surface (guaranteed to be inside polygon)
SELECT id, name,
       ST_AsText(ST_PointOnSurface(poly)) AS point_on_surface
FROM test_polygon_centroid
ORDER BY id;

-- ============================================================================
-- POLYGON Spatial Relationships
-- ============================================================================

CREATE TABLE test_polygon_relationships (
    id INTEGER PRIMARY KEY,
    poly POLYGON,
    name VARCHAR(50)
);

INSERT INTO test_polygon_relationships VALUES (1,
    POLYGON((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
    'Square A'
);

INSERT INTO test_polygon_relationships VALUES (2,
    POLYGON((POINT(5, 5), POINT(15, 5), POINT(15, 15), POINT(5, 15), POINT(5, 5))),
    'Square B (overlapping)'
);

INSERT INTO test_polygon_relationships VALUES (3,
    POLYGON((POINT(20, 0), POINT(30, 0), POINT(30, 10), POINT(20, 10), POINT(20, 0))),
    'Square C (adjacent)'
);

INSERT INTO test_polygon_relationships VALUES (4,
    POLYGON((POINT(2, 2), POINT(8, 2), POINT(8, 8), POINT(2, 8), POINT(2, 2))),
    'Square D (contained in A)'
);

-- Intersection
SELECT p1.name AS poly1, p2.name AS poly2,
       ST_Intersects(p1.poly, p2.poly) AS intersects
FROM test_polygon_relationships p1
CROSS JOIN test_polygon_relationships p2
WHERE p1.id < p2.id;

-- Contains / Within
SELECT p1.name AS poly1, p2.name AS poly2,
       ST_Contains(p1.poly, p2.poly) AS p1_contains_p2,
       ST_Within(p2.poly, p1.poly) AS p2_within_p1
FROM test_polygon_relationships p1
CROSS JOIN test_polygon_relationships p2
WHERE p1.id < p2.id;

-- Overlaps
SELECT p1.name AS poly1, p2.name AS poly2,
       ST_Overlaps(p1.poly, p2.poly) AS overlaps
FROM test_polygon_relationships p1
CROSS JOIN test_polygon_relationships p2
WHERE p1.id < p2.id;

-- Touches
SELECT p1.name AS poly1, p2.name AS poly2,
       ST_Touches(p1.poly, p2.poly) AS touches
FROM test_polygon_relationships p1
CROSS JOIN test_polygon_relationships p2
WHERE p1.id < p2.id;

-- Intersection geometry
SELECT p1.name AS poly1, p2.name AS poly2,
       ST_AsText(ST_Intersection(p1.poly, p2.poly)) AS intersection_geom,
       ST_Area(ST_Intersection(p1.poly, p2.poly)) AS intersection_area
FROM test_polygon_relationships p1
CROSS JOIN test_polygon_relationships p2
WHERE p1.id < p2.id AND ST_Intersects(p1.poly, p2.poly);

-- ============================================================================
-- POLYGON and POINT Relationships
-- ============================================================================

CREATE TABLE test_polygon_point (
    id INTEGER PRIMARY KEY,
    poly POLYGON
);

INSERT INTO test_polygon_point VALUES (1,
    POLYGON((POINT(0, 0), POINT(100, 0), POINT(100, 100), POINT(0, 100), POINT(0, 0)))
);

-- Point containment
SELECT id,
       ST_Contains(poly, POINT(50, 50)) AS contains_center,
       ST_Contains(poly, POINT(0, 0)) AS contains_corner,
       ST_Contains(poly, POINT(150, 150)) AS contains_outside
FROM test_polygon_point;

-- Distance from point to polygon
SELECT id,
       ST_Distance(poly, POINT(50, 50)) AS dist_to_inside,
       ST_Distance(poly, POINT(0, 50)) AS dist_to_edge,
       ST_Distance(poly, POINT(150, 150)) AS dist_to_outside
FROM test_polygon_point;

-- ============================================================================
-- POLYGON Boolean Operations
-- ============================================================================

CREATE TABLE test_polygon_boolean (
    id INTEGER PRIMARY KEY,
    poly1 POLYGON,
    poly2 POLYGON
);

INSERT INTO test_polygon_boolean VALUES (1,
    POLYGON((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
    POLYGON((POINT(5, 5), POINT(15, 5), POINT(15, 15), POINT(5, 15), POINT(5, 5)))
);

-- Union
SELECT id,
       ST_AsText(ST_Union(poly1, poly2)) AS union_result,
       ST_Area(ST_Union(poly1, poly2)) AS union_area
FROM test_polygon_boolean;

-- Intersection
SELECT id,
       ST_AsText(ST_Intersection(poly1, poly2)) AS intersection_result,
       ST_Area(ST_Intersection(poly1, poly2)) AS intersection_area
FROM test_polygon_boolean;

-- Difference (poly1 - poly2)
SELECT id,
       ST_AsText(ST_Difference(poly1, poly2)) AS difference_result,
       ST_Area(ST_Difference(poly1, poly2)) AS difference_area
FROM test_polygon_boolean;

-- Symmetric difference (XOR)
SELECT id,
       ST_AsText(ST_SymDifference(poly1, poly2)) AS symdiff_result,
       ST_Area(ST_SymDifference(poly1, poly2)) AS symdiff_area
FROM test_polygon_boolean;

-- ============================================================================
-- POLYGON Buffers
-- ============================================================================

CREATE TABLE test_polygon_buffer (
    id INTEGER PRIMARY KEY,
    poly POLYGON
);

INSERT INTO test_polygon_buffer VALUES (1,
    POLYGON((POINT(50, 50), POINT(60, 50), POINT(60, 60), POINT(50, 60), POINT(50, 50)))
);

-- Positive buffer (expand)
SELECT id,
       ST_Area(poly) AS original_area,
       ST_Area(ST_Buffer(poly, 5)) AS buffered_area_5,
       ST_Area(ST_Buffer(poly, 10)) AS buffered_area_10
FROM test_polygon_buffer;

-- Negative buffer (shrink/erode)
SELECT id,
       ST_Area(poly) AS original_area,
       ST_Area(ST_Buffer(poly, -2)) AS eroded_area_2,
       ST_Area(ST_Buffer(poly, -4)) AS eroded_area_4
FROM test_polygon_buffer;

-- ============================================================================
-- POLYGON Simplification
-- ============================================================================

CREATE TABLE test_polygon_simplify (
    id INTEGER PRIMARY KEY,
    poly POLYGON
);

-- Complex polygon with many vertices
INSERT INTO test_polygon_simplify VALUES (1,
    ST_Buffer(POINT(0, 0), 10, 'quad_segs=16')
);

-- Simplify
SELECT id,
       ST_NPoints(poly) AS original_points,
       ST_AsText(ST_Simplify(poly, 1.0)) AS simplified,
       ST_NPoints(ST_Simplify(poly, 1.0)) AS simplified_points,
       ST_Area(poly) AS original_area,
       ST_Area(ST_Simplify(poly, 1.0)) AS simplified_area
FROM test_polygon_simplify;

-- ============================================================================
-- POLYGON Convex Hull
-- ============================================================================

CREATE TABLE test_polygon_hull (
    id INTEGER PRIMARY KEY,
    poly POLYGON
);

-- Concave polygon
INSERT INTO test_polygon_hull VALUES (1,
    POLYGON((POINT(0, 0), POINT(10, 0), POINT(5, 3), POINT(10, 10), POINT(0, 10), POINT(0, 0)))
);

-- Convex hull
SELECT id,
       ST_AsText(poly) AS original,
       ST_AsText(ST_ConvexHull(poly)) AS convex_hull,
       ST_Area(poly) AS original_area,
       ST_Area(ST_ConvexHull(poly)) AS hull_area
FROM test_polygon_hull;

-- ============================================================================
-- POLYGON Bounding Box
-- ============================================================================

CREATE TABLE test_polygon_bbox (
    id INTEGER PRIMARY KEY,
    poly POLYGON,
    name VARCHAR(50)
);

INSERT INTO test_polygon_bbox VALUES (1,
    POLYGON((POINT(10, 20), POINT(30, 10), POINT(50, 40), POINT(10, 20))),
    'Tilted triangle'
);

-- Envelope (bounding box)
SELECT id, name,
       ST_AsText(poly) AS original,
       ST_AsText(ST_Envelope(poly)) AS bounding_box,
       ST_Area(poly) AS original_area,
       ST_Area(ST_Envelope(poly)) AS bbox_area
FROM test_polygon_bbox;

-- Extent (aggregate bounding box)
SELECT ST_AsText(ST_Extent(poly)) AS total_extent
FROM test_polygon_bbox;

-- ============================================================================
-- POLYGON Validation
-- ============================================================================

CREATE TABLE test_polygon_validation (
    id INTEGER PRIMARY KEY,
    poly POLYGON,
    description VARCHAR(100)
);

-- Valid polygon
INSERT INTO test_polygon_validation VALUES (1,
    POLYGON((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
    'Valid square'
);

-- Self-intersecting polygon (invalid)
-- INSERT INTO test_polygon_validation VALUES (2,
--     ST_GeomFromText('POLYGON((0 0, 10 10, 10 0, 0 10, 0 0))'),
--     'Self-intersecting (figure-eight)'
-- );

-- Check validity
SELECT id, description,
       ST_IsValid(poly) AS is_valid,
       ST_IsValidReason(poly) AS validation_message
FROM test_polygon_validation
ORDER BY id;

-- Make valid (fix invalid geometries)
SELECT id, description,
       ST_IsValid(poly) AS was_valid,
       ST_IsValid(ST_MakeValid(poly)) AS now_valid
FROM test_polygon_validation
ORDER BY id;

-- ============================================================================
-- POLYGON Aggregation
-- ============================================================================

CREATE TABLE test_polygon_aggregation (
    id INTEGER PRIMARY KEY,
    category VARCHAR(20),
    zone POLYGON
);

INSERT INTO test_polygon_aggregation VALUES (1, 'Zone A',
    POLYGON((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0)))
);
INSERT INTO test_polygon_aggregation VALUES (2, 'Zone A',
    POLYGON((POINT(10, 0), POINT(20, 0), POINT(20, 10), POINT(10, 10), POINT(10, 0)))
);
INSERT INTO test_polygon_aggregation VALUES (3, 'Zone B',
    POLYGON((POINT(0, 20), POINT(15, 20), POINT(15, 30), POINT(0, 30), POINT(0, 20)))
);

-- Union of all polygons in category
SELECT category,
       ST_AsText(ST_Union(zone)) AS merged_zones,
       SUM(ST_Area(zone)) AS total_area
FROM test_polygon_aggregation
GROUP BY category
ORDER BY category;

-- Collect polygons
SELECT category,
       ST_AsText(ST_Collect(zone)) AS collected
FROM test_polygon_aggregation
GROUP BY category
ORDER BY category;

-- ============================================================================
-- POLYGON NULL Handling
-- ============================================================================

CREATE TABLE test_polygon_nulls (
    id INTEGER PRIMARY KEY,
    nullable_poly POLYGON,
    description VARCHAR(50)
);

INSERT INTO test_polygon_nulls VALUES (1, NULL, 'No polygon');
INSERT INTO test_polygon_nulls VALUES (2,
    POLYGON((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
    'Has polygon'
);
INSERT INTO test_polygon_nulls VALUES (3, NULL, 'Another null');

SELECT COUNT(*) AS null_count FROM test_polygon_nulls WHERE nullable_poly IS NULL;

SELECT id, description,
       ST_AsText(COALESCE(nullable_poly, POLYGON((POINT(0, 0), POINT(1, 0), POINT(1, 1), POINT(0, 1), POINT(0, 0))))) AS poly_with_default
FROM test_polygon_nulls
ORDER BY id;

-- ============================================================================
-- POLYGON Indexing
-- ============================================================================

CREATE TABLE test_polygon_index (
    id INTEGER PRIMARY KEY,
    boundary POLYGON,
    region_name VARCHAR(100)
);

CREATE INDEX idx_boundary_gist ON test_polygon_index USING GIST (boundary);

INSERT INTO test_polygon_index VALUES (1,
    POLYGON((POINT(-74.1, 40.6), POINT(-73.9, 40.6), POINT(-73.9, 40.8), POINT(-74.1, 40.8), POINT(-74.1, 40.6))),
    'Manhattan'
);
INSERT INTO test_polygon_index VALUES (2,
    POLYGON((POINT(-74.0, 40.5), POINT(-73.8, 40.5), POINT(-73.8, 40.7), POINT(-74.0, 40.7), POINT(-74.0, 40.5))),
    'Brooklyn'
);

-- Find regions containing a point
SELECT region_name
FROM test_polygon_index
WHERE ST_Contains(boundary, POINT(-74.0, 40.7))
ORDER BY region_name;

-- Find regions intersecting a bounding box
SELECT region_name
FROM test_polygon_index
WHERE boundary && ST_MakeEnvelope(-74.0, 40.6, -73.95, 40.75)
ORDER BY region_name;

-- ============================================================================
-- POLYGON Real-World Use Cases
-- ============================================================================

-- Administrative boundaries
CREATE TABLE test_admin_boundaries (
    id INTEGER PRIMARY KEY,
    boundary_name VARCHAR(100),
    boundary_geom POLYGON,
    population INTEGER
);

INSERT INTO test_admin_boundaries VALUES (1, 'District 1',
    POLYGON((POINT(0, 0), POINT(50, 0), POINT(50, 50), POINT(0, 50), POINT(0, 0))),
    50000
);
INSERT INTO test_admin_boundaries VALUES (2, 'District 2',
    POLYGON((POINT(50, 0), POINT(100, 0), POINT(100, 50), POINT(50, 50), POINT(50, 0))),
    75000
);

-- Calculate population density
SELECT boundary_name,
       population,
       ST_Area(boundary_geom) AS area,
       population / ST_Area(boundary_geom) AS density
FROM test_admin_boundaries
ORDER BY density DESC;

-- Land parcels
CREATE TABLE test_parcels (
    id INTEGER PRIMARY KEY,
    parcel_id VARCHAR(50),
    parcel_boundary POLYGON,
    land_use VARCHAR(50)
);

INSERT INTO test_parcels VALUES (1, 'P001',
    POLYGON((POINT(0, 0), POINT(30, 0), POINT(30, 40), POINT(0, 40), POINT(0, 0))),
    'Residential'
);
INSERT INTO test_parcels VALUES (2, 'P002',
    POLYGON((POINT(30, 0), POINT(60, 0), POINT(60, 40), POINT(30, 40), POINT(30, 0))),
    'Commercial'
);

-- Calculate total area by land use
SELECT land_use,
       COUNT(*) AS parcel_count,
       SUM(ST_Area(parcel_boundary)) AS total_area
FROM test_parcels
GROUP BY land_use
ORDER BY total_area DESC;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_parcels;
DROP TABLE test_admin_boundaries;
DROP INDEX idx_boundary_gist;
DROP TABLE test_polygon_index;
DROP TABLE test_polygon_nulls;
DROP TABLE test_polygon_aggregation;
DROP TABLE test_polygon_validation;
DROP TABLE test_polygon_bbox;
DROP TABLE test_polygon_hull;
DROP TABLE test_polygon_simplify;
DROP TABLE test_polygon_buffer;
DROP TABLE test_polygon_boolean;
DROP TABLE test_polygon_point;
DROP TABLE test_polygon_relationships;
DROP TABLE test_polygon_centroid;
DROP TABLE test_polygon_rings;
DROP TABLE test_polygon_properties;
DROP TABLE test_polygon_formats;
DROP TABLE test_polygon_holes;
DROP TABLE test_polygon_basic;

DROP DATABASE test_spatial_polygon;
