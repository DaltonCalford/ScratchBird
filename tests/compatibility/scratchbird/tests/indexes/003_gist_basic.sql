-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Indexes - GiST Basic
-- Description: Comprehensive GiST (Generalized Search Tree) index testing
-- ============================================================================

-- GiST indexes are lossy, tree-structured indexes
-- Supports: geometric types, range types, network types, full-text search
-- Best for: overlapping data, containment queries, nearest neighbor
-- Used for: PostGIS, range types, IP addresses, text search

-- Create test database
CREATE DATABASE test_gist_basic_db;
USE test_gist_basic_db;

-- ============================================================================
-- Section 1: GiST Index on Range Types
-- ============================================================================

CREATE TABLE test_gist_ranges (
    event_id INT PRIMARY KEY,
    event_name VARCHAR(200),
    event_period TSRANGE,
    price_range NUMRANGE
);

-- GiST index on range types
CREATE INDEX idx_event_period_gist ON test_gist_ranges USING GIST (event_period);
CREATE INDEX idx_price_range_gist ON test_gist_ranges USING GIST (price_range);

INSERT INTO test_gist_ranges VALUES
    (1, 'Conference', '[2024-01-15 09:00:00, 2024-01-15 17:00:00)', '[100.00, 500.00)'),
    (2, 'Workshop', '[2024-01-16 10:00:00, 2024-01-16 16:00:00)', '[50.00, 200.00)'),
    (3, 'Seminar', '[2024-01-15 14:00:00, 2024-01-15 18:00:00)', '[75.00, 150.00)'),
    (4, 'Webinar', '[2024-01-17 13:00:00, 2024-01-17 15:00:00)', '[25.00, 100.00)');

-- Overlapping queries (uses GiST index)
SELECT event_id, event_name FROM test_gist_ranges
WHERE event_period && '[2024-01-15 15:00:00, 2024-01-15 16:00:00)'::TSRANGE
ORDER BY event_id;

-- Containment queries
SELECT event_id, event_name FROM test_gist_ranges
WHERE event_period @> '2024-01-15 15:30:00'::TIMESTAMP
ORDER BY event_id;

-- Range containment
SELECT event_id, event_name FROM test_gist_ranges
WHERE price_range @> 100.00::NUMERIC
ORDER BY event_id;

-- ============================================================================
-- Section 2: GiST Index on Geometric Types
-- ============================================================================

CREATE TABLE test_gist_geometry (
    location_id INT PRIMARY KEY,
    location_name VARCHAR(200),
    area POLYGON,
    center_point POINT
);

-- GiST index on geometric types
CREATE INDEX idx_area_gist ON test_gist_geometry USING GIST (area);
CREATE INDEX idx_center_gist ON test_gist_geometry USING GIST (center_point);

INSERT INTO test_gist_geometry VALUES
    (1, 'Downtown', POLYGON(POINT(0,0), POINT(10,0), POINT(10,10), POINT(0,10)), POINT(5, 5)),
    (2, 'Suburb', POLYGON(POINT(15,15), POINT(25,15), POINT(25,25), POINT(15,25)), POINT(20, 20)),
    (3, 'Park', POLYGON(POINT(5,5), POINT(8,5), POINT(8,8), POINT(5,8)), POINT(6.5, 6.5));

-- Containment query
SELECT location_id, location_name FROM test_gist_geometry
WHERE area @> POINT(6, 6)
ORDER BY location_id;

-- Overlap query
SELECT location_id, location_name FROM test_gist_geometry
WHERE area && POLYGON(POINT(4,4), POINT(7,4), POINT(7,7), POINT(4,7))
ORDER BY location_id;

-- Distance query (nearest neighbor)
SELECT location_id, location_name,
       center_point <-> POINT(0, 0) AS distance
FROM test_gist_geometry
ORDER BY distance
LIMIT 2;

-- ============================================================================
-- Section 3: GiST Index on Network Types (INET/CIDR)
-- ============================================================================

CREATE TABLE test_gist_network (
    network_id INT PRIMARY KEY,
    network_name VARCHAR(100),
    network_range CIDR,
    gateway_ip INET
);

-- GiST index on network types
CREATE INDEX idx_network_range_gist ON test_gist_network USING GIST (network_range);
CREATE INDEX idx_gateway_gist ON test_gist_network USING GIST (gateway_ip);

INSERT INTO test_gist_network VALUES
    (1, 'Internal Network', '192.168.0.0/16', '192.168.1.1'),
    (2, 'DMZ', '10.0.0.0/24', '10.0.0.1'),
    (3, 'Guest WiFi', '172.16.0.0/24', '172.16.0.1'),
    (4, 'VPN', '10.8.0.0/24', '10.8.0.1');

