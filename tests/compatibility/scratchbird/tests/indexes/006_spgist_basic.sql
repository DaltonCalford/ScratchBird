-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Indexes - SP-GiST Basic
-- Description: Comprehensive SP-GiST (Space-Partitioned GiST) testing
-- ============================================================================

-- SP-GiST indexes support non-balanced data structures
-- Supports: points, ranges, IP addresses, text prefix searches
-- Best for: quad-trees, k-d trees, radix trees, suffix trees
-- Characteristics: Partitioned search space, good for clustered data
-- Note: SP-GiST is more specialized than GiST, supports different data patterns

-- Create test database
CREATE DATABASE test_spgist_basic_db;
USE test_spgist_basic_db;

-- ============================================================================
-- Section 1: SP-GiST on POINT (Quad-Tree)
-- ============================================================================

CREATE TABLE test_spgist_points (
    location_id INT PRIMARY KEY,
    location_name VARCHAR(100),
    coordinates POINT
);

-- SP-GiST using quad-tree for 2D points
CREATE INDEX idx_coords_spgist ON test_spgist_points USING SPGIST (coordinates);

INSERT INTO test_spgist_points VALUES
    (1, 'Location A', POINT(10, 20)),
    (2, 'Location B', POINT(15, 25)),
    (3, 'Location C', POINT(5, 30)),
    (4, 'Location D', POINT(25, 15)),
    (5, 'Location E', POINT(12, 18)),
    (6, 'Location F', POINT(8, 22)),
    (7, 'Location G', POINT(20, 28)),
    (8, 'Location H', POINT(30, 10));

-- Exact point lookup
SELECT location_id, location_name FROM test_spgist_points
WHERE coordinates = POINT(15, 25);

-- Points within a box
SELECT location_id, location_name, coordinates FROM test_spgist_points
WHERE coordinates <@ BOX(POINT(0, 0), POINT(20, 30));

-- Nearest neighbor search
SELECT location_id, location_name, coordinates <-> POINT(12, 20) AS distance
FROM test_spgist_points
ORDER BY coordinates <-> POINT(12, 20)
LIMIT 3;

-- ============================================================================
-- Section 2: SP-GiST on TEXT (Prefix Search with Radix Tree)
-- ============================================================================

CREATE TABLE test_spgist_text (
    id INT PRIMARY KEY,
    keyword VARCHAR(100)
);

-- SP-GiST using text pattern ops for prefix searches
CREATE INDEX idx_keyword_spgist ON test_spgist_text USING SPGIST (keyword text_pattern_ops);

INSERT INTO test_spgist_text VALUES
    (1, 'postgresql'),
    (2, 'postgres'),
    (3, 'postgis'),
    (4, 'python'),
    (5, 'pythonic'),
    (6, 'database'),
    (7, 'datascience'),
    (8, 'programming');

-- Prefix search (starts with)
SELECT id, keyword FROM test_spgist_text WHERE keyword LIKE 'post%';
SELECT id, keyword FROM test_spgist_text WHERE keyword LIKE 'pyth%';
SELECT id, keyword FROM test_spgist_text WHERE keyword LIKE 'data%';

-- Exact match
SELECT id, keyword FROM test_spgist_text WHERE keyword = 'postgresql';

-- ============================================================================
-- Section 3: SP-GiST on INET (IP Address Trie)
-- ============================================================================

CREATE TABLE test_spgist_inet (
    ip_id INT PRIMARY KEY,
    ip_address INET,
    location VARCHAR(100)
);

-- SP-GiST for IP addresses (uses trie structure)
CREATE INDEX idx_ip_spgist ON test_spgist_inet USING SPGIST (ip_address);

INSERT INTO test_spgist_inet VALUES
    (1, '192.168.1.1'::INET, 'Office Network'),
    (2, '192.168.1.50'::INET, 'Office Network'),
    (3, '192.168.2.1'::INET, 'Guest Network'),
    (4, '10.0.0.1'::INET, 'Internal Network'),
    (5, '10.0.0.100'::INET, 'Internal Network'),
    (6, '172.16.0.1'::INET, 'DMZ'),
    (7, '2001:db8::1'::INET, 'IPv6 Network'),
    (8, '2001:db8::100'::INET, 'IPv6 Network');

