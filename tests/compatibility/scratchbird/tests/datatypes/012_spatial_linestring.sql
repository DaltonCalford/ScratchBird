-- ScratchBird Native Test: Spatial Types - LINESTRING
-- Test ID: datatypes_012
-- Description: Comprehensive testing of LINESTRING geometry type

CREATE DATABASE test_spatial_linestring;
\c test_spatial_linestring;

-- ============================================================================
-- LINESTRING Basic Testing
-- ============================================================================

CREATE TABLE test_linestring_basic (
    id INTEGER PRIMARY KEY,
    line LINESTRING,
    name VARCHAR(100)
);

-- Simple linestrings
INSERT INTO test_linestring_basic VALUES (1, LINESTRING(POINT(0, 0), POINT(1, 1)), 'Diagonal');
INSERT INTO test_linestring_basic VALUES (2, LINESTRING(POINT(0, 0), POINT(10, 0)), 'Horizontal');
INSERT INTO test_linestring_basic VALUES (3, LINESTRING(POINT(0, 0), POINT(0, 10)), 'Vertical');

-- Multi-point linestrings
INSERT INTO test_linestring_basic VALUES (10, LINESTRING(POINT(0, 0), POINT(5, 5), POINT(10, 0)), 'Triangle path');
INSERT INTO test_linestring_basic VALUES (11, LINESTRING(POINT(0, 0), POINT(3, 4), POINT(6, 0), POINT(9, 4)), 'Zigzag');

-- Real-world: Street/Route
INSERT INTO test_linestring_basic VALUES (20,
    LINESTRING(POINT(-74.0060, 40.7128), POINT(-73.9352, 40.7306), POINT(-73.8654, 40.7489)),
    'Route NYC to Queens'
);

SELECT id, ST_AsText(line) AS linestring_wkt, name FROM test_linestring_basic ORDER BY id;

-- ============================================================================
-- LINESTRING Input Formats
-- ============================================================================

CREATE TABLE test_linestring_formats (
    id INTEGER PRIMARY KEY,
    line_geom LINESTRING,
    format_description VARCHAR(50)
);

-- WKT format
INSERT INTO test_linestring_formats VALUES (1,
    ST_GeomFromText('LINESTRING(0 0, 10 10, 20 0)'),
    'WKT format'
);

INSERT INTO test_linestring_formats VALUES (2,
    ST_LineFromText('LINESTRING(5 5, 15 15)'),
    'LineFromText'
);

-- Constructor from points
INSERT INTO test_linestring_formats VALUES (10,
    ST_MakeLine(ARRAY[POINT(0, 0), POINT(5, 5), POINT(10, 0)]),
    'ST_MakeLine from array'
);

-- From two points
INSERT INTO test_linestring_formats VALUES (11,
    ST_MakeLine(POINT(0, 0), POINT(10, 10)),
    'ST_MakeLine from two points'
);

-- WKB round-trip
INSERT INTO test_linestring_formats VALUES (20,
    ST_GeomFromWKB(ST_AsBinary(LINESTRING(POINT(20, 20), POINT(30, 30)))),
    'WKB round-trip'
);

SELECT id, ST_AsText(line_geom) AS wkt, format_description FROM test_linestring_formats ORDER BY id;

-- ============================================================================
-- LINESTRING Properties
-- ============================================================================

CREATE TABLE test_linestring_properties (
    id INTEGER PRIMARY KEY,
    line LINESTRING
);

INSERT INTO test_linestring_properties VALUES (1, LINESTRING(POINT(0, 0), POINT(10, 0)));
INSERT INTO test_linestring_properties VALUES (2, LINESTRING(POINT(0, 0), POINT(3, 4)));
INSERT INTO test_linestring_properties VALUES (3, LINESTRING(POINT(0, 0), POINT(5, 5), POINT(10, 0)));

-- Basic properties
SELECT id,
       ST_AsText(line) AS linestring,
       ST_GeometryType(line) AS geom_type,
       ST_Dimension(line) AS dimension,
       ST_IsValid(line) AS is_valid,
       ST_IsSimple(line) AS is_simple,
       ST_IsClosed(line) AS is_closed,
       ST_IsRing(line) AS is_ring
FROM test_linestring_properties
ORDER BY id;

