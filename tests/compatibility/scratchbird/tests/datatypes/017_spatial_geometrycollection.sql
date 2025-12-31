-- ScratchBird Native Test: Spatial Types - GEOMETRYCOLLECTION
-- Test ID: datatypes_017
-- Description: Comprehensive testing of GEOMETRYCOLLECTION geometry type

CREATE DATABASE test_spatial_geometrycollection;
\c test_spatial_geometrycollection;

-- ============================================================================
-- GEOMETRYCOLLECTION Basic Testing
-- ============================================================================

CREATE TABLE test_geometrycollection_basic (
    id INTEGER PRIMARY KEY,
    geom_coll GEOMETRYCOLLECTION,
    name VARCHAR(100)
);

-- Mixed geometry types
INSERT INTO test_geometrycollection_basic VALUES (1,
    GEOMETRYCOLLECTION(
        POINT(0, 0),
        LINESTRING(POINT(10, 10), POINT(20, 20)),
        POLYGON((POINT(30, 30), POINT(40, 30), POINT(40, 40), POINT(30, 40), POINT(30, 30)))
    ),
    'Point, Line, Polygon'
);

INSERT INTO test_geometrycollection_basic VALUES (2,
    GEOMETRYCOLLECTION(
        POINT(5, 5),
        POINT(15, 15),
        LINESTRING(POINT(0, 10), POINT(10, 10))
    ),
    'Two points and a line'
);

SELECT id, ST_AsText(geom_coll) AS geometrycollection_wkt, name FROM test_geometrycollection_basic ORDER BY id;

-- ============================================================================
-- GEOMETRYCOLLECTION Input Formats
-- ============================================================================

CREATE TABLE test_geometrycollection_formats (
    id INTEGER PRIMARY KEY,
    gc_geom GEOMETRYCOLLECTION,
    format_description VARCHAR(50)
);

-- WKT format
INSERT INTO test_geometrycollection_formats VALUES (1,
    ST_GeomFromText('GEOMETRYCOLLECTION(POINT(10 10), LINESTRING(20 20, 30 30))'),
    'WKT format'
);

INSERT INTO test_geometrycollection_formats VALUES (2,
    ST_GeomCollFromText('GEOMETRYCOLLECTION(POINT(5 5), POLYGON((0 0, 10 0, 10 10, 0 10, 0 0)))'),
    'GeomCollFromText'
);

-- ST_Collect (creates GeometryCollection from mixed types)
INSERT INTO test_geometrycollection_formats VALUES (10,
    ST_Collect(ARRAY[
        POINT(0, 0)::GEOMETRY,
        LINESTRING(POINT(5, 5), POINT(10, 10))::GEOMETRY,
        POLYGON((POINT(20, 20), POINT(30, 20), POINT(30, 30), POINT(20, 30), POINT(20, 20)))::GEOMETRY
    ]),
    'ST_Collect from mixed array'
);

SELECT id, ST_AsText(gc_geom) AS wkt, format_description FROM test_geometrycollection_formats ORDER BY id;

-- ============================================================================
-- GEOMETRYCOLLECTION Properties
-- ============================================================================

CREATE TABLE test_geometrycollection_properties (
    id INTEGER PRIMARY KEY,
    geom_coll GEOMETRYCOLLECTION
);

INSERT INTO test_geometrycollection_properties VALUES (1,
    GEOMETRYCOLLECTION(
        POINT(0, 0),
        LINESTRING(POINT(10, 10), POINT(20, 20)),
        POLYGON((POINT(30, 30), POINT(40, 30), POINT(40, 40), POINT(30, 40), POINT(30, 30)))
    )
);

-- Basic properties
SELECT id,
       ST_AsText(geom_coll) AS geometrycollection,
       ST_GeometryType(geom_coll) AS geom_type,
       ST_Dimension(geom_coll) AS max_dimension,
       ST_IsValid(geom_coll) AS is_valid,
       ST_IsSimple(geom_coll) AS is_simple,
       ST_IsEmpty(geom_coll) AS is_empty