-- Exact IP lookup
SELECT ip_id, ip_address, location FROM test_spgist_inet
WHERE ip_address = '192.168.1.1'::INET;

-- Subnet containment
SELECT ip_id, ip_address FROM test_spgist_inet
WHERE ip_address <<= '192.168.1.0/24'::INET;

-- IP range query
SELECT ip_id, ip_address FROM test_spgist_inet
WHERE ip_address >= '192.168.1.0'::INET AND ip_address <= '192.168.1.255'::INET;

-- ============================================================================
-- Section 4: SP-GiST on BOX (2D Rectangles)
-- ============================================================================

CREATE TABLE test_spgist_boxes (
    region_id INT PRIMARY KEY,
    region_name VARCHAR(100),
    bounds BOX
);

CREATE INDEX idx_bounds_spgist ON test_spgist_boxes USING SPGIST (bounds);

INSERT INTO test_spgist_boxes VALUES
    (1, 'Region A', BOX(POINT(0, 0), POINT(10, 10))),
    (2, 'Region B', BOX(POINT(5, 5), POINT(15, 15))),
    (3, 'Region C', BOX(POINT(20, 20), POINT(30, 30))),
    (4, 'Region D', BOX(POINT(10, 10), POINT(20, 20)));

-- Find boxes containing a point
SELECT region_id, region_name FROM test_spgist_boxes
WHERE bounds @> POINT(7, 7);

-- Find overlapping boxes
SELECT region_id, region_name FROM test_spgist_boxes
WHERE bounds && BOX(POINT(8, 8), POINT(12, 12));

-- ============================================================================
-- Section 5: SP-GiST for Range Types
-- ============================================================================

CREATE TABLE test_spgist_ranges (
    period_id INT PRIMARY KEY,
    event_name VARCHAR(200),
    event_period TSRANGE
);

CREATE INDEX idx_period_spgist ON test_spgist_ranges USING SPGIST (event_period);

INSERT INTO test_spgist_ranges VALUES
    (1, 'Morning Meeting', '[2024-01-15 09:00:00, 2024-01-15 10:00:00)'::TSRANGE),
    (2, 'Lunch Break', '[2024-01-15 12:00:00, 2024-01-15 13:00:00)'::TSRANGE),
    (3, 'Afternoon Session', '[2024-01-15 14:00:00, 2024-01-15 17:00:00)'::TSRANGE),
    (4, 'Evening Event', '[2024-01-15 18:00:00, 2024-01-15 21:00:00)'::TSRANGE);

-- Find overlapping ranges
SELECT period_id, event_name FROM test_spgist_ranges
WHERE event_period && '[2024-01-15 15:00:00, 2024-01-15 16:00:00)'::TSRANGE;

-- Find ranges containing a timestamp
SELECT period_id, event_name FROM test_spgist_ranges
WHERE event_period @> '2024-01-15 15:30:00'::TIMESTAMP;

-- ============================================================================
-- Section 6: SP-GiST for Geographic Clustering
-- ============================================================================

CREATE TABLE test_spgist_geo_cluster (
    store_id INT PRIMARY KEY,
    store_name VARCHAR(100),
    location POINT,
    store_type VARCHAR(50)
);

CREATE INDEX idx_location_cluster_spgist ON test_spgist_geo_cluster USING SPGIST (location);

-- Insert geographically clustered stores
INSERT INTO test_spgist_geo_cluster VALUES
    (1, 'Store A1', POINT(40.7128, -74.0060), 'grocery'),  -- New York area
    (2, 'Store A2', POINT(40.7589, -73.9851), 'grocery'),  -- New York area
    (3, 'Store B1', POINT(34.0522, -118.2437), 'electronics'),  -- LA area
    (4, 'Store B2', POINT(34.0195, -118.4912), 'electronics'),  -- LA area
    (5, 'Store C1', POINT(41.8781, -87.6298), 'clothing'),  -- Chicago area
    (6, 'Store C2', POINT(41.8500, -87.6501), 'clothing');  -- Chicago area