-- Length calculation
SELECT id,
       ST_AsText(line) AS linestring,
       ST_Length(line) AS length
FROM test_linestring_properties
ORDER BY id;

-- Number of points
SELECT id,
       ST_AsText(line) AS linestring,
       ST_NPoints(line) AS num_points
FROM test_linestring_properties
ORDER BY id;

-- ============================================================================
-- LINESTRING Point Extraction
-- ============================================================================

CREATE TABLE test_linestring_points (
    id INTEGER PRIMARY KEY,
    line LINESTRING
);

INSERT INTO test_linestring_points VALUES (1,
    LINESTRING(POINT(0, 0), POINT(5, 5), POINT(10, 0), POINT(15, 5))
);

-- Get start and end points
SELECT id,
       ST_AsText(line) AS linestring,
       ST_AsText(ST_StartPoint(line)) AS start_point,
       ST_AsText(ST_EndPoint(line)) AS end_point
FROM test_linestring_points;

-- Get specific point by index (1-based)
SELECT id,
       ST_AsText(ST_PointN(line, 1)) AS first_point,
       ST_AsText(ST_PointN(line, 2)) AS second_point,
       ST_AsText(ST_PointN(line, 3)) AS third_point,
       ST_AsText(ST_PointN(line, 4)) AS fourth_point
FROM test_linestring_points;

-- Extract all points
SELECT id,
       ST_AsText(ST_PointN(line, n)) AS point,
       n AS point_index
FROM test_linestring_points,
     generate_series(1, ST_NPoints(line)) AS n
ORDER BY id, n;

-- ============================================================================
-- LINESTRING Interpolation
-- ============================================================================

CREATE TABLE test_linestring_interpolation (
    id INTEGER PRIMARY KEY,
    line LINESTRING
);

INSERT INTO test_linestring_interpolation VALUES (1,
    LINESTRING(POINT(0, 0), POINT(100, 0))
);

INSERT INTO test_linestring_interpolation VALUES (2,
    LINESTRING(POINT(0, 0), POINT(50, 50), POINT(100, 0))
);

-- Interpolate point at fraction of line length
SELECT id,
       ST_AsText(line) AS linestring,
       ST_AsText(ST_LineInterpolatePoint(line, 0.0)) AS at_start,
       ST_AsText(ST_LineInterpolatePoint(line, 0.25)) AS at_quarter,
       ST_AsText(ST_LineInterpolatePoint(line, 0.5)) AS at_half,
       ST_AsText(ST_LineInterpolatePoint(line, 0.75)) AS at_three_quarters,
       ST_AsText(ST_LineInterpolatePoint(line, 1.0)) AS at_end
FROM test_linestring_interpolation
ORDER BY id;

-- Locate point along line
SELECT id,
       ST_LineLocatePoint(line, POINT(50, 0)) AS fraction_at_50_0,
       ST_LineLocatePoint(line, POINT(25, 12.5)) AS fraction_at_25_12
FROM test_linestring_interpolation
WHERE id = 2;

-- ============================================================================
-- LINESTRING Subsetting
-- ============================================================================

CREATE TABLE test_linestring_subset (
    id INTEGER PRIMARY KEY,
    line LINESTRING
);

INSERT INTO test_linestring_subset VALUES (1,
    LINESTRING(POINT(0, 0), POINT(100, 100))
);

-- Extract substring between fractions
SELECT id,
       ST_AsText(line) AS original,
       ST_AsText(ST_LineSubstring(line, 0.25, 0.75)) AS middle_half,
       ST_AsText(ST_LineSubstring(line, 0.0, 0.5)) AS first_half,
       ST_AsText(ST_LineSubstring(line, 0.5, 1.0)) AS second_half
FROM test_linestring_subset;

-- ============================================================================
-- LINESTRING Spatial Relationships
-- ============================================================================

CREATE TABLE test_linestring_relationships (
    id INTEGER PRIMARY KEY,
    line LINESTRING,
    name VARCHAR(50)
);

INSERT INTO test_linestring_relationships VALUES (1, LINESTRING(POINT(0, 0), POINT(10, 0)), 'Horizontal');
INSERT INTO test_linestring_relationships VALUES (2, LINESTRING(POINT(5, -5), POINT(5, 5)), 'Vertical crossing');
INSERT INTO test_linestring_relationships VALUES (3, LINESTRING(POINT(10, 0), POINT(20, 0)), 'Horizontal adjacent');
INSERT INTO test_linestring_relationships VALUES (4, LINESTRING(POINT(0, 5), POINT(10, 5)), 'Horizontal parallel');