FROM test_geometrycollection_properties
ORDER BY id;

-- Number of geometries
SELECT id,
       ST_NumGeometries(geom_coll) AS num_geometries
FROM test_geometrycollection_properties
ORDER BY id;

-- Total number of points
SELECT id,
       ST_NPoints(geom_coll) AS total_points
FROM test_geometrycollection_properties
ORDER BY id;

-- ============================================================================
-- GEOMETRYCOLLECTION Geometry Extraction
-- ============================================================================

CREATE TABLE test_geometrycollection_extraction (
    id INTEGER PRIMARY KEY,
    geom_coll GEOMETRYCOLLECTION
);

INSERT INTO test_geometrycollection_extraction VALUES (1,
    GEOMETRYCOLLECTION(
        POINT(10, 20),
        LINESTRING(POINT(30, 40), POINT(50, 60)),
        POLYGON((POINT(70, 80), POINT(90, 80), POINT(90, 100), POINT(70, 100), POINT(70, 80)))
    )
);

-- Extract individual geometries
SELECT id,
       ST_AsText(ST_GeometryN(geom_coll, 1)) AS first_geom,
       ST_GeometryType(ST_GeometryN(geom_coll, 1)) AS first_type,
       ST_AsText(ST_GeometryN(geom_coll, 2)) AS second_geom,
       ST_GeometryType(ST_GeometryN(geom_coll, 2)) AS second_type,
       ST_AsText(ST_GeometryN(geom_coll, 3)) AS third_geom,
       ST_GeometryType(ST_GeometryN(geom_coll, 3)) AS third_type
FROM test_geometrycollection_extraction;

-- Extract all geometries
SELECT id,
       n AS geom_index,
       ST_AsText(ST_GeometryN(geom_coll, n)) AS geometry,
       ST_GeometryType(ST_GeometryN(geom_coll, n)) AS geom_type,
       ST_Dimension(ST_GeometryN(geom_coll, n)) AS dimension
FROM test_geometrycollection_extraction,
     generate_series(1, ST_NumGeometries(geom_coll)) AS n
ORDER BY id, n;

-- ============================================================================
-- GEOMETRYCOLLECTION with Nested Collections
-- ============================================================================

CREATE TABLE test_geometrycollection_nested (
    id INTEGER PRIMARY KEY,
    geom_coll GEOMETRYCOLLECTION,
    description VARCHAR(100)
);

-- Nested geometry collection
INSERT INTO test_geometrycollection_nested VALUES (1,
    GEOMETRYCOLLECTION(
        POINT(0, 0),
        MULTIPOINT(POINT(10, 10), POINT(20, 20)),
        LINESTRING(POINT(30, 30), POINT(40, 40))
    ),
    'Collection with MULTIPOINT'
);

SELECT id, ST_AsText(geom_coll) AS wkt, description FROM test_geometrycollection_nested ORDER BY id;

-- Flatten to all component geometries
SELECT id,
       ST_AsText(ST_CollectionExtract(geom_coll, 1)) AS points_only,
       ST_AsText(ST_CollectionExtract(geom_coll, 2)) AS lines_only,
       ST_AsText(ST_CollectionExtract(geom_coll, 3)) AS polygons_only
FROM test_geometrycollection_nested;

-- ============================================================================
-- GEOMETRYCOLLECTION Spatial Operations
-- ============================================================================

CREATE TABLE test_geometrycollection_operations (
    id INTEGER PRIMARY KEY,
    geom_coll GEOMETRYCOLLECTION
);

INSERT INTO test_geometrycollection_operations VALUES (1,
    GEOMETRYCOLLECTION(
        POINT(5, 5),
        LINESTRING(POINT(0, 0), POINT(10, 10)),
        POLYGON((POINT(20, 20), POINT(30, 20), POINT(30, 30), POINT(20, 30), POINT(20, 20)))
    )
);

