-- ScratchBird Native Test: Spatial Types - MULTILINESTRING
-- Test ID: datatypes_015
-- Description: Comprehensive testing of MULTILINESTRING geometry type

CREATE DATABASE test_spatial_multilinestring;
\c test_spatial_multilinestring;

-- ============================================================================
-- MULTILINESTRING Basic Testing
-- ============================================================================

CREATE TABLE test_multilinestring_basic (
    id INTEGER PRIMARY KEY,
    lines MULTILINESTRING,
    name VARCHAR(100)
);

-- Simple multilinestrings
INSERT INTO test_multilinestring_basic VALUES (1,
    MULTILINESTRING(
        (POINT(0, 0), POINT(10, 10)),
        (POINT(0, 10), POINT(10, 0))
    ),
    'X shape - two crossing lines'
);

INSERT INTO test_multilinestring_basic VALUES (2,
    MULTILINESTRING(
        (POINT(0, 0), POINT(10, 0)),
        (POINT(0, 5), POINT(10, 5)),
        (POINT(0, 10), POINT(10, 10))
    ),
    'Three parallel horizontal lines'
);

INSERT INTO test_multilinestring_basic VALUES (3,
    MULTILINESTRING(
        (POINT(0, 0), POINT(5, 5), POINT(10, 0)),
        (POINT(10, 0), POINT(15, 5), POINT(20, 0))
    ),
    'Connected zigzag'
);

SELECT id, ST_AsText(lines) AS multilinestring_wkt, name FROM test_multilinestring_basic ORDER BY id;

-- ============================================================================
-- MULTILINESTRING Input Formats
-- ============================================================================

CREATE TABLE test_multilinestring_formats (
    id INTEGER PRIMARY KEY,
    mls_geom MULTILINESTRING,
    format_description VARCHAR(50)
);

-- WKT format
INSERT INTO test_multilinestring_formats VALUES (1,
    ST_GeomFromText('MULTILINESTRING((0 0, 10 10), (20 20, 30 30))'),
    'WKT format'
);

INSERT INTO test_multilinestring_formats VALUES (2,
    ST_MLineFromText('MULTILINESTRING((5 5, 15 15), (25 25, 35 35))'),
    'MLineFromText'
);

-- ST_Collect from linestrings
INSERT INTO test_multilinestring_formats VALUES (10,
    ST_Collect(ARRAY[
        LINESTRING(POINT(0, 0), POINT(10, 0)),
        LINESTRING(POINT(0, 5), POINT(10, 5))
    ]),
    'ST_Collect from array'
);

-- Build incrementally
INSERT INTO test_multilinestring_formats VALUES (11,
    ST_Collect(
        ST_Collect(
            LINESTRING(POINT(0, 0), POINT(5, 5)),
            LINESTRING(POINT(10, 0), POINT(15, 5))
        ),
        LINESTRING(POINT(20, 0), POINT(25, 5))
    ),
    'ST_Collect nested'
);

SELECT id, ST_AsText(mls_geom) AS wkt, format_description FROM test_multilinestring_formats ORDER BY id;

-- ============================================================================
-- MULTILINESTRING Properties
-- ============================================================================

CREATE TABLE test_multilinestring_properties (
    id INTEGER PRIMARY KEY,
    lines MULTILINESTRING
);

INSERT INTO test_multilinestring_properties VALUES (1,
    MULTILINESTRING((POINT(0, 0), POINT(10, 0)), (POINT(0, 10), POINT(10, 10)))
);

INSERT INTO test_multilinestring_properties VALUES (2,
    MULTILINESTRING((POINT(0, 0), POINT(5, 5), POINT(10, 0)))
);

-- Basic properties
SELECT id,
       ST_AsText(lines) AS multilinestring,
       ST_GeometryType(lines) AS geom_type,
       ST_Dimension(lines) AS dimension,
       ST_IsValid(lines) AS is_valid,
       ST_IsSimple(lines) AS is_simple,
       ST_IsClosed(lines) AS is_closed
FROM test_multilinestring_properties
ORDER BY id;

-- Number of linestrings
SELECT id,
       ST_AsText(lines) AS multilinestring,
       ST_NumGeometries(lines) AS num_linestrings