-- Intersection
SELECT l1.name AS line1, l2.name AS line2,
       ST_Intersects(l1.line, l2.line) AS intersects,
       ST_AsText(ST_Intersection(l1.line, l2.line)) AS intersection_geom
FROM test_linestring_relationships l1
CROSS JOIN test_linestring_relationships l2
WHERE l1.id < l2.id;

-- Touches
SELECT l1.name AS line1, l2.name AS line2,
       ST_Touches(l1.line, l2.line) AS touches
FROM test_linestring_relationships l1
CROSS JOIN test_linestring_relationships l2
WHERE l1.id < l2.id AND ST_Touches(l1.line, l2.line);

-- Crosses
SELECT l1.name AS line1, l2.name AS line2,
       ST_Crosses(l1.line, l2.line) AS crosses
FROM test_linestring_relationships l1
CROSS JOIN test_linestring_relationships l2
WHERE l1.id < l2.id AND ST_Crosses(l1.line, l2.line);

-- Distance between lines
SELECT l1.name AS line1, l2.name AS line2,
       ST_Distance(l1.line, l2.line) AS distance
FROM test_linestring_relationships l1
CROSS JOIN test_linestring_relationships l2
WHERE l1.id < l2.id
ORDER BY distance;

-- ============================================================================
-- LINESTRING and POINT Relationships
-- ============================================================================

CREATE TABLE test_linestring_point (
    id INTEGER PRIMARY KEY,
    line LINESTRING
);

INSERT INTO test_linestring_point VALUES (1, LINESTRING(POINT(0, 0), POINT(10, 10)));

-- Point on line
SELECT id,
       ST_Contains(line, POINT(5, 5)) AS contains_midpoint,
       ST_Contains(line, POINT(0, 0)) AS contains_start,
       ST_Contains(line, POINT(5, 0)) AS contains_off_line
FROM test_linestring_point;

-- Distance from point to line
SELECT id,
       ST_Distance(line, POINT(5, 5)) AS dist_to_on_line,
       ST_Distance(line, POINT(5, 0)) AS dist_to_off_line,
       ST_Distance(line, POINT(0, 10)) AS dist_to_far_point
FROM test_linestring_point;

-- Closest point on line to external point
SELECT id,
       ST_AsText(ST_ClosestPoint(line, POINT(5, 0))) AS closest_to_5_0,
       ST_AsText(ST_ClosestPoint(line, POINT(0, 10))) AS closest_to_0_10
FROM test_linestring_point;

-- ============================================================================
-- LINESTRING Buffers
-- ============================================================================

CREATE TABLE test_linestring_buffer (
    id INTEGER PRIMARY KEY,
    line LINESTRING
);

INSERT INTO test_linestring_buffer VALUES (1, LINESTRING(POINT(0, 0), POINT(10, 0)));
INSERT INTO test_linestring_buffer VALUES (2, LINESTRING(POINT(0, 0), POINT(5, 5), POINT(10, 0)));

-- Create buffer around line
SELECT id,
       ST_AsText(line) AS linestring,
       ST_Area(ST_Buffer(line, 1)) AS buffer_area_1,
       ST_Area(ST_Buffer(line, 5)) AS buffer_area_5
FROM test_linestring_buffer
ORDER BY id;

-- Buffer with different cap styles
SELECT id,
       ST_Area(ST_Buffer(line, 2, 'endcap=round')) AS round_cap,
       ST_Area(ST_Buffer(line, 2, 'endcap=flat')) AS flat_cap,
       ST_Area(ST_Buffer(line, 2, 'endcap=square')) AS square_cap
FROM test_linestring_buffer
WHERE id = 1;

-- ============================================================================
-- LINESTRING Transformations
-- ============================================================================

CREATE TABLE test_linestring_transform (
    id INTEGER PRIMARY KEY,
    line LINESTRING
);

INSERT INTO test_linestring_transform VALUES (1, LINESTRING(POINT(0, 0), POINT(10, 0), POINT(10, 10)));