-- Find network containing IP
SELECT network_id, network_name FROM test_gist_network
WHERE network_range >>= '192.168.10.50'::INET
ORDER BY masklen(network_range) DESC
LIMIT 1;

-- Overlapping networks
SELECT n1.network_name AS net1, n2.network_name AS net2
FROM test_gist_network n1
JOIN test_gist_network n2 ON n1.network_id < n2.network_id
WHERE n1.network_range && n2.network_range;

-- ============================================================================
-- Section 4: GiST Index on TSVECTOR (Full-Text Search)
-- ============================================================================

CREATE TABLE test_gist_fulltext (
    doc_id INT PRIMARY KEY,
    title TEXT,
    content TEXT,
    search_vector TSVECTOR
);

-- GiST index on TSVECTOR (alternative to GIN)
CREATE INDEX idx_search_gist ON test_gist_fulltext USING GIST (search_vector);

INSERT INTO test_gist_fulltext VALUES
    (1, 'Database Systems', 'Introduction to relational databases',
        to_tsvector('english', 'Database Systems Introduction to relational databases')),
    (2, 'Full-Text Search', 'Text search capabilities in databases',
        to_tsvector('english', 'Full-Text Search Text search capabilities in databases')),
    (3, 'Index Types', 'Different types of database indexes',
        to_tsvector('english', 'Index Types Different types of database indexes'));

-- Text search using GiST index
SELECT doc_id, title FROM test_gist_fulltext
WHERE search_vector @@ to_tsquery('english', 'database')
ORDER BY doc_id;

-- ============================================================================
-- Section 5: GiST Index for Exclusion Constraints
-- ============================================================================

CREATE TABLE test_gist_exclusion (
    reservation_id INT PRIMARY KEY,
    room_name VARCHAR(100),
    reserved_period TSRANGE,
    EXCLUDE USING GIST (room_name WITH =, reserved_period WITH &&)
);

-- Insert non-overlapping reservations
INSERT INTO test_gist_exclusion VALUES
    (1, 'Conference Room A', '[2024-01-15 09:00:00, 2024-01-15 11:00:00)'),
    (2, 'Conference Room A', '[2024-01-15 11:00:00, 2024-01-15 13:00:00)'),
    (3, 'Conference Room B', '[2024-01-15 09:00:00, 2024-01-15 11:00:00)');

-- This would fail: overlapping reservation
-- INSERT INTO test_gist_exclusion VALUES
--     (4, 'Conference Room A', '[2024-01-15 10:00:00, 2024-01-15 12:00:00)');

SELECT * FROM test_gist_exclusion ORDER BY room_name, reservation_id;

-- ============================================================================
-- Section 6: GiST Index on Date Ranges
-- ============================================================================

CREATE TABLE test_gist_date_ranges (
    project_id INT PRIMARY KEY,
    project_name VARCHAR(200),
    project_timeline DATERANGE,
    budget_range NUMRANGE
);

CREATE INDEX idx_timeline_gist ON test_gist_date_ranges USING GIST (project_timeline);
CREATE INDEX idx_budget_gist ON test_gist_date_ranges USING GIST (budget_range);

INSERT INTO test_gist_date_ranges VALUES
    (1, 'Website Redesign', '[2024-01-01, 2024-03-31)', '[10000, 50000)'),
    (2, 'Mobile App', '[2024-02-01, 2024-06-30)', '[50000, 150000)'),
    (3, 'Database Migration', '[2024-03-15, 2024-05-15)', '[20000, 60000)');

-- Find projects active in a date range
SELECT project_id, project_name FROM test_gist_date_ranges
WHERE project_timeline && '[2024-02-15, 2024-03-15)'::DATERANGE
ORDER BY project_id;

-- Find projects within budget
SELECT project_id, project_name FROM test_gist_date_ranges
WHERE budget_range @> 40000::NUMERIC
ORDER BY project_id;

-- ============================================================================
-- Section 7: GiST Index Performance - Range Queries
-- ============================================================================

CREATE TABLE test_gist_performance (
    id INT PRIMARY KEY,
    time_range TSRANGE,
    value_range INT4RANGE
);

CREATE INDEX idx_time_range_gist ON test_gist_performance USING GIST (time_range);
CREATE INDEX idx_value_range_gist ON test_gist_performance USING GIST (value_range);

-- Insert test data
INSERT INTO test_gist_performance
SELECT i,
       tsrange(
           '2024-01-01 00:00:00'::TIMESTAMP + (i || ' hours')::INTERVAL,
           '2024-01-01 00:00:00'::TIMESTAMP + ((i + 2) || ' hours')::INTERVAL
       ),
       int4range(i * 10, (i + 5) * 10)
FROM generate_series(1, 1000) i;

