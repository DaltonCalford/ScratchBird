-- ScratchBird Native Test: Spatial Types - MULTIPOINT
-- Test ID: datatypes_014
-- Description: Comprehensive testing of MULTIPOINT geometry type

CREATE DATABASE test_spatial_multipoint;
\c test_spatial_multipoint;

-- ============================================================================
-- MULTIPOINT Basic Testing
-- ============================================================================

CREATE TABLE test_multipoint_basic (
    id INTEGER PRIMARY KEY,
    points MULTIPOINT,
    name VARCHAR(100)
);

-- Simple multipoints
INSERT INTO test_multipoint_basic VALUES (1,
    MULTIPOINT(POINT(0, 0), POINT(1, 1)),
    'Two points'
);

INSERT INTO test_multipoint_basic VALUES (2,
    MULTIPOINT(POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10)),
    'Square corners'
);

INSERT INTO test_multipoint_basic VALUES (3,
    MULTIPOINT(POINT(5, 5), POINT(10, 5), POINT(15, 5)),
    'Horizontal line of points'
);

-- Many points
INSERT INTO test_multipoint_basic VALUES (10,
    ST_Collect(ARRAY[POINT(0,0), POINT(1,1), POINT(2,2), POINT(3,3), POINT(4,4),
                     POINT(5,5), POINT(6,6), POINT(7,7), POINT(8,8), POINT(9,9)]),
    'Diagonal points'
);

SELECT id, ST_AsText(points) AS multipoint_wkt, name FROM test_multipoint_basic ORDER BY id;

-- ============================================================================
-- MULTIPOINT Input Formats
-- ============================================================================

CREATE TABLE test_multipoint_formats (
    id INTEGER PRIMARY KEY,
    mp_geom MULTIPOINT,
    format_description VARCHAR(50)
);

-- WKT format
INSERT INTO test_multipoint_formats VALUES (1,
    ST_GeomFromText('MULTIPOINT((0 0), (10 10), (20 20))'),
    'WKT format'
);

INSERT INTO test_multipoint_formats VALUES (2,
    ST_MPointFromText('MULTIPOINT((5 5), (15 15))'),
    'MPointFromText'
);

-- ST_Collect from points
INSERT INTO test_multipoint_formats VALUES (10,
    ST_Collect(ARRAY[POINT(1, 2), POINT(3, 4), POINT(5, 6)]),
    'ST_Collect from array'
);

-- Build from individual geometries
INSERT INTO test_multipoint_formats VALUES (11,
    ST_Collect(ST_Collect(POINT(10, 10), POINT(20, 20)), POINT(30, 30)),
    'ST_Collect nested'
);

SELECT id, ST_AsText(mp_geom) AS wkt, format_description FROM test_multipoint_formats ORDER BY id;

-- ============================================================================
-- MULTIPOINT Properties
-- ============================================================================

CREATE TABLE test_multipoint_properties (
    id INTEGER PRIMARY KEY,
    points MULTIPOINT
);

INSERT INTO test_multipoint_properties VALUES (1, MULTIPOINT(POINT(0, 0), POINT(10, 10)));
INSERT INTO test_multipoint_properties VALUES (2, MULTIPOINT(POINT(0, 0), POINT(5, 0), POINT(10, 0)));
INSERT INTO test_multipoint_properties VALUES (3, MULTIPOINT(POINT(0, 0), POINT(0, 10), POINT(10, 10), POINT(10, 0)));

-- Basic properties
SELECT id,
       ST_AsText(points) AS multipoint,
       ST_GeometryType(points) AS geom_type,
       ST_Dimension(points) AS dimension,
       ST_IsValid(points) AS is_valid,
       ST_IsSimple(points) AS is_simple,
       ST_IsEmpty(points) AS is_empty
FROM test_multipoint_properties
ORDER BY id;

-- Number of geometries (points)
SELECT id,
       ST_AsText(points) AS multipoint,
       ST_NumGeometries(points) AS num_points
FROM test_multipoint_properties
ORDER BY id;

-- Number of points (total)
SELECT id,
       ST_AsText(points) AS multipoint,
       ST_NPoints(points) AS total_points
FROM test_multipoint_properties
ORDER BY id;

-- ============================================================================
-- MULTIPOINT Point Extraction
-- ============================================================================

CREATE TABLE test_multipoint_extraction (
    id INTEGER PRIMARY KEY,
    points MULTIPOINT
);

INSERT INTO test_multipoint_extraction VALUES (1,
    MULTIPOINT(POINT(10, 20), POINT(30, 40), POINT(50, 60))
);

-- Extract individual points by index (1-based)
SELECT id,
       ST_AsText(points) AS multipoint,
       ST_AsText(ST_GeometryN(points, 1)) AS first_point,
       ST_AsText(ST_GeometryN(points, 2)) AS second_point,
       ST_AsText(ST_GeometryN(points, 3)) AS third_point