-- Reverse line direction
SELECT id,
       ST_AsText(line) AS original,
       ST_AsText(ST_Reverse(line)) AS reversed
FROM test_linestring_transform;

-- Simplify line (reduce points while preserving shape)
INSERT INTO test_linestring_transform VALUES (2,
    LINESTRING(POINT(0,0), POINT(1,0), POINT(2,0), POINT(3,0), POINT(4,0),
               POINT(5,0), POINT(6,0), POINT(7,0), POINT(8,0), POINT(9,0), POINT(10,0))
);

SELECT id,
       ST_NPoints(line) AS original_points,
       ST_AsText(ST_Simplify(line, 1.0)) AS simplified,
       ST_NPoints(ST_Simplify(line, 1.0)) AS simplified_points
FROM test_linestring_transform
WHERE id = 2;

-- Translate (move) line
SELECT id,
       ST_AsText(line) AS original,
       ST_AsText(ST_Translate(line, 5, 3)) AS translated
FROM test_linestring_transform
WHERE id = 1;

-- Scale line
SELECT id,
       ST_AsText(line) AS original,
       ST_AsText(ST_Scale(line, 2, 2)) AS scaled_2x
FROM test_linestring_transform
WHERE id = 1;

-- ============================================================================
-- LINESTRING Measurement Functions
-- ============================================================================

CREATE TABLE test_linestring_measurement (
    id INTEGER PRIMARY KEY,
    route LINESTRING,
    name VARCHAR(100)
);

INSERT INTO test_linestring_measurement VALUES (1,
    LINESTRING(POINT(0, 0), POINT(100, 0)),
    'Straight 100 units'
);

INSERT INTO test_linestring_measurement VALUES (2,
    LINESTRING(POINT(0, 0), POINT(60, 80)),
    'Diagonal (3-4-5 triangle scaled by 20)'
);

INSERT INTO test_linestring_measurement VALUES (3,
    LINESTRING(POINT(0, 0), POINT(50, 50), POINT(100, 0)),
    'Triangle path'
);

-- Length
SELECT id, name,
       ST_Length(route) AS length,
       ST_Length(route) / 1000 AS length_km
FROM test_linestring_measurement
ORDER BY id;

-- 2D vs 3D length (if 3D supported)
SELECT id, name,
       ST_Length(route) AS length_2d,
       ST_3DLength(route) AS length_3d
FROM test_linestring_measurement
ORDER BY id;

-- ============================================================================
-- LINESTRING Aggregation
-- ============================================================================

CREATE TABLE test_linestring_aggregation (
    id INTEGER PRIMARY KEY,
    category VARCHAR(20),
    segment LINESTRING
);

INSERT INTO test_linestring_aggregation VALUES (1, 'Route A', LINESTRING(POINT(0, 0), POINT(5, 0)));
INSERT INTO test_linestring_aggregation VALUES (2, 'Route A', LINESTRING(POINT(5, 0), POINT(10, 5)));
INSERT INTO test_linestring_aggregation VALUES (3, 'Route A', LINESTRING(POINT(10, 5), POINT(15, 0)));
INSERT INTO test_linestring_aggregation VALUES (4, 'Route B', LINESTRING(POINT(0, 10), POINT(10, 10)));
INSERT INTO test_linestring_aggregation VALUES (5, 'Route B', LINESTRING(POINT(10, 10), POINT(10, 20)));

-- Merge linestrings
SELECT category,
       ST_AsText(ST_LineMerge(ST_Collect(segment))) AS merged_route,
       SUM(ST_Length(segment)) AS total_length
FROM test_linestring_aggregation
GROUP BY category
ORDER BY category;

-- Union of lines
SELECT category,
       ST_AsText(ST_Union(segment)) AS union_geom
FROM test_linestring_aggregation
GROUP BY category
ORDER BY category;

-- ============================================================================
-- LINESTRING NULL Handling
-- ============================================================================

CREATE TABLE test_linestring_nulls (
    id INTEGER PRIMARY KEY,
    nullable_line LINESTRING,
    description VARCHAR(50)
);

INSERT INTO test_linestring_nulls VALUES (1, NULL, 'No line');
INSERT INTO test_linestring_nulls VALUES (2, LINESTRING(POINT(0, 0), POINT(10, 10)), 'Has line');
INSERT INTO test_linestring_nulls VALUES (3, NULL, 'Another null');