-- Find stores near a point
SELECT store_id, store_name, location <-> POINT(40.75, -74.00) AS distance
FROM test_spgist_geo_cluster
ORDER BY location <-> POINT(40.75, -74.00)
LIMIT 3;

-- Find stores in a bounding box
SELECT store_id, store_name FROM test_spgist_geo_cluster
WHERE location <@ BOX(POINT(40, -75), POINT(42, -73));

-- ============================================================================
-- Section 7: SP-GiST for Text Prefix Autocomplete
-- ============================================================================

CREATE TABLE test_spgist_autocomplete (
    term_id INT PRIMARY KEY,
    search_term VARCHAR(200),
    popularity INT
);

CREATE INDEX idx_term_spgist ON test_spgist_autocomplete USING SPGIST (search_term text_pattern_ops);

INSERT INTO test_spgist_autocomplete VALUES
    (1, 'database administration', 100),
    (2, 'database design', 150),
    (3, 'database management', 200),
    (4, 'database optimization', 120),
    (5, 'data science', 300),
    (6, 'data analysis', 250),
    (7, 'data visualization', 180),
    (8, 'python programming', 400),
    (9, 'python tutorial', 350),
    (10, 'postgresql database', 220);

-- Autocomplete suggestions (prefix search)
SELECT term_id, search_term, popularity FROM test_spgist_autocomplete
WHERE search_term LIKE 'data%'
ORDER BY popularity DESC;

SELECT term_id, search_term FROM test_spgist_autocomplete
WHERE search_term LIKE 'python%'
ORDER BY popularity DESC;

-- ============================================================================
-- Section 8: SP-GiST vs GiST for Points
-- ============================================================================

CREATE TABLE test_spgist_vs_gist_points (
    point_id INT PRIMARY KEY,
    coordinates POINT
);

-- Create both SP-GiST and GiST indexes
CREATE INDEX idx_spgist_coords ON test_spgist_vs_gist_points USING SPGIST (coordinates);
CREATE INDEX idx_gist_coords ON test_spgist_vs_gist_points USING GIST (coordinates);

INSERT INTO test_spgist_vs_gist_points
SELECT i, POINT(random() * 100, random() * 100)
FROM generate_series(1, 10000) i;

-- Nearest neighbor query (both can be used)
SELECT point_id FROM test_spgist_vs_gist_points
ORDER BY coordinates <-> POINT(50, 50)
LIMIT 5;

-- Box containment (both can be used)
SELECT point_id FROM test_spgist_vs_gist_points
WHERE coordinates <@ BOX(POINT(40, 40), POINT(60, 60))
LIMIT 10;

-- SP-GiST: Better for clustered/partitioned data, quad-tree structure
-- GiST: Better for overlapping ranges, R-tree structure

-- ============================================================================
-- Section 9: SP-GiST for Domain Name Hierarchy
-- ============================================================================

CREATE TABLE test_spgist_domains (
    domain_id INT PRIMARY KEY,
    domain_name VARCHAR(200),
    registrar VARCHAR(100)
);

CREATE INDEX idx_domain_spgist ON test_spgist_domains USING SPGIST (domain_name text_pattern_ops);

INSERT INTO test_spgist_domains VALUES
    (1, 'example.com', 'Registrar A'),
    (2, 'subdomain.example.com', 'Registrar A'),
    (3, 'api.example.com', 'Registrar A'),
    (4, 'testsite.org', 'Registrar B'),
    (5, 'www.testsite.org', 'Registrar B'),
    (6, 'mysite.net', 'Registrar C');

-- Prefix search for domains
SELECT domain_id, domain_name FROM test_spgist_domains
WHERE domain_name LIKE 'example.%';

SELECT domain_id, domain_name FROM test_spgist_domains
WHERE domain_name LIKE 'test%';