FROM test_multipoint_extraction;

-- Extract all points
SELECT id,
       n AS point_index,
       ST_AsText(ST_GeometryN(points, n)) AS point,
       ST_X(ST_GeometryN(points, n)) AS x_coord,
       ST_Y(ST_GeometryN(points, n)) AS y_coord
FROM test_multipoint_extraction,
     generate_series(1, ST_NumGeometries(points)) AS n
ORDER BY id, n;

-- ============================================================================
-- MULTIPOINT Bounding Box and Extent
-- ============================================================================

CREATE TABLE test_multipoint_bounds (
    id INTEGER PRIMARY KEY,
    points MULTIPOINT
);

INSERT INTO test_multipoint_bounds VALUES (1, MULTIPOINT(POINT(10, 20), POINT(50, 60), POINT(30, 40)));
INSERT INTO test_multipoint_bounds VALUES (2, MULTIPOINT(POINT(0, 0), POINT(100, 0), POINT(50, 100)));

-- Envelope (bounding box)
SELECT id,
       ST_AsText(points) AS multipoint,
       ST_AsText(ST_Envelope(points)) AS bounding_box
FROM test_multipoint_bounds
ORDER BY id;

-- Extent (aggregate bounding box)
SELECT ST_AsText(ST_Extent(points)) AS total_extent
FROM test_multipoint_bounds;

-- Centroid
SELECT id,
       ST_AsText(points) AS multipoint,
       ST_AsText(ST_Centroid(points)) AS centroid
FROM test_multipoint_bounds
ORDER BY id;

-- ============================================================================
-- MULTIPOINT Spatial Relationships
-- ============================================================================

CREATE TABLE test_multipoint_relationships (
    id INTEGER PRIMARY KEY,
    points MULTIPOINT,
    name VARCHAR(50)
);

INSERT INTO test_multipoint_relationships VALUES (1,
    MULTIPOINT(POINT(0, 0), POINT(5, 5), POINT(10, 10)),
    'Diagonal points'
);

INSERT INTO test_multipoint_relationships VALUES (2,
    MULTIPOINT(POINT(5, 5), POINT(15, 15)),
    'Overlapping points'
);

INSERT INTO test_multipoint_relationships VALUES (3,
    MULTIPOINT(POINT(20, 20), POINT(25, 25)),
    'Distant points'
);

-- Intersection
SELECT mp1.name AS multipoint1, mp2.name AS multipoint2,
       ST_Intersects(mp1.points, mp2.points) AS intersects
FROM test_multipoint_relationships mp1
CROSS JOIN test_multipoint_relationships mp2
WHERE mp1.id < mp2.id;

-- Distance
SELECT mp1.name AS multipoint1, mp2.name AS multipoint2,
       ST_Distance(mp1.points, mp2.points) AS min_distance
FROM test_multipoint_relationships mp1
CROSS JOIN test_multipoint_relationships mp2
WHERE mp1.id < mp2.id
ORDER BY min_distance;

-- ============================================================================
-- MULTIPOINT with POLYGON Relationships
-- ============================================================================

CREATE TABLE test_multipoint_polygon (
    id INTEGER PRIMARY KEY,
    points MULTIPOINT,
    boundary POLYGON
);

INSERT INTO test_multipoint_polygon VALUES (1,
    MULTIPOINT(POINT(5, 5), POINT(15, 15), POINT(25, 25)),
    POLYGON((POINT(0, 0), POINT(20, 0), POINT(20, 20), POINT(0, 20), POINT(0, 0)))
);

-- Points within polygon
SELECT id,
       ST_AsText(points) AS multipoint,
       ST_AsText(boundary) AS polygon,
       ST_Contains(boundary, points) AS polygon_contains_all_points,
       ST_Intersects(boundary, points) AS polygon_intersects_points
FROM test_multipoint_polygon;

-- Filter points within polygon
SELECT id,
       ST_AsText(ST_Intersection(points, boundary)) AS points_within
FROM test_multipoint_polygon;

-- ============================================================================
-- MULTIPOINT Buffers
-- ============================================================================

CREATE TABLE test_multipoint_buffer (
    id INTEGER PRIMARY KEY,
    points MULTIPOINT
);

INSERT INTO test_multipoint_buffer VALUES (1, MULTIPOINT(POINT(0, 0), POINT(10, 0), POINT(10, 10)));

-- Create buffer around all points
SELECT id,
       ST_AsText(points) AS multipoint,
       ST_Area(ST_Buffer(points, 2)) AS buffer_area_2,
       ST_Area(ST_Buffer(points, 5)) AS buffer_area_5
FROM test_multipoint_buffer;

-- Union of individual point buffers
SELECT id,
       ST_Area(ST_Union(ST_Buffer(ST_GeometryN(points, n), 3))) AS union_buffer_area