FROM test_multilinestring_properties
ORDER BY id;

-- Total length
SELECT id,
       ST_AsText(lines) AS multilinestring,
       ST_Length(lines) AS total_length
FROM test_multilinestring_properties
ORDER BY id;

-- Total number of points
SELECT id,
       ST_NPoints(lines) AS total_points
FROM test_multilinestring_properties
ORDER BY id;

-- ============================================================================
-- MULTILINESTRING Line Extraction
-- ============================================================================

CREATE TABLE test_multilinestring_extraction (
    id INTEGER PRIMARY KEY,
    lines MULTILINESTRING
);

INSERT INTO test_multilinestring_extraction VALUES (1,
    MULTILINESTRING(
        (POINT(0, 0), POINT(10, 0)),
        (POINT(0, 5), POINT(10, 5)),
        (POINT(0, 10), POINT(10, 10))
    )
);

-- Extract individual linestrings
SELECT id,
       ST_AsText(lines) AS multilinestring,
       ST_AsText(ST_GeometryN(lines, 1)) AS first_line,
       ST_AsText(ST_GeometryN(lines, 2)) AS second_line,
       ST_AsText(ST_GeometryN(lines, 3)) AS third_line
FROM test_multilinestring_extraction;

-- Extract all linestrings with properties
SELECT id,
       n AS line_index,
       ST_AsText(ST_GeometryN(lines, n)) AS linestring,
       ST_Length(ST_GeometryN(lines, n)) AS line_length,
       ST_NPoints(ST_GeometryN(lines, n)) AS line_points
FROM test_multilinestring_extraction,
     generate_series(1, ST_NumGeometries(lines)) AS n
ORDER BY id, n;

-- ============================================================================
-- MULTILINESTRING Spatial Relationships
-- ============================================================================

CREATE TABLE test_multilinestring_relationships (
    id INTEGER PRIMARY KEY,
    lines MULTILINESTRING,
    name VARCHAR(50)
);

INSERT INTO test_multilinestring_relationships VALUES (1,
    MULTILINESTRING((POINT(0, 0), POINT(10, 0)), (POINT(0, 5), POINT(10, 5))),
    'Two horizontal lines'
);

INSERT INTO test_multilinestring_relationships VALUES (2,
    MULTILINESTRING((POINT(5, -5), POINT(5, 15))),
    'Vertical line crossing'
);

INSERT INTO test_multilinestring_relationships VALUES (3,
    MULTILINESTRING((POINT(20, 0), POINT(30, 0))),
    'Distant horizontal line'
);

-- Intersection
SELECT ml1.name AS mls1, ml2.name AS mls2,
       ST_Intersects(ml1.lines, ml2.lines) AS intersects
FROM test_multilinestring_relationships ml1
CROSS JOIN test_multilinestring_relationships ml2
WHERE ml1.id < ml2.id;

-- Intersection geometry
SELECT ml1.name AS mls1, ml2.name AS mls2,
       ST_AsText(ST_Intersection(ml1.lines, ml2.lines)) AS intersection_geom
FROM test_multilinestring_relationships ml1
CROSS JOIN test_multilinestring_relationships ml2
WHERE ml1.id < ml2.id AND ST_Intersects(ml1.lines, ml2.lines);

-- Distance
SELECT ml1.name AS mls1, ml2.name AS mls2,
       ST_Distance(ml1.lines, ml2.lines) AS min_distance
FROM test_multilinestring_relationships ml1
CROSS JOIN test_multilinestring_relationships ml2
WHERE ml1.id < ml2.id
ORDER BY min_distance;

-- ============================================================================
-- MULTILINESTRING Buffers
-- ============================================================================

CREATE TABLE test_multilinestring_buffer (
    id INTEGER PRIMARY KEY,
    lines MULTILINESTRING
);

INSERT INTO test_multilinestring_buffer VALUES (1,
    MULTILINESTRING((POINT(0, 0), POINT(10, 0)), (POINT(0, 10), POINT(10, 10)))
);

-- Create buffer around all lines
SELECT id,
       ST_AsText(lines) AS multilinestring,
       ST_Area(ST_Buffer(lines, 2)) AS buffer_area_2,
       ST_Area(ST_Buffer(lines, 5)) AS buffer_area_5