-- Bounding box
SELECT id,
       ST_AsText(geom_coll) AS geometrycollection,
       ST_AsText(ST_Envelope(geom_coll)) AS bounding_box
FROM test_geometrycollection_operations;

-- Convex hull
SELECT id,
       ST_AsText(ST_ConvexHull(geom_coll)) AS convex_hull,
       ST_GeometryType(ST_ConvexHull(geom_coll)) AS hull_type
FROM test_geometrycollection_operations;

-- Centroid
SELECT id,
       ST_AsText(ST_Centroid(geom_coll)) AS centroid
FROM test_geometrycollection_operations;

-- ============================================================================
-- GEOMETRYCOLLECTION with POINT Relationship
-- ============================================================================

CREATE TABLE test_geometrycollection_point (
    id INTEGER PRIMARY KEY,
    geom_coll GEOMETRYCOLLECTION
);

INSERT INTO test_geometrycollection_point VALUES (1,
    GEOMETRYCOLLECTION(
        LINESTRING(POINT(0, 0), POINT(10, 10)),
        POLYGON((POINT(20, 20), POINT(40, 20), POINT(40, 40), POINT(20, 40), POINT(20, 20)))
    )
);

-- Point containment
SELECT id,
       ST_Contains(geom_coll, POINT(5, 5)) AS contains_on_line,
       ST_Contains(geom_coll, POINT(30, 30)) AS contains_in_polygon,
       ST_Contains(geom_coll, POINT(50, 50)) AS contains_outside
FROM test_geometrycollection_point;

-- Intersection with point
SELECT id,
       ST_Intersects(geom_coll, POINT(5, 5)) AS intersects_line,
       ST_Intersects(geom_coll, POINT(30, 30)) AS intersects_polygon
FROM test_geometrycollection_point;

-- Distance from point
SELECT id,
       ST_Distance(geom_coll, POINT(0, 0)) AS dist_to_origin,
       ST_Distance(geom_coll, POINT(50, 50)) AS dist_to_far_point
FROM test_geometrycollection_point;

-- ============================================================================
-- GEOMETRYCOLLECTION Buffers
-- ============================================================================

CREATE TABLE test_geometrycollection_buffer (
    id INTEGER PRIMARY KEY,
    geom_coll GEOMETRYCOLLECTION
);

INSERT INTO test_geometrycollection_buffer VALUES (1,
    GEOMETRYCOLLECTION(
        POINT(0, 0),
        LINESTRING(POINT(10, 10), POINT(20, 10))
    )
);

-- Create buffer
SELECT id,
       ST_Area(ST_Buffer(geom_coll, 5)) AS buffer_area_5,
       ST_Area(ST_Buffer(geom_coll, 10)) AS buffer_area_10
FROM test_geometrycollection_buffer;

-- Union of individual buffers
SELECT id,
       ST_Area(ST_Union(ST_Buffer(ST_GeometryN(geom_coll, n), 3))) AS union_buffer_area
FROM test_geometrycollection_buffer,
     generate_series(1, ST_NumGeometries(geom_coll)) AS n
GROUP BY id;

-- ============================================================================
-- GEOMETRYCOLLECTION Aggregation
-- ============================================================================

CREATE TABLE test_geometrycollection_aggregation (
    id INTEGER PRIMARY KEY,
    category VARCHAR(20),
    geometries GEOMETRYCOLLECTION
);

INSERT INTO test_geometrycollection_aggregation VALUES (1, 'Group A',
    GEOMETRYCOLLECTION(POINT(0, 0), LINESTRING(POINT(5, 5), POINT(10, 10)))
);
INSERT INTO test_geometrycollection_aggregation VALUES (2, 'Group A',
    GEOMETRYCOLLECTION(POINT(15, 15), POLYGON((POINT(20, 20), POINT(30, 20), POINT(30, 30), POINT(20, 30), POINT(20, 20))))
);
INSERT INTO test_geometrycollection_aggregation VALUES (3, 'Group B',
    GEOMETRYCOLLECTION(POINT(50, 50))
);