-- Overlap queries (fast with GiST)
SELECT id FROM test_gist_performance
WHERE time_range && '[2024-01-01 20:00:00, 2024-01-01 22:00:00)'::TSRANGE
ORDER BY id LIMIT 10;

SELECT id FROM test_gist_performance
WHERE value_range && '[100, 200)'::INT4RANGE
ORDER BY id LIMIT 10;

-- ============================================================================
-- Section 8: GiST Index on Box Type
-- ============================================================================

CREATE TABLE test_gist_boxes (
    object_id INT PRIMARY KEY,
    object_name VARCHAR(100),
    bounding_box BOX
);

CREATE INDEX idx_box_gist ON test_gist_boxes USING GIST (bounding_box);

INSERT INTO test_gist_boxes VALUES
    (1, 'Object A', BOX(POINT(0, 0), POINT(10, 10))),
    (2, 'Object B', BOX(POINT(5, 5), POINT(15, 15))),
    (3, 'Object C', BOX(POINT(20, 20), POINT(30, 30)));

-- Overlap query
SELECT object_id, object_name FROM test_gist_boxes
WHERE bounding_box && BOX(POINT(8, 8), POINT(12, 12))
ORDER BY object_id;

-- Containment query
SELECT object_id, object_name FROM test_gist_boxes
WHERE bounding_box @> POINT(7, 7)
ORDER BY object_id;

-- ============================================================================
-- Section 9: GiST Index on Circle Type
-- ============================================================================

CREATE TABLE test_gist_circles (
    area_id INT PRIMARY KEY,
    area_name VARCHAR(100),
    coverage_area CIRCLE
);

CREATE INDEX idx_circle_gist ON test_gist_circles USING GIST (coverage_area);

INSERT INTO test_gist_circles VALUES
    (1, 'Coverage Zone A', CIRCLE(POINT(0, 0), 10)),
    (2, 'Coverage Zone B', CIRCLE(POINT(20, 20), 15)),
    (3, 'Coverage Zone C', CIRCLE(POINT(10, 10), 8));

-- Find areas containing a point
SELECT area_id, area_name FROM test_gist_circles
WHERE coverage_area @> POINT(5, 5)
ORDER BY area_id;

-- Overlapping coverage areas
SELECT c1.area_name AS area1, c2.area_name AS area2
FROM test_gist_circles c1
JOIN test_gist_circles c2 ON c1.area_id < c2.area_id
WHERE c1.coverage_area && c2.coverage_area
ORDER BY c1.area_id;

-- ============================================================================
-- Section 10: GiST Index Maintenance
-- ============================================================================

CREATE TABLE test_gist_maintenance (
    id INT PRIMARY KEY,
    time_period TSRANGE
);

CREATE INDEX idx_period_gist_maint ON test_gist_maintenance USING GIST (time_period);

INSERT INTO test_gist_maintenance
SELECT i, tsrange(
    '2024-01-01'::TIMESTAMP + (i || ' hours')::INTERVAL,
    '2024-01-01'::TIMESTAMP + ((i + 1) || ' hours')::INTERVAL
)
FROM generate_series(1, 100) i;

-- Reindex GiST index
REINDEX INDEX idx_period_gist_maint;

-- Vacuum table
VACUUM test_gist_maintenance;

-- ============================================================================
-- Section 11: GiST vs B-tree for Ranges
-- ============================================================================

CREATE TABLE test_gist_vs_btree_ranges (
    id INT PRIMARY KEY,
    time_range TSRANGE,
    start_time TIMESTAMP,
    end_time TIMESTAMP
);

-- GiST index on range
CREATE INDEX idx_range_gist ON test_gist_vs_btree_ranges USING GIST (time_range);

-- B-tree indexes on individual bounds
CREATE INDEX idx_start_btree ON test_gist_vs_btree_ranges (start_time);
CREATE INDEX idx_end_btree ON test_gist_vs_btree_ranges (end_time);

INSERT INTO test_gist_vs_btree_ranges
SELECT i,
       tsrange(
           '2024-01-01'::TIMESTAMP + (i || ' hours')::INTERVAL,
           '2024-01-01'::TIMESTAMP + ((i + 2) || ' hours')::INTERVAL
       ),
       '2024-01-01'::TIMESTAMP + (i || ' hours')::INTERVAL,
       '2024-01-01'::TIMESTAMP + ((i + 2) || ' hours')::INTERVAL
FROM generate_series(1, 100) i;

-- Overlap query: GiST is optimal
SELECT id FROM test_gist_vs_btree_ranges
WHERE time_range && '[2024-01-01 50:00:00, 2024-01-01 52:00:00)'::TSRANGE
LIMIT 5;