FROM test_multilinestring_buffer;

-- Union of individual line buffers
SELECT id,
       ST_Area(ST_Union(ST_Buffer(ST_GeometryN(lines, n), 3))) AS union_buffer_area
FROM test_multilinestring_buffer,
     generate_series(1, ST_NumGeometries(lines)) AS n
GROUP BY id;

-- ============================================================================
-- MULTILINESTRING Union and Merge
-- ============================================================================

CREATE TABLE test_multilinestring_merge (
    id INTEGER PRIMARY KEY,
    lines MULTILINESTRING
);

-- Connected lines that can be merged
INSERT INTO test_multilinestring_merge VALUES (1,
    MULTILINESTRING(
        (POINT(0, 0), POINT(5, 0)),
        (POINT(5, 0), POINT(10, 0)),
        (POINT(10, 0), POINT(15, 0))
    )
);

-- Disconnected lines
INSERT INTO test_multilinestring_merge VALUES (2,
    MULTILINESTRING(
        (POINT(0, 0), POINT(5, 0)),
        (POINT(10, 0), POINT(15, 0))
    )
);

-- Line merge (merge connected lines into single linestring)
SELECT id,
       ST_AsText(lines) AS original,
       ST_AsText(ST_LineMerge(lines)) AS merged,
       ST_GeometryType(ST_LineMerge(lines)) AS merged_type
FROM test_multilinestring_merge
ORDER BY id;

-- ============================================================================
-- MULTILINESTRING Simplification
-- ============================================================================

CREATE TABLE test_multilinestring_simplify (
    id INTEGER PRIMARY KEY,
    lines MULTILINESTRING
);

-- Complex multilinestring
INSERT INTO test_multilinestring_simplify VALUES (1,
    MULTILINESTRING(
        (POINT(0,0), POINT(1,0), POINT(2,0), POINT(3,0), POINT(4,0), POINT(5,0)),
        (POINT(0,10), POINT(1,10), POINT(2,10), POINT(3,10), POINT(4,10), POINT(5,10))
    )
);

-- Simplify
SELECT id,
       ST_NPoints(lines) AS original_points,
       ST_AsText(ST_Simplify(lines, 1.0)) AS simplified,
       ST_NPoints(ST_Simplify(lines, 1.0)) AS simplified_points,
       ST_Length(lines) AS original_length,
       ST_Length(ST_Simplify(lines, 1.0)) AS simplified_length
FROM test_multilinestring_simplify;

-- ============================================================================
-- MULTILINESTRING Bounding Box
-- ============================================================================

CREATE TABLE test_multilinestring_bbox (
    id INTEGER PRIMARY KEY,
    lines MULTILINESTRING
);

INSERT INTO test_multilinestring_bbox VALUES (1,
    MULTILINESTRING((POINT(10, 20), POINT(30, 40)), (POINT(50, 60), POINT(70, 80)))
);

-- Envelope
SELECT id,
       ST_AsText(lines) AS multilinestring,
       ST_AsText(ST_Envelope(lines)) AS bounding_box
FROM test_multilinestring_bbox;

-- Extent (aggregate)
SELECT ST_AsText(ST_Extent(lines)) AS total_extent
FROM test_multilinestring_bbox;

-- ============================================================================
-- MULTILINESTRING Aggregation
-- ============================================================================

CREATE TABLE test_multilinestring_aggregation (
    id INTEGER PRIMARY KEY,
    category VARCHAR(20),
    line_segments MULTILINESTRING
);

INSERT INTO test_multilinestring_aggregation VALUES (1, 'Route A',
    MULTILINESTRING((POINT(0, 0), POINT(5, 0)), (POINT(5, 0), POINT(10, 5)))
);
INSERT INTO test_multilinestring_aggregation VALUES (2, 'Route A',
    MULTILINESTRING((POINT(10, 5), POINT(15, 0)))
);
INSERT INTO test_multilinestring_aggregation VALUES (3, 'Route B',
    MULTILINESTRING((POINT(0, 10), POINT(10, 10)), (POINT(10, 10), POINT(10, 20)))
);