-- Collect all geometries by category
SELECT category,
       ST_AsText(ST_Collect(geometries)) AS all_geometries,
       SUM(ST_NumGeometries(geometries)) AS total_geom_count
FROM test_geometrycollection_aggregation
GROUP BY category
ORDER BY category;

-- Union geometries
SELECT category,
       ST_AsText(ST_Union(geometries)) AS union_geom
FROM test_geometrycollection_aggregation
GROUP BY category
ORDER BY category;

-- ============================================================================
-- GEOMETRYCOLLECTION Type Filtering
-- ============================================================================

CREATE TABLE test_geometrycollection_filtering (
    id INTEGER PRIMARY KEY,
    mixed_geoms GEOMETRYCOLLECTION
);

INSERT INTO test_geometrycollection_filtering VALUES (1,
    GEOMETRYCOLLECTION(
        POINT(0, 0),
        POINT(5, 5),
        LINESTRING(POINT(10, 10), POINT(20, 20)),
        LINESTRING(POINT(25, 25), POINT(35, 35)),
        POLYGON((POINT(40, 40), POINT(50, 40), POINT(50, 50), POINT(40, 50), POINT(40, 40)))
    )
);

-- Extract only specific geometry types
SELECT id,
       ST_AsText(ST_CollectionExtract(mixed_geoms, 1)) AS points_only,
       ST_AsText(ST_CollectionExtract(mixed_geoms, 2)) AS linestrings_only,
       ST_AsText(ST_CollectionExtract(mixed_geoms, 3)) AS polygons_only
FROM test_geometrycollection_filtering;

-- Count by geometry type
SELECT id,
       SUM(CASE WHEN ST_GeometryType(ST_GeometryN(mixed_geoms, n)) = 'ST_Point' THEN 1 ELSE 0 END) AS point_count,
       SUM(CASE WHEN ST_GeometryType(ST_GeometryN(mixed_geoms, n)) = 'ST_LineString' THEN 1 ELSE 0 END) AS linestring_count,
       SUM(CASE WHEN ST_GeometryType(ST_GeometryN(mixed_geoms, n)) = 'ST_Polygon' THEN 1 ELSE 0 END) AS polygon_count
FROM test_geometrycollection_filtering,
     generate_series(1, ST_NumGeometries(mixed_geoms)) AS n
GROUP BY id;

-- ============================================================================
-- GEOMETRYCOLLECTION NULL Handling
-- ============================================================================

CREATE TABLE test_geometrycollection_nulls (
    id INTEGER PRIMARY KEY,
    nullable_geom_coll GEOMETRYCOLLECTION,
    description VARCHAR(50)
);

INSERT INTO test_geometrycollection_nulls VALUES (1, NULL, 'No geometries');
INSERT INTO test_geometrycollection_nulls VALUES (2,
    GEOMETRYCOLLECTION(POINT(10, 20), LINESTRING(POINT(30, 40), POINT(50, 60))),
    'Has geometries'
);
INSERT INTO test_geometrycollection_nulls VALUES (3, NULL, 'Another null');

SELECT COUNT(*) AS null_count FROM test_geometrycollection_nulls WHERE nullable_geom_coll IS NULL;

SELECT id, description,
       ST_AsText(COALESCE(nullable_geom_coll, GEOMETRYCOLLECTION(POINT(0, 0)))) AS geom_with_default
FROM test_geometrycollection_nulls
ORDER BY id;

-- ============================================================================
-- GEOMETRYCOLLECTION Indexing
-- ============================================================================

CREATE TABLE test_geometrycollection_index (
    id INTEGER PRIMARY KEY,
    mixed_features GEOMETRYCOLLECTION,
    feature_set_name VARCHAR(100)
);

CREATE INDEX idx_mixed_features_gist ON test_geometrycollection_index USING GIST (mixed_features);