SELECT COUNT(*) AS null_count FROM test_linestring_nulls WHERE nullable_line IS NULL;

SELECT id, description,
       ST_AsText(COALESCE(nullable_line, LINESTRING(POINT(0, 0), POINT(1, 1)))) AS line_with_default
FROM test_linestring_nulls
ORDER BY id;

-- ============================================================================
-- LINESTRING Indexing
-- ============================================================================

CREATE TABLE test_linestring_index (
    id INTEGER PRIMARY KEY,
    road LINESTRING,
    road_name VARCHAR(100)
);

CREATE INDEX idx_road_gist ON test_linestring_index USING GIST (road);

INSERT INTO test_linestring_index VALUES (1, LINESTRING(POINT(0, 0), POINT(10, 0)), 'Main Street');
INSERT INTO test_linestring_index VALUES (2, LINESTRING(POINT(5, -5), POINT(5, 5)), 'First Avenue');
INSERT INTO test_linestring_index VALUES (3, LINESTRING(POINT(0, 10), POINT(10, 10)), 'Oak Street');
INSERT INTO test_linestring_index VALUES (4, LINESTRING(POINT(10, 0), POINT(10, 10)), 'Park Avenue');

-- Find roads that intersect a point
SELECT road_name
FROM test_linestring_index
WHERE ST_Intersects(road, POINT(5, 0))
ORDER BY road_name;

-- Find roads within bounding box
SELECT road_name
FROM test_linestring_index
WHERE road && ST_MakeEnvelope(0, 0, 6, 6)
ORDER BY road_name;

-- ============================================================================
-- LINESTRING Real-World Use Cases
-- ============================================================================

-- Transportation routes
CREATE TABLE test_routes (
    id INTEGER PRIMARY KEY,
    route_name VARCHAR(100),
    route_line LINESTRING,
    route_type VARCHAR(50)
);

INSERT INTO test_routes VALUES (1, 'Bus Route 1',
    LINESTRING(POINT(-74.0, 40.7), POINT(-73.95, 40.72), POINT(-73.9, 40.74)),
    'Bus'
);
INSERT INTO test_routes VALUES (2, 'Subway Line A',
    LINESTRING(POINT(-74.01, 40.71), POINT(-73.98, 40.73), POINT(-73.95, 40.75)),
    'Subway'
);

-- Find route length
SELECT route_name, route_type,
       ST_Length(route_line) AS route_length
FROM test_routes
ORDER BY route_length DESC;

-- Pipelines/cables
CREATE TABLE test_infrastructure (
    id INTEGER PRIMARY KEY,
    infrastructure_type VARCHAR(50),
    path LINESTRING
);

INSERT INTO test_infrastructure VALUES (1, 'Water Main',
    LINESTRING(POINT(0, 0), POINT(50, 0), POINT(50, 50))
);
INSERT INTO test_infrastructure VALUES (2, 'Gas Pipeline',
    LINESTRING(POINT(10, 0), POINT(10, 60))
);
INSERT INTO test_infrastructure VALUES (3, 'Power Line',
    LINESTRING(POINT(0, 10), POINT(60, 10))
);

-- Find infrastructure intersections
SELECT i1.infrastructure_type AS type1,
       i2.infrastructure_type AS type2,
       ST_AsText(ST_Intersection(i1.path, i2.path)) AS intersection_point
FROM test_infrastructure i1
CROSS JOIN test_infrastructure i2
WHERE i1.id < i2.id AND ST_Intersects(i1.path, i2.path);

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_infrastructure;
DROP TABLE test_routes;
DROP INDEX idx_road_gist;
DROP TABLE test_linestring_index;
DROP TABLE test_linestring_nulls;
DROP TABLE test_linestring_aggregation;
DROP TABLE test_linestring_measurement;
DROP TABLE test_linestring_transform;
DROP TABLE test_linestring_buffer;
DROP TABLE test_linestring_point;
DROP TABLE test_linestring_relationships;
DROP TABLE test_linestring_subset;
DROP TABLE test_linestring_interpolation;
DROP TABLE test_linestring_points;
DROP TABLE test_linestring_properties;
DROP TABLE test_linestring_formats;
DROP TABLE test_linestring_basic;

DROP DATABASE test_spatial_linestring;