FROM test_multipoint_buffer,
     generate_series(1, ST_NumGeometries(points)) AS n
GROUP BY id;

-- ============================================================================
-- MULTIPOINT Convex Hull
-- ============================================================================

CREATE TABLE test_multipoint_hull (
    id INTEGER PRIMARY KEY,
    points MULTIPOINT
);

INSERT INTO test_multipoint_hull VALUES (1,
    MULTIPOINT(POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(5, 5))
);

-- Convex hull of points
SELECT id,
       ST_AsText(points) AS multipoint,
       ST_AsText(ST_ConvexHull(points)) AS convex_hull,
       ST_GeometryType(ST_ConvexHull(points)) AS hull_type
FROM test_multipoint_hull;

-- ============================================================================
-- MULTIPOINT Aggregation
-- ============================================================================

CREATE TABLE test_multipoint_aggregation (
    id INTEGER PRIMARY KEY,
    category VARCHAR(20),
    points MULTIPOINT
);

INSERT INTO test_multipoint_aggregation VALUES (1, 'Group A', MULTIPOINT(POINT(0, 0), POINT(5, 5)));
INSERT INTO test_multipoint_aggregation VALUES (2, 'Group A', MULTIPOINT(POINT(10, 10), POINT(15, 15)));
INSERT INTO test_multipoint_aggregation VALUES (3, 'Group B', MULTIPOINT(POINT(20, 20), POINT(25, 25)));

-- Collect all points by category
SELECT category,
       ST_AsText(ST_Collect(points)) AS all_points,
       SUM(ST_NumGeometries(points)) AS total_point_count
FROM test_multipoint_aggregation
GROUP BY category
ORDER BY category;

-- Union (same as collect for points)
SELECT category,
       ST_AsText(ST_Union(points)) AS union_points
FROM test_multipoint_aggregation
GROUP BY category
ORDER BY category;

-- ============================================================================
-- MULTIPOINT NULL Handling
-- ============================================================================

CREATE TABLE test_multipoint_nulls (
    id INTEGER PRIMARY KEY,
    nullable_points MULTIPOINT,
    description VARCHAR(50)
);

INSERT INTO test_multipoint_nulls VALUES (1, NULL, 'No points');
INSERT INTO test_multipoint_nulls VALUES (2, MULTIPOINT(POINT(10, 20), POINT(30, 40)), 'Has points');
INSERT INTO test_multipoint_nulls VALUES (3, NULL, 'Another null');

SELECT COUNT(*) AS null_count FROM test_multipoint_nulls WHERE nullable_points IS NULL;

SELECT id, description,
       ST_AsText(COALESCE(nullable_points, MULTIPOINT(POINT(0, 0)))) AS points_with_default
FROM test_multipoint_nulls
ORDER BY id;

-- ============================================================================
-- MULTIPOINT Indexing
-- ============================================================================

CREATE TABLE test_multipoint_index (
    id INTEGER PRIMARY KEY,
    locations MULTIPOINT,
    location_set_name VARCHAR(100)
);

CREATE INDEX idx_locations_gist ON test_multipoint_index USING GIST (locations);

INSERT INTO test_multipoint_index VALUES (1,
    MULTIPOINT(POINT(-74.0, 40.7), POINT(-74.01, 40.71), POINT(-74.02, 40.72)),
    'NYC Cluster 1'
);
INSERT INTO test_multipoint_index VALUES (2,
    MULTIPOINT(POINT(-118.24, 34.05), POINT(-118.25, 34.06)),
    'LA Cluster 1'
);

-- Find location sets intersecting a point
SELECT location_set_name
FROM test_multipoint_index
WHERE ST_Intersects(locations, POINT(-74.01, 40.71))
ORDER BY location_set_name;

-- Find location sets within distance
SELECT location_set_name,
       ST_Distance(locations, POINT(-74.0, 40.7)) AS distance
FROM test_multipoint_index
WHERE ST_DWithin(locations, POINT(-74.0, 40.7), 0.1)
ORDER BY distance;

-- ============================================================================
-- MULTIPOINT Real-World Use Cases
-- ============================================================================

-- Store chains/franchises
CREATE TABLE test_store_chains (
    id INTEGER PRIMARY KEY,
    chain_name VARCHAR(100),
    store_locations MULTIPOINT
);

INSERT INTO test_store_chains VALUES (1, 'Coffee Chain A',
    MULTIPOINT(POINT(-74.0, 40.7), POINT(-74.01, 40.71), POINT(-74.02, 40.72),
               POINT(-73.99, 40.69), POINT(-74.03, 40.73))
);
INSERT INTO test_store_chains VALUES (2, 'Restaurant Chain B',
    MULTIPOINT(POINT(-74.005, 40.705), POINT(-74.015, 40.715))
);