-- Collect all line segments by category
SELECT category,
       ST_AsText(ST_Collect(line_segments)) AS all_segments,
       SUM(ST_NumGeometries(line_segments)) AS total_segment_count,
       SUM(ST_Length(line_segments)) AS total_length
FROM test_multilinestring_aggregation
GROUP BY category
ORDER BY category;

-- Union and merge
SELECT category,
       ST_AsText(ST_LineMerge(ST_Union(line_segments))) AS merged_route
FROM test_multilinestring_aggregation
GROUP BY category
ORDER BY category;

-- ============================================================================
-- MULTILINESTRING NULL Handling
-- ============================================================================

CREATE TABLE test_multilinestring_nulls (
    id INTEGER PRIMARY KEY,
    nullable_lines MULTILINESTRING,
    description VARCHAR(50)
);

INSERT INTO test_multilinestring_nulls VALUES (1, NULL, 'No lines');
INSERT INTO test_multilinestring_nulls VALUES (2,
    MULTILINESTRING((POINT(0, 0), POINT(10, 10)), (POINT(0, 10), POINT(10, 0))),
    'Has lines'
);
INSERT INTO test_multilinestring_nulls VALUES (3, NULL, 'Another null');

SELECT COUNT(*) AS null_count FROM test_multilinestring_nulls WHERE nullable_lines IS NULL;

SELECT id, description,
       ST_AsText(COALESCE(nullable_lines,
           MULTILINESTRING((POINT(0, 0), POINT(1, 1))))) AS lines_with_default
FROM test_multilinestring_nulls
ORDER BY id;

-- ============================================================================
-- MULTILINESTRING Indexing
-- ============================================================================

CREATE TABLE test_multilinestring_index (
    id INTEGER PRIMARY KEY,
    road_network MULTILINESTRING,
    network_name VARCHAR(100)
);

CREATE INDEX idx_road_network_gist ON test_multilinestring_index USING GIST (road_network);

INSERT INTO test_multilinestring_index VALUES (1,
    MULTILINESTRING((POINT(0, 0), POINT(10, 0)), (POINT(5, -5), POINT(5, 5))),
    'Downtown grid'
);
INSERT INTO test_multilinestring_index VALUES (2,
    MULTILINESTRING((POINT(10, 0), POINT(20, 0)), (POINT(15, -5), POINT(15, 5))),
    'Midtown grid'
);

-- Find networks intersecting a point
SELECT network_name
FROM test_multilinestring_index
WHERE ST_Intersects(road_network, POINT(5, 0))
ORDER BY network_name;

-- Find networks within bounding box
SELECT network_name
FROM test_multilinestring_index
WHERE road_network && ST_MakeEnvelope(0, -5, 10, 5)
ORDER BY network_name;

-- ============================================================================
-- MULTILINESTRING Real-World Use Cases
-- ============================================================================

-- Transportation networks
CREATE TABLE test_transit_routes (
    id INTEGER PRIMARY KEY,
    route_name VARCHAR(100),
    route_paths MULTILINESTRING,
    route_type VARCHAR(50)
);

INSERT INTO test_transit_routes VALUES (1, 'Bus Route 101',
    MULTILINESTRING(
        (POINT(-74.0, 40.7), POINT(-73.95, 40.72), POINT(-73.9, 40.74)),
        (POINT(-73.9, 40.74), POINT(-73.85, 40.76))
    ),
    'Bus'
);

INSERT INTO test_transit_routes VALUES (2, 'Subway Line Blue',
    MULTILINESTRING(
        (POINT(-74.01, 40.71), POINT(-73.96, 40.73)),
        (POINT(-73.96, 40.73), POINT(-73.91, 40.75))
    ),
    'Subway'
);

-- Total route length
SELECT route_name, route_type,
       ST_Length(route_paths) AS total_route_length
FROM test_transit_routes
ORDER BY total_route_length DESC;

-- River/stream networks
CREATE TABLE test_river_networks (
    id INTEGER PRIMARY KEY,
    watershed_name VARCHAR(100),
    streams MULTILINESTRING
);