-- ============================================================================
-- Section 10: SP-GiST for IP Address Allocation
-- ============================================================================

CREATE TABLE test_spgist_ip_allocation (
    allocation_id INT PRIMARY KEY,
    network INET,
    description VARCHAR(200),
    vlan_id INT
);

CREATE INDEX idx_network_spgist ON test_spgist_ip_allocation USING SPGIST (network);

INSERT INTO test_spgist_ip_allocation VALUES
    (1, '10.0.0.0/8'::INET, 'Private Network A', 100),
    (2, '172.16.0.0/12'::INET, 'Private Network B', 200),
    (3, '192.168.0.0/16'::INET, 'Private Network C', 300),
    (4, '192.168.1.0/24'::INET, 'Office Subnet', 301),
    (5, '192.168.2.0/24'::INET, 'Guest Subnet', 302),
    (6, '10.10.0.0/16'::INET, 'Data Center', 101);

-- Find network containing an IP
SELECT allocation_id, network, description FROM test_spgist_ip_allocation
WHERE network >>= '192.168.1.50'::INET
ORDER BY masklen(network) DESC
LIMIT 1;

-- Find subnets within a larger network
SELECT allocation_id, network FROM test_spgist_ip_allocation
WHERE network <<= '192.168.0.0/16'::INET AND network != '192.168.0.0/16'::INET;

-- ============================================================================
-- Section 11: SP-GiST for Spatial Queries
-- ============================================================================

CREATE TABLE test_spgist_spatial (
    poi_id INT PRIMARY KEY,
    poi_name VARCHAR(100),
    category VARCHAR(50),
    location POINT
);

CREATE INDEX idx_poi_location_spgist ON test_spgist_spatial USING SPGIST (location);

INSERT INTO test_spgist_spatial VALUES
    (1, 'Restaurant A', 'food', POINT(40.7580, -73.9855)),
    (2, 'Coffee Shop B', 'cafe', POINT(40.7614, -73.9776)),
    (3, 'Park C', 'recreation', POINT(40.7829, -73.9654)),
    (4, 'Museum D', 'culture', POINT(40.7794, -73.9632)),
    (5, 'Theater E', 'entertainment', POINT(40.7590, -73.9845)),
    (6, 'Library F', 'education', POINT(40.7532, -73.9822));

-- Find points of interest near user location
SELECT poi_id, poi_name, category,
       location <-> POINT(40.76, -73.98) AS distance
FROM test_spgist_spatial
ORDER BY location <-> POINT(40.76, -73.98)
LIMIT 5;

-- Find POIs in a region
SELECT poi_id, poi_name, category FROM test_spgist_spatial
WHERE location <@ BOX(POINT(40.75, -74.00), POINT(40.79, -73.96));

-- ============================================================================
-- Section 12: SP-GiST Performance with Large Dataset
-- ============================================================================

CREATE TABLE test_spgist_performance (
    id INT PRIMARY KEY,
    coords POINT,
    ip_addr INET,
    prefix VARCHAR(50)
);

CREATE INDEX idx_coords_perf_spgist ON test_spgist_performance USING SPGIST (coords);
CREATE INDEX idx_ip_perf_spgist ON test_spgist_performance USING SPGIST (ip_addr);
CREATE INDEX idx_prefix_perf_spgist ON test_spgist_performance USING SPGIST (prefix text_pattern_ops);

INSERT INTO test_spgist_performance
SELECT i,
       POINT(random() * 1000, random() * 1000),
       ('10.' || (i % 256) || '.' || ((i / 256) % 256) || '.1')::INET,
       'prefix_' || (i % 100)
FROM generate_series(1, 100000) i;

-- Nearest neighbor on points
SELECT id FROM test_spgist_performance
ORDER BY coords <-> POINT(500, 500)
LIMIT 10;

-- IP subnet query
SELECT id FROM test_spgist_performance
WHERE ip_addr <<= '10.50.0.0/16'::INET
LIMIT 10;