INSERT INTO test_geometrycollection_index VALUES (1,
    GEOMETRYCOLLECTION(
        POINT(-74.0, 40.7),
        LINESTRING(POINT(-74.0, 40.7), POINT(-73.95, 40.72)),
        POLYGON((POINT(-74.05, 40.68), POINT(-73.95, 40.68), POINT(-73.95, 40.72), POINT(-74.05, 40.72), POINT(-74.05, 40.68)))
    ),
    'NYC Mixed Features'
);

-- Spatial queries
SELECT feature_set_name
FROM test_geometrycollection_index
WHERE ST_Intersects(mixed_features, POINT(-74.0, 40.7))
ORDER BY feature_set_name;

SELECT feature_set_name
FROM test_geometrycollection_index
WHERE mixed_features && ST_MakeEnvelope(-74.1, 40.65, -73.9, 40.75)
ORDER BY feature_set_name;

-- ============================================================================
-- GEOMETRYCOLLECTION Real-World Use Cases
-- ============================================================================

-- Mixed map features
CREATE TABLE test_map_features (
    id INTEGER PRIMARY KEY,
    layer_name VARCHAR(100),
    map_objects GEOMETRYCOLLECTION
);

INSERT INTO test_map_features VALUES (1, 'City Infrastructure',
    GEOMETRYCOLLECTION(
        POINT(-74.0, 40.7),  -- City hall location
        LINESTRING(POINT(-74.0, 40.7), POINT(-73.95, 40.72)),  -- Main street
        POLYGON((POINT(-74.05, 40.68), POINT(-73.95, 40.68), POINT(-73.95, 40.72), POINT(-74.05, 40.72), POINT(-74.05, 40.68)))  -- Park
    )
);

-- Feature counts
SELECT layer_name,
       ST_NumGeometries(map_objects) AS total_features
FROM test_map_features;

-- Survey data (mixed measurement types)
CREATE TABLE test_survey_data (
    id INTEGER PRIMARY KEY,
    survey_name VARCHAR(100),
    measurements GEOMETRYCOLLECTION,
    survey_date DATE
);

INSERT INTO test_survey_data VALUES (1, 'Site Survey 2024-01',
    GEOMETRYCOLLECTION(
        POINT(100, 200),  -- Sample point
        LINESTRING(POINT(0, 0), POINT(100, 0), POINT(100, 100), POINT(0, 100)),  -- Property boundary
        POLYGON((POINT(20, 20), POINT(80, 20), POINT(80, 80), POINT(20, 80), POINT(20, 20)))  -- Building footprint
    ),
    DATE '2024-01-15'
);

-- Survey statistics
SELECT survey_name, survey_date,
       ST_NumGeometries(measurements) AS measurement_count
FROM test_survey_data;

-- GIS layer composition
CREATE TABLE test_gis_layers (
    id INTEGER PRIMARY KEY,
    layer_type VARCHAR(50),
    layer_geometries GEOMETRYCOLLECTION
);

INSERT INTO test_gis_layers VALUES (1, 'Transportation',
    GEOMETRYCOLLECTION(
        POINT(10, 10),  -- Bus stop
        POINT(20, 20),  -- Train station
        LINESTRING(POINT(0, 0), POINT(50, 50)),  -- Highway
        LINESTRING(POINT(50, 0), POINT(0, 50))  -- Rail line
    )
);

INSERT INTO test_gis_layers VALUES (2, 'Land Use',
    GEOMETRYCOLLECTION(
        POLYGON((POINT(0, 0), POINT(30, 0), POINT(30, 30), POINT(0, 30), POINT(0, 0))),  -- Residential
        POLYGON((POINT(40, 40), POINT(70, 40), POINT(70, 70), POINT(40, 70), POINT(40, 40)))  -- Commercial
    )
);

-- Layer composition analysis
SELECT layer_type,
       ST_NumGeometries(layer_geometries) AS feature_count