-- Count stores per chain
SELECT chain_name,
       ST_NumGeometries(store_locations) AS store_count
FROM test_store_chains
ORDER BY store_count DESC;

-- Find chains with stores near a location
SELECT chain_name,
       ST_Distance(store_locations, POINT(-74.01, 40.71)) AS nearest_store_distance
FROM test_store_chains
WHERE ST_DWithin(store_locations, POINT(-74.01, 40.71), 0.05)
ORDER BY nearest_store_distance;

-- Traffic camera locations
CREATE TABLE test_traffic_cameras (
    id INTEGER PRIMARY KEY,
    zone_name VARCHAR(100),
    camera_locations MULTIPOINT
);

INSERT INTO test_traffic_cameras VALUES (1, 'Downtown',
    MULTIPOINT(POINT(10, 10), POINT(15, 15), POINT(20, 20),
               POINT(12, 18), POINT(18, 12))
);

-- Coverage area (convex hull)
SELECT zone_name,
       ST_NumGeometries(camera_locations) AS camera_count,
       ST_Area(ST_ConvexHull(camera_locations)) AS coverage_area
FROM test_traffic_cameras;

-- Sensor networks
CREATE TABLE test_sensor_network (
    id INTEGER PRIMARY KEY,
    network_name VARCHAR(100),
    sensor_positions MULTIPOINT
);

INSERT INTO test_sensor_network VALUES (1, 'Environmental Sensors',
    MULTIPOINT(POINT(0, 0), POINT(10, 0), POINT(20, 0),
               POINT(0, 10), POINT(10, 10), POINT(20, 10))
);

-- Average sensor spacing
SELECT network_name,
       ST_NumGeometries(sensor_positions) AS sensor_count,
       ST_AsText(ST_Centroid(sensor_positions)) AS network_center
FROM test_sensor_network;

-- GPS waypoints/tracks
CREATE TABLE test_gps_waypoints (
    id INTEGER PRIMARY KEY,
    track_name VARCHAR(100),
    waypoints MULTIPOINT,
    recorded_date DATE
);

INSERT INTO test_gps_waypoints VALUES (1, 'Morning Jog',
    MULTIPOINT(POINT(-74.0, 40.7), POINT(-74.005, 40.705), POINT(-74.01, 40.71),
               POINT(-74.015, 40.715), POINT(-74.02, 40.72)),
    DATE '2024-01-15'
);

-- Track statistics
SELECT track_name, recorded_date,
       ST_NumGeometries(waypoints) AS waypoint_count,
       ST_Distance(ST_StartPoint(ST_MakeLine(waypoints)),
                   ST_EndPoint(ST_MakeLine(waypoints))) AS straight_line_distance
FROM test_gps_waypoints;

-- ============================================================================
-- MULTIPOINT Distance Matrix
-- ============================================================================

CREATE TABLE test_multipoint_distance_matrix (
    id INTEGER PRIMARY KEY,
    points MULTIPOINT
);

INSERT INTO test_multipoint_distance_matrix VALUES (1,
    MULTIPOINT(POINT(0, 0), POINT(10, 0), POINT(0, 10))
);

-- Distance between all pairs of points
SELECT p1.point_index AS point1_idx,
       p2.point_index AS point2_idx,
       ST_AsText(p1.point) AS point1,
       ST_AsText(p2.point) AS point2,
       ST_Distance(p1.point, p2.point) AS distance
FROM (
    SELECT id, n AS point_index, ST_GeometryN(points, n) AS point
    FROM test_multipoint_distance_matrix,
         generate_series(1, ST_NumGeometries(points)) AS n
) p1
CROSS JOIN (
    SELECT id, n AS point_index, ST_GeometryN(points, n) AS point
    FROM test_multipoint_distance_matrix,
         generate_series(1, ST_NumGeometries(points)) AS n
) p2
WHERE p1.id = p2.id AND p1.point_index < p2.point_index
ORDER BY distance;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_multipoint_distance_matrix;
DROP TABLE test_gps_waypoints;
DROP TABLE test_sensor_network;
DROP TABLE test_traffic_cameras;
DROP TABLE test_store_chains;
DROP INDEX idx_locations_gist;
DROP TABLE test_multipoint_index;
DROP TABLE test_multipoint_nulls;
DROP TABLE test_multipoint_aggregation;
DROP TABLE test_multipoint_hull;
DROP TABLE test_multipoint_buffer;
DROP TABLE test_multipoint_polygon;
DROP TABLE test_multipoint_relationships;
DROP TABLE test_multipoint_bounds;
DROP TABLE test_multipoint_extraction;
DROP TABLE test_multipoint_properties;
DROP TABLE test_multipoint_formats;
DROP TABLE test_multipoint_basic;

DROP DATABASE test_spatial_multipoint;