-- Range queries: both work, B-tree may be better for simple bounds
SELECT id FROM test_gist_vs_btree_ranges
WHERE start_time >= '2024-01-01 50:00:00'
LIMIT 5;

-- ============================================================================
-- Section 12: GiST Index Size
-- ============================================================================

CREATE TABLE test_gist_size (
    id INT PRIMARY KEY,
    period TSRANGE,
    value_range NUMRANGE
);

CREATE INDEX idx_period_gist_size ON test_gist_size USING GIST (period);
CREATE INDEX idx_value_gist_size ON test_gist_size USING GIST (value_range);

INSERT INTO test_gist_size
SELECT i,
       tsrange(
           '2024-01-01'::TIMESTAMP + (i || ' minutes')::INTERVAL,
           '2024-01-01'::TIMESTAMP + ((i + 30) || ' minutes')::INTERVAL
       ),
       numrange(i::NUMERIC, (i + 100)::NUMERIC)
FROM generate_series(1, 1000) i;

-- Check index sizes
SELECT pg_size_pretty(pg_relation_size('idx_period_gist_size')) AS period_index_size;
SELECT pg_size_pretty(pg_relation_size('idx_value_gist_size')) AS value_index_size;

-- ============================================================================
-- Section 13: GiST Nearest Neighbor Queries
-- ============================================================================

CREATE TABLE test_gist_knn (
    location_id INT PRIMARY KEY,
    location_name VARCHAR(100),
    coordinates POINT
);

CREATE INDEX idx_coordinates_gist ON test_gist_knn USING GIST (coordinates);

INSERT INTO test_gist_knn VALUES
    (1, 'Store A', POINT(0, 0)),
    (2, 'Store B', POINT(10, 10)),
    (3, 'Store C', POINT(5, 5)),
    (4, 'Store D', POINT(20, 20)),
    (5, 'Store E', POINT(15, 15));

-- Find nearest locations to a point
SELECT location_id, location_name,
       coordinates <-> POINT(6, 6) AS distance
FROM test_gist_knn
ORDER BY coordinates <-> POINT(6, 6)
LIMIT 3;

-- ============================================================================
-- Section 14: GiST Index for IP Address Ranges
-- ============================================================================

CREATE TABLE test_gist_ip_ranges (
    range_id INT PRIMARY KEY,
    description VARCHAR(200),
    ip_range CIDR,
    owner VARCHAR(100)
);

CREATE INDEX idx_ip_range_gist ON test_gist_ip_ranges USING GIST (ip_range);

INSERT INTO test_gist_ip_ranges VALUES
    (1, 'Office Network', '192.168.0.0/16', 'Company HQ'),
    (2, 'Data Center', '10.0.0.0/8', 'Cloud Provider'),
    (3, 'Branch Office', '172.16.0.0/12', 'Branch Location');

-- Find which range contains an IP
SELECT range_id, description, owner FROM test_gist_ip_ranges
WHERE ip_range >>= '192.168.100.50'::INET
ORDER BY masklen(ip_range) DESC
LIMIT 1;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_gist_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_gist_best_practices VALUES
    (1, 'GiST indexes are lossy (may return false positives, rechecked)'),
    (2, 'Use GiST for: range types, geometric types, network types'),
    (3, 'Supports overlapping data and containment queries'),
    (4, 'Ideal for @>, <@, &&, <<, >> operators'),
    (5, 'Required for EXCLUDE constraints'),
    (6, 'Supports nearest neighbor queries (<-> operator)'),
    (7, 'Generally larger than B-tree indexes'),
    (8, 'Build time slower than B-tree'),
    (9, 'Query performance excellent for specialized queries'),
    (10, 'GiST for TSVECTOR: use if updates frequent, GIN if static'),
    (11, 'Essential for PostGIS and spatial queries'),
    (12, 'Use for range overlap/containment (temporal, numeric)'),
    (13, 'REINDEX periodically for heavily updated tables'),
    (14, 'Consider buffer size for large GiST indexes'),
    (15, 'GiST is extensible - supports custom data types');

SELECT id, guideline FROM test_gist_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_gist_best_practices;
DROP TABLE test_gist_ip_ranges;
DROP TABLE test_gist_knn;
DROP TABLE test_gist_size;
DROP TABLE test_gist_vs_btree_ranges;
DROP TABLE test_gist_maintenance;
DROP TABLE test_gist_circles;
DROP TABLE test_gist_boxes;
DROP TABLE test_gist_performance;
DROP TABLE test_gist_date_ranges;
DROP TABLE test_gist_exclusion;
DROP TABLE test_gist_fulltext;
DROP TABLE test_gist_network;
DROP TABLE test_gist_geometry;
DROP TABLE test_gist_ranges;

DROP DATABASE test_gist_basic_db;

-- End of GiST index tests
