-- ScratchBird Native Test: Spatial Types - POINT
-- Test ID: datatypes_011
-- Description: Comprehensive testing of POINT geometry type

CREATE DATABASE test_spatial_point;
\c test_spatial_point;

-- ============================================================================
-- POINT Basic Testing
-- ============================================================================

CREATE TABLE test_point_basic (
    id INTEGER PRIMARY KEY,
    location POINT,
    name VARCHAR(100)
);

-- Basic 2D points
INSERT INTO test_point_basic VALUES (1, POINT(0, 0), 'Origin');
INSERT INTO test_point_basic VALUES (2, POINT(1, 0), 'Unit X');
INSERT INTO test_point_basic VALUES (3, POINT(0, 1), 'Unit Y');
INSERT INTO test_point_basic VALUES (4, POINT(1, 1), 'Unit diagonal');
INSERT INTO test_point_basic VALUES (5, POINT(-1, -1), 'Negative quadrant');

-- Real-world coordinates (latitude, longitude)
INSERT INTO test_point_basic VALUES (10, POINT(-74.0060, 40.7128), 'New York City');
INSERT INTO test_point_basic VALUES (11, POINT(-118.2437, 34.0522), 'Los Angeles');
INSERT INTO test_point_basic VALUES (12, POINT(2.3522, 48.8566), 'Paris');
INSERT INTO test_point_basic VALUES (13, POINT(139.6917, 35.6895), 'Tokyo');
INSERT INTO test_point_basic VALUES (14, POINT(-0.1276, 51.5074), 'London');

-- Decimal precision
INSERT INTO test_point_basic VALUES (20, POINT(123.456789, 987.654321), 'High precision');
INSERT INTO test_point_basic VALUES (21, POINT(0.000001, 0.000001), 'Very small');
INSERT INTO test_point_basic VALUES (22, POINT(1000000, 2000000), 'Very large');

SELECT id, ST_AsText(location) AS point_wkt, name FROM test_point_basic ORDER BY id;

-- ============================================================================
-- POINT Input Formats
-- ============================================================================

CREATE TABLE test_point_formats (
    id INTEGER PRIMARY KEY,
    point_geom POINT,
    format_description VARCHAR(50)
);

-- WKT (Well-Known Text) format
INSERT INTO test_point_formats VALUES (1, ST_GeomFromText('POINT(10 20)'), 'WKT format');
INSERT INTO test_point_formats VALUES (2, ST_PointFromText('POINT(30 40)'), 'PointFromText');

-- Constructor function
INSERT INTO test_point_formats VALUES (10, ST_MakePoint(50, 60), 'ST_MakePoint');
INSERT INTO test_point_formats VALUES (11, ST_Point(70, 80), 'ST_Point');

-- WKB (Well-Known Binary) - if supported
INSERT INTO test_point_formats VALUES (20, ST_GeomFromWKB(ST_AsBinary(POINT(90, 100))), 'WKB round-trip');

-- GeoJSON - if supported
INSERT INTO test_point_formats VALUES (30, ST_GeomFromGeoJSON('{"type":"Point","coordinates":[110,120]}'), 'GeoJSON');

SELECT id, ST_AsText(point_geom) AS wkt, format_description FROM test_point_formats ORDER BY id;

-- ============================================================================
-- POINT Coordinate Extraction
-- ============================================================================

CREATE TABLE test_point_extraction (
    id INTEGER PRIMARY KEY,
    location POINT
);

INSERT INTO test_point_extraction VALUES (1, POINT(123.456, 789.012));
INSERT INTO test_point_extraction VALUES (2, POINT(-45.678, 90.123));
INSERT INTO test_point_extraction VALUES (3, POINT(0, 0));

-- Extract X and Y coordinates
SELECT id,
       ST_AsText(location) AS point,
       ST_X(location) AS x_coord,
       ST_Y(location) AS y_coord
FROM test_point_extraction
ORDER BY id;

-- Set coordinates
SELECT id,
       ST_AsText(location) AS original,
       ST_AsText(ST_SetX(location, 100)) AS new_x,
       ST_AsText(ST_SetY(location, 200)) AS new_y
FROM test_point_extraction
WHERE id = 1;

-- ============================================================================
-- POINT Distance Calculations
-- ============================================================================

CREATE TABLE test_point_distance (
    id INTEGER PRIMARY KEY,
    city VARCHAR(50),
    coordinates POINT
);