INSERT INTO test_river_networks VALUES (1, 'North Watershed',
    MULTILINESTRING(
        (POINT(0, 100), POINT(10, 90), POINT(20, 80), POINT(30, 70)),  -- Main river
        (POINT(5, 95), POINT(10, 90)),  -- Tributary 1
        (POINT(15, 85), POINT(20, 80))  -- Tributary 2
    )
);

-- Calculate total stream length by watershed
SELECT watershed_name,
       ST_NumGeometries(streams) AS stream_count,
       ST_Length(streams) AS total_stream_length
FROM test_river_networks;

-- Utility networks (power lines, pipelines)
CREATE TABLE test_utility_networks (
    id INTEGER PRIMARY KEY,
    utility_type VARCHAR(50),
    network_lines MULTILINESTRING
);

INSERT INTO test_utility_networks VALUES (1, 'Power Lines',
    MULTILINESTRING(
        (POINT(0, 0), POINT(50, 0)),
        (POINT(50, 0), POINT(50, 50)),
        (POINT(50, 50), POINT(0, 50))
    )
);

INSERT INTO test_utility_networks VALUES (2, 'Gas Pipelines',
    MULTILINESTRING(
        (POINT(10, 0), POINT(10, 60)),
        (POINT(40, 0), POINT(40, 60))
    )
);

-- Find utility network intersections
SELECT u1.utility_type AS type1,
       u2.utility_type AS type2,
       ST_AsText(ST_Intersection(u1.network_lines, u2.network_lines)) AS intersection_points
FROM test_utility_networks u1
CROSS JOIN test_utility_networks u2
WHERE u1.id < u2.id AND ST_Intersects(u1.network_lines, u2.network_lines);

-- Trail/hiking path systems
CREATE TABLE test_trail_systems (
    id INTEGER PRIMARY KEY,
    trail_system_name VARCHAR(100),
    trail_paths MULTILINESTRING,
    difficulty VARCHAR(20)
);

INSERT INTO test_trail_systems VALUES (1, 'Mountain Loop',
    MULTILINESTRING(
        (POINT(0, 0), POINT(10, 5), POINT(20, 10), POINT(30, 5), POINT(40, 0)),  -- Main loop
        (POINT(10, 5), POINT(10, 15)),  -- Side trail 1
        (POINT(30, 5), POINT(30, 15))   -- Side trail 2
    ),
    'Moderate'
);

-- Trail statistics
SELECT trail_system_name, difficulty,
       ST_NumGeometries(trail_paths) AS path_count,
       ST_Length(trail_paths) AS total_trail_length
FROM test_trail_systems;

-- GPS tracks collection
CREATE TABLE test_gps_tracks (
    id INTEGER PRIMARY KEY,
    user_id INTEGER,
    activity_type VARCHAR(50),
    track_segments MULTILINESTRING,
    recorded_date DATE
);

INSERT INTO test_gps_tracks VALUES (1, 101, 'Running',
    MULTILINESTRING(
        (POINT(-74.0, 40.7), POINT(-74.005, 40.705), POINT(-74.01, 40.71)),
        (POINT(-74.01, 40.71), POINT(-74.015, 40.715), POINT(-74.02, 40.72))
    ),
    DATE '2024-01-15'
);

-- Track statistics
SELECT user_id, activity_type, recorded_date,
       ST_NumGeometries(track_segments) AS segment_count,
       ST_Length(track_segments) AS total_distance
FROM test_gps_tracks;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_gps_tracks;
DROP TABLE test_trail_systems;
DROP TABLE test_utility_networks;
DROP TABLE test_river_networks;
DROP TABLE test_transit_routes;
DROP INDEX idx_road_network_gist;
DROP TABLE test_multilinestring_index;
DROP TABLE test_multilinestring_nulls;
DROP TABLE test_multilinestring_aggregation;
DROP TABLE test_multilinestring_bbox;
DROP TABLE test_multilinestring_simplify;
DROP TABLE test_multilinestring_merge;
DROP TABLE test_multilinestring_buffer;
DROP TABLE test_multilinestring_relationships;
DROP TABLE test_multilinestring_extraction;
DROP TABLE test_multilinestring_properties;
DROP TABLE test_multilinestring_formats;
DROP TABLE test_multilinestring_basic;

DROP DATABASE test_spatial_multilinestring;