FROM test_gis_layers
ORDER BY feature_count DESC;

-- CAD/Engineering drawings
CREATE TABLE test_cad_drawings (
    id INTEGER PRIMARY KEY,
    drawing_name VARCHAR(100),
    drawing_elements GEOMETRYCOLLECTION
);

INSERT INTO test_cad_drawings VALUES (1, 'Floor Plan',
    GEOMETRYCOLLECTION(
        POINT(5, 5),  -- Fixture point
        LINESTRING(POINT(0, 0), POINT(100, 0)),  -- Wall
        LINESTRING(POINT(0, 0), POINT(0, 80)),  -- Wall
        POLYGON((POINT(10, 10), POINT(40, 10), POINT(40, 30), POINT(10, 30), POINT(10, 10))),  -- Room
        POLYGON((POINT(50, 10), POINT(90, 10), POINT(90, 30), POINT(50, 30), POINT(50, 10)))  -- Room
    )
);

-- Drawing element count
SELECT drawing_name,
       ST_NumGeometries(drawing_elements) AS element_count
FROM test_cad_drawings;

-- Search/rescue area definition
CREATE TABLE test_search_areas (
    id INTEGER PRIMARY KEY,
    search_zone_name VARCHAR(100),
    zone_features GEOMETRYCOLLECTION
);

INSERT INTO test_search_areas VALUES (1, 'Zone Alpha',
    GEOMETRYCOLLECTION(
        POINT(0, 0),  -- Last known position
        LINESTRING(POINT(0, 0), POINT(10, 10)),  -- Search path
        POLYGON((POINT(-5, -5), POINT(15, -5), POINT(15, 15), POINT(-5, 15), POINT(-5, -5)))  -- Search area boundary
    )
);

-- Zone analysis
SELECT search_zone_name,
       ST_Area(ST_CollectionExtract(zone_features, 3)) AS search_area_size
FROM test_search_areas;

-- ============================================================================
-- GEOMETRYCOLLECTION Homogenize (Convert to Multi* type)
-- ============================================================================

CREATE TABLE test_geometrycollection_homogenize (
    id INTEGER PRIMARY KEY,
    geom_coll GEOMETRYCOLLECTION
);

INSERT INTO test_geometrycollection_homogenize VALUES (1,
    GEOMETRYCOLLECTION(POINT(0, 0), POINT(10, 10), POINT(20, 20))
);

INSERT INTO test_geometrycollection_homogenize VALUES (2,
    GEOMETRYCOLLECTION(
        LINESTRING(POINT(0, 0), POINT(10, 0)),
        LINESTRING(POINT(0, 10), POINT(10, 10))
    )
);

-- Homogenize to appropriate Multi* type
SELECT id,
       ST_AsText(geom_coll) AS original,
       ST_AsText(ST_CollectionHomogenize(geom_coll)) AS homogenized,
       ST_GeometryType(ST_CollectionHomogenize(geom_coll)) AS homogenized_type
FROM test_geometrycollection_homogenize
ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_geometrycollection_homogenize;
DROP TABLE test_search_areas;
DROP TABLE test_cad_drawings;
DROP TABLE test_gis_layers;
DROP TABLE test_survey_data;
DROP TABLE test_map_features;
DROP INDEX idx_mixed_features_gist;
DROP TABLE test_geometrycollection_index;
DROP TABLE test_geometrycollection_nulls;
DROP TABLE test_geometrycollection_filtering;
DROP TABLE test_geometrycollection_aggregation;
DROP TABLE test_geometrycollection_buffer;
DROP TABLE test_geometrycollection_point;
DROP TABLE test_geometrycollection_operations;
DROP TABLE test_geometrycollection_nested;
DROP TABLE test_geometrycollection_extraction;
DROP TABLE test_geometrycollection_properties;
DROP TABLE test_geometrycollection_formats;
DROP TABLE test_geometrycollection_basic;

DROP DATABASE test_spatial_geometrycollection;
