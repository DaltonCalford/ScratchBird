-- ScratchBird Native Test: Spatial Types - MULTIPOLYGON
-- Test ID: datatypes_016
-- Description: Comprehensive testing of MULTIPOLYGON geometry type

CREATE DATABASE test_spatial_multipolygon;
\c test_spatial_multipolygon;

-- ============================================================================
-- MULTIPOLYGON Basic Testing
-- ============================================================================

CREATE TABLE test_multipolygon_basic (
    id INTEGER PRIMARY KEY,
    polygons MULTIPOLYGON,
    name VARCHAR(100)
);

-- Simple multipolygons
INSERT INTO test_multipolygon_basic VALUES (1,
    MULTIPOLYGON(
        ((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
        ((POINT(20, 0), POINT(30, 0), POINT(30, 10), POINT(20, 10), POINT(20, 0)))
    ),
    'Two squares'
);

INSERT INTO test_multipolygon_basic VALUES (2,
    MULTIPOLYGON(
        ((POINT(0, 0), POINT(10, 0), POINT(5, 10), POINT(0, 0))),
        ((POINT(15, 0), POINT(25, 0), POINT(20, 10), POINT(15, 0))),
        ((POINT(30, 0), POINT(40, 0), POINT(35, 10), POINT(30, 0)))
    ),
    'Three triangles'
);

SELECT id, ST_AsText(polygons) AS multipolygon_wkt, name FROM test_multipolygon_basic ORDER BY id;

-- ============================================================================
-- MULTIPOLYGON with Holes
-- ============================================================================

CREATE TABLE test_multipolygon_holes (
    id INTEGER PRIMARY KEY,
    polygons MULTIPOLYGON,
    description VARCHAR(100)
);

-- Multipolygon with holes in polygons
INSERT INTO test_multipolygon_holes VALUES (1,
    ST_GeomFromText('MULTIPOLYGON(
        ((0 0, 100 0, 100 100, 0 100, 0 0), (20 20, 80 20, 80 80, 20 80, 20 20)),
        ((150 0, 250 0, 250 100, 150 100, 150 0))
    )'),
    'Two polygons, first with hole'
);

SELECT id, ST_AsText(polygons) AS multipolygon_wkt, description FROM test_multipolygon_holes ORDER BY id;

-- ============================================================================
-- MULTIPOLYGON Input Formats
-- ============================================================================

CREATE TABLE test_multipolygon_formats (
    id INTEGER PRIMARY KEY,
    mp_geom MULTIPOLYGON,
    format_description VARCHAR(50)
);

-- WKT format
INSERT INTO test_multipolygon_formats VALUES (1,
    ST_GeomFromText('MULTIPOLYGON(((0 0, 10 0, 10 10, 0 10, 0 0)), ((20 20, 30 20, 30 30, 20 30, 20 20)))'),
    'WKT format'
);

INSERT INTO test_multipolygon_formats VALUES (2,
    ST_MPolyFromText('MULTIPOLYGON(((5 5, 15 5, 15 15, 5 15, 5 5)))'),
    'MPolyFromText'
);

-- ST_Collect from polygons
INSERT INTO test_multipolygon_formats VALUES (10,
    ST_Collect(ARRAY[
        POLYGON((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
        POLYGON((POINT(20, 20), POINT(30, 20), POINT(30, 30), POINT(20, 30), POINT(20, 20)))
    ]),
    'ST_Collect from array'
);

SELECT id, ST_AsText(mp_geom) AS wkt, format_description FROM test_multipolygon_formats ORDER BY id;

-- ============================================================================
-- MULTIPOLYGON Properties
-- ============================================================================

CREATE TABLE test_multipolygon_properties (
    id INTEGER PRIMARY KEY,
    polygons MULTIPOLYGON
);

INSERT INTO test_multipolygon_properties VALUES (1,
    MULTIPOLYGON(
        ((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
        ((POINT(20, 20), POINT(30, 20), POINT(30, 30), POINT(20, 30), POINT(20, 20)))
    )
);

-- Basic properties
SELECT id,
       ST_AsText(polygons) AS multipolygon,
       ST_GeometryType(polygons) AS geom_type,
       ST_Dimension(polygons) AS dimension,
       ST_IsValid(polygons) AS is_valid,
       ST_IsSimple(polygons) AS is_simple
FROM test_multipolygon_properties
ORDER BY id;

-- Number of polygons
SELECT id,
       ST_NumGeometries(polygons) AS num_polygons
FROM test_multipolygon_properties
ORDER BY id;

-- Total area
SELECT id,
       ST_Area(polygons) AS total_area
FROM test_multipolygon_properties
ORDER BY id;

-- Total perimeter
SELECT id,
       ST_Perimeter(polygons) AS total_perimeter
FROM test_multipolygon_properties
ORDER BY id;

-- ============================================================================
-- MULTIPOLYGON Polygon Extraction
-- ============================================================================

CREATE TABLE test_multipolygon_extraction (
    id INTEGER PRIMARY KEY,
    polygons MULTIPOLYGON
);

INSERT INTO test_multipolygon_extraction VALUES (1,
    MULTIPOLYGON(
        ((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
        ((POINT(20, 0), POINT(30, 0), POINT(30, 10), POINT(20, 10), POINT(20, 0))),
        ((POINT(40, 0), POINT(50, 0), POINT(50, 10), POINT(40, 10), POINT(40, 0)))
    )
);

-- Extract individual polygons
SELECT id,
       ST_AsText(polygons) AS multipolygon,
       ST_AsText(ST_GeometryN(polygons, 1)) AS first_polygon,
       ST_AsText(ST_GeometryN(polygons, 2)) AS second_polygon,
       ST_AsText(ST_GeometryN(polygons, 3)) AS third_polygon
FROM test_multipolygon_extraction;

-- Extract all polygons with properties
SELECT id,
       n AS polygon_index,
       ST_AsText(ST_GeometryN(polygons, n)) AS polygon,
       ST_Area(ST_GeometryN(polygons, n)) AS polygon_area,
       ST_Perimeter(ST_GeometryN(polygons, n)) AS polygon_perimeter
FROM test_multipolygon_extraction,
     generate_series(1, ST_NumGeometries(polygons)) AS n
ORDER BY id, n;

-- ============================================================================
-- MULTIPOLYGON Spatial Relationships
-- ============================================================================

CREATE TABLE test_multipolygon_relationships (
    id INTEGER PRIMARY KEY,
    polygons MULTIPOLYGON,
    name VARCHAR(50)
);

INSERT INTO test_multipolygon_relationships VALUES (1,
    MULTIPOLYGON(
        ((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
        ((POINT(20, 20), POINT(30, 20), POINT(30, 30), POINT(20, 30), POINT(20, 20)))
    ),
    'Separated squares'
);

INSERT INTO test_multipolygon_relationships VALUES (2,
    MULTIPOLYGON(
        ((POINT(5, 5), POINT(15, 5), POINT(15, 15), POINT(5, 15), POINT(5, 5)))
    ),
    'Overlapping square'
);

INSERT INTO test_multipolygon_relationships VALUES (3,
    MULTIPOLYGON(
        ((POINT(50, 50), POINT(60, 50), POINT(60, 60), POINT(50, 60), POINT(50, 50)))
    ),
    'Distant square'
);

-- Intersection
SELECT mp1.name AS mp1_name, mp2.name AS mp2_name,
       ST_Intersects(mp1.polygons, mp2.polygons) AS intersects
FROM test_multipolygon_relationships mp1
CROSS JOIN test_multipolygon_relationships mp2
WHERE mp1.id < mp2.id;

-- Overlaps
SELECT mp1.name AS mp1_name, mp2.name AS mp2_name,
       ST_Overlaps(mp1.polygons, mp2.polygons) AS overlaps
FROM test_multipolygon_relationships mp1
CROSS JOIN test_multipolygon_relationships mp2
WHERE mp1.id < mp2.id;

-- Intersection area
SELECT mp1.name AS mp1_name, mp2.name AS mp2_name,
       ST_Area(ST_Intersection(mp1.polygons, mp2.polygons)) AS intersection_area
FROM test_multipolygon_relationships mp1
CROSS JOIN test_multipolygon_relationships mp2
WHERE mp1.id < mp2.id AND ST_Intersects(mp1.polygons, mp2.polygons);

-- Distance
SELECT mp1.name AS mp1_name, mp2.name AS mp2_name,
       ST_Distance(mp1.polygons, mp2.polygons) AS min_distance
FROM test_multipolygon_relationships mp1
CROSS JOIN test_multipolygon_relationships mp2
WHERE mp1.id < mp2.id
ORDER BY min_distance;

-- ============================================================================
-- MULTIPOLYGON Boolean Operations
-- ============================================================================

CREATE TABLE test_multipolygon_boolean (
    id INTEGER PRIMARY KEY,
    mp1 MULTIPOLYGON,
    mp2 MULTIPOLYGON
);

INSERT INTO test_multipolygon_boolean VALUES (1,
    MULTIPOLYGON(
        ((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
        ((POINT(15, 0), POINT(25, 0), POINT(25, 10), POINT(15, 10), POINT(15, 0)))
    ),
    MULTIPOLYGON(
        ((POINT(5, 5), POINT(15, 5), POINT(15, 15), POINT(5, 15), POINT(5, 5)))
    )
);

-- Union
SELECT id,
       ST_AsText(ST_Union(mp1, mp2)) AS union_result,
       ST_Area(ST_Union(mp1, mp2)) AS union_area
FROM test_multipolygon_boolean;

-- Intersection
SELECT id,
       ST_AsText(ST_Intersection(mp1, mp2)) AS intersection_result,
       ST_Area(ST_Intersection(mp1, mp2)) AS intersection_area
FROM test_multipolygon_boolean;

-- Difference
SELECT id,
       ST_AsText(ST_Difference(mp1, mp2)) AS difference_result,
       ST_Area(ST_Difference(mp1, mp2)) AS difference_area
FROM test_multipolygon_boolean;

-- Symmetric difference
SELECT id,
       ST_AsText(ST_SymDifference(mp1, mp2)) AS symdiff_result,
       ST_Area(ST_SymDifference(mp1, mp2)) AS symdiff_area
FROM test_multipolygon_boolean;

-- ============================================================================
-- MULTIPOLYGON Buffers
-- ============================================================================

CREATE TABLE test_multipolygon_buffer (
    id INTEGER PRIMARY KEY,
    polygons MULTIPOLYGON
);

INSERT INTO test_multipolygon_buffer VALUES (1,
    MULTIPOLYGON(
        ((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
        ((POINT(50, 50), POINT(60, 50), POINT(60, 60), POINT(50, 60), POINT(50, 50)))
    )
);

-- Positive buffer (expand)
SELECT id,
       ST_Area(polygons) AS original_area,
       ST_Area(ST_Buffer(polygons, 5)) AS buffered_area_5,
       ST_NumGeometries(ST_Buffer(polygons, 5)) AS buffered_geom_count
FROM test_multipolygon_buffer;

-- Negative buffer (shrink)
SELECT id,
       ST_Area(polygons) AS original_area,
       ST_Area(ST_Buffer(polygons, -2)) AS eroded_area_2
FROM test_multipolygon_buffer;

-- ============================================================================
-- MULTIPOLYGON Centroid and Representative Point
-- ============================================================================

CREATE TABLE test_multipolygon_centroid (
    id INTEGER PRIMARY KEY,
    polygons MULTIPOLYGON
);

INSERT INTO test_multipolygon_centroid VALUES (1,
    MULTIPOLYGON(
        ((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
        ((POINT(50, 50), POINT(60, 50), POINT(60, 60), POINT(50, 60), POINT(50, 50)))
    )
);

-- Centroid of all polygons
SELECT id,
       ST_AsText(polygons) AS multipolygon,
       ST_AsText(ST_Centroid(polygons)) AS centroid
FROM test_multipolygon_centroid;

-- Point on surface
SELECT id,
       ST_AsText(ST_PointOnSurface(polygons)) AS point_on_surface
FROM test_multipolygon_centroid;

-- ============================================================================
-- MULTIPOLYGON Convex Hull
-- ============================================================================

CREATE TABLE test_multipolygon_hull (
    id INTEGER PRIMARY KEY,
    polygons MULTIPOLYGON
);

INSERT INTO test_multipolygon_hull VALUES (1,
    MULTIPOLYGON(
        ((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0))),
        ((POINT(30, 0), POINT(40, 0), POINT(40, 10), POINT(30, 10), POINT(30, 0)))
    )
);

-- Convex hull
SELECT id,
       ST_AsText(polygons) AS multipolygon,
       ST_AsText(ST_ConvexHull(polygons)) AS convex_hull,
       ST_Area(polygons) AS original_area,
       ST_Area(ST_ConvexHull(polygons)) AS hull_area
FROM test_multipolygon_hull;

-- ============================================================================
-- MULTIPOLYGON Aggregation
-- ============================================================================

CREATE TABLE test_multipolygon_aggregation (
    id INTEGER PRIMARY KEY,
    category VARCHAR(20),
    zones MULTIPOLYGON
);

INSERT INTO test_multipolygon_aggregation VALUES (1, 'Zone A',
    MULTIPOLYGON(
        ((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0)))
    )
);
INSERT INTO test_multipolygon_aggregation VALUES (2, 'Zone A',
    MULTIPOLYGON(
        ((POINT(10, 0), POINT(20, 0), POINT(20, 10), POINT(10, 10), POINT(10, 0)))
    )
);
INSERT INTO test_multipolygon_aggregation VALUES (3, 'Zone B',
    MULTIPOLYGON(
        ((POINT(0, 20), POINT(15, 20), POINT(15, 30), POINT(0, 30), POINT(0, 20)))
    )
);

-- Union all zones by category
SELECT category,
       ST_AsText(ST_Union(zones)) AS merged_zones,
       SUM(ST_Area(zones)) AS total_area
FROM test_multipolygon_aggregation
GROUP BY category
ORDER BY category;

-- Collect zones
SELECT category,
       ST_AsText(ST_Collect(zones)) AS collected_zones,
       SUM(ST_NumGeometries(zones)) AS total_polygon_count
FROM test_multipolygon_aggregation
GROUP BY category
ORDER BY category;

-- ============================================================================
-- MULTIPOLYGON Bounding Box
-- ============================================================================

CREATE TABLE test_multipolygon_bbox (
    id INTEGER PRIMARY KEY,
    polygons MULTIPOLYGON
);

INSERT INTO test_multipolygon_bbox VALUES (1,
    MULTIPOLYGON(
        ((POINT(10, 20), POINT(30, 20), POINT(30, 40), POINT(10, 40), POINT(10, 20))),
        ((POINT(50, 60), POINT(70, 60), POINT(70, 80), POINT(50, 80), POINT(50, 60)))
    )
);

-- Envelope
SELECT id,
       ST_AsText(polygons) AS multipolygon,
       ST_AsText(ST_Envelope(polygons)) AS bounding_box
FROM test_multipolygon_bbox;

-- Extent
SELECT ST_AsText(ST_Extent(polygons)) AS total_extent
FROM test_multipolygon_bbox;

-- ============================================================================
-- MULTIPOLYGON NULL Handling
-- ============================================================================

CREATE TABLE test_multipolygon_nulls (
    id INTEGER PRIMARY KEY,
    nullable_polygons MULTIPOLYGON,
    description VARCHAR(50)
);

INSERT INTO test_multipolygon_nulls VALUES (1, NULL, 'No polygons');
INSERT INTO test_multipolygon_nulls VALUES (2,
    MULTIPOLYGON(
        ((POINT(0, 0), POINT(10, 0), POINT(10, 10), POINT(0, 10), POINT(0, 0)))
    ),
    'Has polygons'
);
INSERT INTO test_multipolygon_nulls VALUES (3, NULL, 'Another null');

SELECT COUNT(*) AS null_count FROM test_multipolygon_nulls WHERE nullable_polygons IS NULL;

SELECT id, description,
       ST_AsText(COALESCE(nullable_polygons,
           MULTIPOLYGON(((POINT(0, 0), POINT(1, 0), POINT(1, 1), POINT(0, 1), POINT(0, 0)))))) AS polygons_with_default
FROM test_multipolygon_nulls
ORDER BY id;

-- ============================================================================
-- MULTIPOLYGON Indexing
-- ============================================================================

CREATE TABLE test_multipolygon_index (
    id INTEGER PRIMARY KEY,
    boundaries MULTIPOLYGON,
    region_name VARCHAR(100)
);

CREATE INDEX idx_boundaries_gist ON test_multipolygon_index USING GIST (boundaries);

INSERT INTO test_multipolygon_index VALUES (1,
    MULTIPOLYGON(
        ((POINT(-74.1, 40.6), POINT(-73.9, 40.6), POINT(-73.9, 40.7), POINT(-74.1, 40.7), POINT(-74.1, 40.6))),
        ((POINT(-74.2, 40.5), POINT(-74.0, 40.5), POINT(-74.0, 40.6), POINT(-74.2, 40.6), POINT(-74.2, 40.5)))
    ),
    'NYC Zones'
);

INSERT INTO test_multipolygon_index VALUES (2,
    MULTIPOLYGON(
        ((POINT(-118.3, 34.0), POINT(-118.2, 34.0), POINT(-118.2, 34.1), POINT(-118.3, 34.1), POINT(-118.3, 34.0)))
    ),
    'LA Zones'
);

-- Find regions containing a point
SELECT region_name
FROM test_multipolygon_index
WHERE ST_Contains(boundaries, POINT(-74.0, 40.65))
ORDER BY region_name;

-- Find regions intersecting a bounding box
SELECT region_name
FROM test_multipolygon_index
WHERE boundaries && ST_MakeEnvelope(-74.15, 40.55, -74.05, 40.65)
ORDER BY region_name;

-- ============================================================================
-- MULTIPOLYGON Real-World Use Cases
-- ============================================================================

-- Archipelago/Islands
CREATE TABLE test_islands (
    id INTEGER PRIMARY KEY,
    archipelago_name VARCHAR(100),
    islands MULTIPOLYGON
);

INSERT INTO test_islands VALUES (1, 'Pacific Islands',
    MULTIPOLYGON(
        ((POINT(150, 10), POINT(155, 10), POINT(155, 15), POINT(150, 15), POINT(150, 10))),
        ((POINT(160, 5), POINT(165, 5), POINT(165, 10), POINT(160, 10), POINT(160, 5))),
        ((POINT(170, 12), POINT(175, 12), POINT(175, 17), POINT(170, 17), POINT(170, 12)))
    )
);

-- Total land area
SELECT archipelago_name,
       ST_NumGeometries(islands) AS island_count,
       ST_Area(islands) AS total_land_area
FROM test_islands;

-- Protected areas/parks
CREATE TABLE test_protected_areas (
    id INTEGER PRIMARY KEY,
    park_system_name VARCHAR(100),
    park_boundaries MULTIPOLYGON
);

INSERT INTO test_protected_areas VALUES (1, 'National Park System',
    MULTIPOLYGON(
        ((POINT(0, 0), POINT(50, 0), POINT(50, 50), POINT(0, 50), POINT(0, 0))),
        ((POINT(100, 100), POINT(150, 100), POINT(150, 150), POINT(100, 150), POINT(100, 100)))
    )
);

-- Total protected area
SELECT park_system_name,
       ST_NumGeometries(park_boundaries) AS park_count,
       ST_Area(park_boundaries) AS total_protected_area
FROM test_protected_areas;

-- Zoning districts
CREATE TABLE test_zoning (
    id INTEGER PRIMARY KEY,
    zoning_type VARCHAR(50),
    zones MULTIPOLYGON
);

INSERT INTO test_zoning VALUES (1, 'Residential',
    MULTIPOLYGON(
        ((POINT(0, 0), POINT(30, 0), POINT(30, 30), POINT(0, 30), POINT(0, 0))),
        ((POINT(60, 60), POINT(90, 60), POINT(90, 90), POINT(60, 90), POINT(60, 90)))
    )
);

INSERT INTO test_zoning VALUES (2, 'Commercial',
    MULTIPOLYGON(
        ((POINT(30, 0), POINT(60, 0), POINT(60, 30), POINT(30, 30), POINT(30, 0)))
    )
);

-- Calculate area by zoning type
SELECT zoning_type,
       ST_NumGeometries(zones) AS zone_count,
       ST_Area(zones) AS total_area
FROM test_zoning
ORDER BY total_area DESC;

-- Building footprints
CREATE TABLE test_building_footprints (
    id INTEGER PRIMARY KEY,
    building_complex_name VARCHAR(100),
    building_polygons MULTIPOLYGON
);

INSERT INTO test_building_footprints VALUES (1, 'Shopping Center',
    MULTIPOLYGON(
        ((POINT(0, 0), POINT(20, 0), POINT(20, 30), POINT(0, 30), POINT(0, 0))),  -- Main building
        ((POINT(25, 0), POINT(40, 0), POINT(40, 15), POINT(25, 15), POINT(25, 0))),  -- Annex 1
        ((POINT(25, 20), POINT(40, 20), POINT(40, 30), POINT(25, 30), POINT(25, 20)))  -- Annex 2
    )
);

-- Total building footprint
SELECT building_complex_name,
       ST_NumGeometries(building_polygons) AS building_count,
       ST_Area(building_polygons) AS total_footprint
FROM test_building_footprints;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_building_footprints;
DROP TABLE test_zoning;
DROP TABLE test_protected_areas;
DROP TABLE test_islands;
DROP INDEX idx_boundaries_gist;
DROP TABLE test_multipolygon_index;
DROP TABLE test_multipolygon_nulls;
DROP TABLE test_multipolygon_bbox;
DROP TABLE test_multipolygon_aggregation;
DROP TABLE test_multipolygon_hull;
DROP TABLE test_multipolygon_centroid;
DROP TABLE test_multipolygon_buffer;
DROP TABLE test_multipolygon_boolean;
DROP TABLE test_multipolygon_relationships;
DROP TABLE test_multipolygon_extraction;
DROP TABLE test_multipolygon_properties;
DROP TABLE test_multipolygon_formats;
DROP TABLE test_multipolygon_holes;
DROP TABLE test_multipolygon_basic;

DROP DATABASE test_spatial_multipolygon;