INSERT INTO test_point_distance VALUES (1, 'New York', POINT(-74.0060, 40.7128));
INSERT INTO test_point_distance VALUES (2, 'Los Angeles', POINT(-118.2437, 34.0522));
INSERT INTO test_point_distance VALUES (3, 'Chicago', POINT(-87.6298, 41.8781));
INSERT INTO test_point_distance VALUES (4, 'Houston', POINT(-95.3698, 29.7604));
INSERT INTO test_point_distance VALUES (5, 'Phoenix', POINT(-112.0740, 33.4484));

-- Euclidean distance between points
SELECT p1.city AS city1,
       p2.city AS city2,
       ST_Distance(p1.coordinates, p2.coordinates) AS euclidean_distance
FROM test_point_distance p1
CROSS JOIN test_point_distance p2
WHERE p1.id < p2.id
ORDER BY euclidean_distance;

-- Distance from a specific point (e.g., New York)
SELECT city,
       ST_Distance(coordinates, POINT(-74.0060, 40.7128)) AS dist_from_ny
FROM test_point_distance
WHERE city != 'New York'
ORDER BY dist_from_ny;

-- Geographic distance (Great Circle) - if supported
SELECT p1.city AS city1,
       p2.city AS city2,
       ST_Distance_Sphere(p1.coordinates, p2.coordinates) / 1000 AS km_distance
FROM test_point_distance p1
CROSS JOIN test_point_distance p2
WHERE p1.id < p2.id
ORDER BY km_distance;

-- ============================================================================
-- POINT Spatial Relationships
-- ============================================================================

CREATE TABLE test_point_relationships (
    id INTEGER PRIMARY KEY,
    point POINT,
    description VARCHAR(50)
);

INSERT INTO test_point_relationships VALUES (1, POINT(0, 0), 'Origin');
INSERT INTO test_point_relationships VALUES (2, POINT(5, 5), 'Diagonal');
INSERT INTO test_point_relationships VALUES (3, POINT(10, 10), 'Far diagonal');

-- Equality
SELECT p1.id AS id1, p2.id AS id2
FROM test_point_relationships p1
JOIN test_point_relationships p2 ON ST_Equals(p1.point, p2.point)
WHERE p1.id < p2.id;

-- Within distance
SELECT id, description
FROM test_point_relationships
WHERE ST_DWithin(point, POINT(0, 0), 10)
ORDER BY id;

-- Nearest neighbor
SELECT id, description,
       ST_Distance(point, POINT(7, 7)) AS distance
FROM test_point_relationships
ORDER BY distance
LIMIT 1;

-- ============================================================================
-- POINT Transformations
-- ============================================================================

CREATE TABLE test_point_transform (
    id INTEGER PRIMARY KEY,
    original POINT
);

INSERT INTO test_point_transform VALUES (1, POINT(10, 20));
INSERT INTO test_point_transform VALUES (2, POINT(-5, 15));

-- Translate (move) point
SELECT id,
       ST_AsText(original) AS original_point,
       ST_AsText(ST_Translate(original, 5, -3)) AS translated
FROM test_point_transform
ORDER BY id;

-- Scale point
SELECT id,
       ST_AsText(original) AS original_point,
       ST_AsText(ST_Scale(original, 2, 2)) AS scaled_2x
FROM test_point_transform
ORDER BY id;

-- Rotate point around origin (if supported)
SELECT id,
       ST_AsText(original) AS original_point,
       ST_AsText(ST_Rotate(original, PI() / 4)) AS rotated_45deg
FROM test_point_transform
ORDER BY id;

-- ============================================================================
-- POINT Comparison and Sorting
-- ============================================================================

CREATE TABLE test_point_sorting (
    id INTEGER PRIMARY KEY,
    location POINT,
    label VARCHAR(10)
);

INSERT INTO test_point_sorting VALUES (1, POINT(5, 5), 'E');
INSERT INTO test_point_sorting VALUES (2, POINT(1, 1), 'A');
INSERT INTO test_point_sorting VALUES (3, POINT(3, 3), 'C');
INSERT INTO test_point_sorting VALUES (4, POINT(2, 2), 'B');
INSERT INTO test_point_sorting VALUES (5, POINT(4, 4), 'D');

-- Sort by X coordinate
SELECT id, label, ST_AsText(location) AS point
FROM test_point_sorting
ORDER BY ST_X(location), ST_Y(location);

-- Sort by distance from origin
SELECT id, label, ST_AsText(location) AS point,
       ST_Distance(location, POINT(0, 0)) AS dist_from_origin
FROM test_point_sorting
ORDER BY dist_from_origin;

-- ============================================================================
-- POINT Bounding Box and Envelope
-- ============================================================================