-- Prefix search
SELECT id FROM test_spgist_performance
WHERE prefix LIKE 'prefix_5%'
LIMIT 10;

-- ============================================================================
-- Section 13: SP-GiST Index Maintenance
-- ============================================================================

CREATE TABLE test_spgist_maintenance (
    id INT PRIMARY KEY,
    location POINT,
    label VARCHAR(100)
);

CREATE INDEX idx_maint_spgist ON test_spgist_maintenance USING SPGIST (location);

INSERT INTO test_spgist_maintenance
SELECT i, POINT(random() * 100, random() * 100), 'Point ' || i
FROM generate_series(1, 1000) i;

-- Reindex SP-GiST
REINDEX INDEX idx_maint_spgist;

-- Vacuum
VACUUM test_spgist_maintenance;

-- ============================================================================
-- Section 14: SP-GiST Use Cases
-- ============================================================================

CREATE TABLE test_spgist_use_cases (
    id INT PRIMARY KEY,
    use_case TEXT,
    example TEXT
);

INSERT INTO test_spgist_use_cases VALUES
    (1, 'Point location queries', 'WHERE coordinates <@ box'),
    (2, 'Nearest neighbor search', 'ORDER BY point <-> target_point'),
    (3, 'IP address routing', 'WHERE network >>= ip_address'),
    (4, 'Text prefix autocomplete', 'WHERE text LIKE ''prefix%'''),
    (5, 'Geographic clustering', 'Quad-tree for spatial partitioning'),
    (6, 'Range overlap queries', 'WHERE range1 && range2'),
    (7, 'Trie-based text search', 'Radix tree for string prefixes'),
    (8, 'Network address lookup', 'Longest prefix match for routing'),
    (9, 'Spatial indexing', '2D point and box queries'),
    (10, 'Domain hierarchy', 'Hierarchical domain name searches');

SELECT id, use_case, example FROM test_spgist_use_cases ORDER BY id;

-- ============================================================================
-- Section 15: SP-GiST Best Practices
-- ============================================================================

CREATE TABLE test_spgist_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_spgist_best_practices VALUES
    (1, 'Use SP-GiST for partitioned/non-balanced data structures'),
    (2, 'Ideal for quad-trees (points), radix trees (text), tries (IP)'),
    (3, 'SP-GiST works well with clustered geographic data'),
    (4, 'Better than GiST for prefix searches on text'),
    (5, 'Supports nearest neighbor queries with <-> operator'),
    (6, 'Use text_pattern_ops for LIKE ''prefix%'' queries'),
    (7, 'Efficient for IP address routing and subnet lookups'),
    (8, 'SP-GiST handles range types (TSRANGE, NUMRANGE, etc.)'),
    (9, 'Not lossy like GiST - exact index entries'),
    (10, 'Good for autocomplete and typeahead search'),
    (11, 'SP-GiST typically smaller than GiST for same data'),
    (12, 'Use for hierarchical data (domains, file paths)'),
    (13, 'Supports BOX, POINT, INET, TEXT with pattern ops'),
    (14, 'Consider B-tree for simple equality/range, SP-GiST for spatial'),
    (15, 'Monitor query performance - SP-GiST benefits vary by data distribution');

SELECT id, guideline FROM test_spgist_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_spgist_best_practices;
DROP TABLE test_spgist_use_cases;
DROP TABLE test_spgist_maintenance;
DROP TABLE test_spgist_performance;
DROP TABLE test_spgist_spatial;
DROP TABLE test_spgist_ip_allocation;
DROP TABLE test_spgist_domains;
DROP TABLE test_spgist_vs_gist_points;
DROP TABLE test_spgist_autocomplete;
DROP TABLE test_spgist_geo_cluster;
DROP TABLE test_spgist_ranges;
DROP TABLE test_spgist_boxes;
DROP TABLE test_spgist_inet;
DROP TABLE test_spgist_text;
DROP TABLE test_spgist_points;

DROP DATABASE test_spgist_basic_db;

-- End of SP-GiST index tests