CREATE TABLE test_point_bounds (
    id INTEGER PRIMARY KEY,
    point POINT
);

INSERT INTO test_point_bounds VALUES (1, POINT(10, 20));
INSERT INTO test_point_bounds VALUES (2, POINT(30, 40));
INSERT INTO test_point_bounds VALUES (3, POINT(5, 15));
INSERT INTO test_point_bounds VALUES (4, POINT(25, 35));

-- Envelope (bounding box) of a point
SELECT id, ST_AsText(point) AS point,
       ST_AsText(ST_Envelope(point)) AS envelope
FROM test_point_bounds
WHERE id = 1;

-- Extent of all points (aggregate)
SELECT ST_AsText(ST_Extent(point)) AS bounding_box
FROM test_point_bounds;

-- ============================================================================
-- POINT Buffers
-- ============================================================================

CREATE TABLE test_point_buffer (
    id INTEGER PRIMARY KEY,
    center POINT
);

INSERT INTO test_point_buffer VALUES (1, POINT(0, 0));
INSERT INTO test_point_buffer VALUES (2, POINT(10, 10));

-- Create buffer (circle) around point
SELECT id,
       ST_AsText(center) AS center_point,
       ST_AsText(ST_Buffer(center, 5)) AS buffer_5,
       ST_Area(ST_Buffer(center, 5)) AS buffer_area
FROM test_point_buffer
ORDER BY id;

-- Different buffer sizes
SELECT id,
       ST_AsText(center) AS center_point,
       ST_Area(ST_Buffer(center, 1)) AS area_r1,
       ST_Area(ST_Buffer(center, 5)) AS area_r5,
       ST_Area(ST_Buffer(center, 10)) AS area_r10
FROM test_point_buffer
WHERE id = 1;

-- ============================================================================
-- POINT Validity and Dimension
-- ============================================================================

CREATE TABLE test_point_properties (
    id INTEGER PRIMARY KEY,
    geom POINT
);

INSERT INTO test_point_properties VALUES (1, POINT(10, 20));
INSERT INTO test_point_properties VALUES (2, POINT(0, 0));

-- Geometry properties
SELECT id,
       ST_AsText(geom) AS point,
       ST_GeometryType(geom) AS geom_type,
       ST_Dimension(geom) AS dimension,
       ST_IsValid(geom) AS is_valid,
       ST_IsSimple(geom) AS is_simple,
       ST_IsEmpty(geom) AS is_empty
FROM test_point_properties
ORDER BY id;

-- SRID (Spatial Reference ID)
SELECT id,
       ST_AsText(geom) AS point,
       ST_SRID(geom) AS srid
FROM test_point_properties
ORDER BY id;

-- Set SRID (e.g., 4326 for WGS84)
SELECT id,
       ST_AsText(geom) AS point,
       ST_SRID(ST_SetSRID(geom, 4326)) AS new_srid
FROM test_point_properties
WHERE id = 1;

-- ============================================================================
-- POINT Aggregations
-- ============================================================================

CREATE TABLE test_point_aggregation (
    id INTEGER PRIMARY KEY,
    category VARCHAR(20),
    location POINT
);

INSERT INTO test_point_aggregation VALUES (1, 'Group A', POINT(1, 1));
INSERT INTO test_point_aggregation VALUES (2, 'Group A', POINT(3, 3));
INSERT INTO test_point_aggregation VALUES (3, 'Group A', POINT(5, 5));
INSERT INTO test_point_aggregation VALUES (4, 'Group B', POINT(10, 10));
INSERT INTO test_point_aggregation VALUES (5, 'Group B', POINT(12, 12));

-- Collect points into a geometry collection
SELECT category,
       ST_AsText(ST_Collect(location)) AS collected_points,
       COUNT(*) AS point_count
FROM test_point_aggregation
GROUP BY category
ORDER BY category;

-- Centroid (center) of point collection
SELECT category,
       ST_AsText(ST_Centroid(ST_Collect(location))) AS centroid
FROM test_point_aggregation
GROUP BY category
ORDER BY category;

-- ============================================================================
-- POINT NULL Handling
-- ============================================================================

CREATE TABLE test_point_nulls (
    id INTEGER PRIMARY KEY,
    nullable_point POINT,
    description VARCHAR(50)
);

INSERT INTO test_point_nulls VALUES (1, NULL, 'No location');
INSERT INTO test_point_nulls VALUES (2, POINT(10, 20), 'Has location');
INSERT INTO test_point_nulls VALUES (3, NULL, 'Another null');

SELECT COUNT(*) AS null_count FROM test_point_nulls WHERE nullable_point IS NULL;
SELECT COUNT(*) AS not_null_count FROM test_point_nulls WHERE nullable_point IS NOT NULL;

-- COALESCE with default point
SELECT id, description,
       ST_AsText(COALESCE(nullable_point, POINT(0, 0))) AS point_with_default
FROM test_point_nulls
ORDER BY id;

-- ============================================================================
-- POINT Indexing
-- ============================================================================

CREATE TABLE test_point_index (
    id INTEGER PRIMARY KEY,
    location POINT,
    place_name VARCHAR(100)
);

-- Create spatial index
CREATE INDEX idx_location_gist ON test_point_index USING GIST (location);

INSERT INTO test_point_index VALUES (1, POINT(-74.0060, 40.7128), 'New York');
INSERT INTO test_point_index VALUES (2, POINT(-118.2437, 34.0522), 'Los Angeles');
INSERT INTO test_point_index VALUES (3, POINT(-87.6298, 41.8781), 'Chicago');
INSERT INTO test_point_index VALUES (4, POINT(2.3522, 48.8566), 'Paris');
INSERT INTO test_point_index VALUES (5, POINT(139.6917, 35.6895), 'Tokyo');

-- Spatial query using index
SELECT place_name
FROM test_point_index
WHERE ST_DWithin(location, POINT(-74.0060, 40.7128), 10)
ORDER BY place_name;

-- Nearest neighbor query
SELECT place_name,
       ST_Distance(location, POINT(0, 50)) AS distance
FROM test_point_index
ORDER BY distance
LIMIT 3;

-- ============================================================================
-- POINT Real-World Use Cases
-- ============================================================================

-- Store locations
CREATE TABLE test_stores (
    id INTEGER PRIMARY KEY,
    store_name VARCHAR(100),
    location POINT,
    category VARCHAR(50)
);

INSERT INTO test_stores VALUES (1, 'Store A', POINT(-73.9, 40.7), 'Electronics');
INSERT INTO test_stores VALUES (2, 'Store B', POINT(-74.0, 40.7), 'Groceries');
INSERT INTO test_stores VALUES (3, 'Store C', POINT(-74.1, 40.8), 'Electronics');
INSERT INTO test_stores VALUES (4, 'Store D', POINT(-73.8, 40.6), 'Clothing');

-- Find nearest stores to a location
SELECT store_name, category,
       ST_Distance(location, POINT(-74.0, 40.7)) AS distance
FROM test_stores
ORDER BY distance
LIMIT 3;

-- Find stores within a radius
SELECT store_name, category
FROM test_stores
WHERE ST_DWithin(location, POINT(-74.0, 40.7), 0.2)
ORDER BY store_name;

-- GPS tracking
CREATE TABLE test_gps_track (
    id INTEGER PRIMARY KEY,
    timestamp TIMESTAMP,
    location POINT,
    speed FLOAT
);

INSERT INTO test_gps_track VALUES (1, TIMESTAMP '2024-01-01 10:00:00', POINT(-74.0, 40.7), 30.5);
INSERT INTO test_gps_track VALUES (2, TIMESTAMP '2024-01-01 10:05:00', POINT(-74.01, 40.71), 35.2);
INSERT INTO test_gps_track VALUES (3, TIMESTAMP '2024-01-01 10:10:00', POINT(-74.02, 40.72), 28.8);
INSERT INTO test_gps_track VALUES (4, TIMESTAMP '2024-01-01 10:15:00', POINT(-74.03, 40.73), 42.1);

-- Calculate distance traveled
SELECT id,
       timestamp,
       ST_AsText(location) AS position,
       speed,
       LAG(location) OVER (ORDER BY timestamp) AS prev_location,
       ST_Distance(location, LAG(location) OVER (ORDER BY timestamp)) AS distance_from_prev
FROM test_gps_track
ORDER BY timestamp;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_gps_track;
DROP TABLE test_stores;
DROP INDEX idx_location_gist;
DROP TABLE test_point_index;
DROP TABLE test_point_nulls;
DROP TABLE test_point_aggregation;
DROP TABLE test_point_properties;
DROP TABLE test_point_buffer;
DROP TABLE test_point_bounds;
DROP TABLE test_point_sorting;
DROP TABLE test_point_transform;
DROP TABLE test_point_relationships;
DROP TABLE test_point_distance;
DROP TABLE test_point_extraction;
DROP TABLE test_point_formats;
DROP TABLE test_point_basic;

DROP DATABASE test_spatial_point;
