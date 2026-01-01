-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - CIDR
-- Description: Comprehensive CIDR (network address) testing
-- ============================================================================

-- CIDR stores network addresses in CIDR notation (address/prefix_length)
-- Unlike INET, CIDR does not accept addresses with host bits set
-- Used for: routing tables, network definitions, subnet planning

-- Create test database
CREATE DATABASE test_cidr_db;
USE test_cidr_db;

-- ============================================================================
-- Section 1: Basic CIDR - Network Addresses
-- ============================================================================

CREATE TABLE test_cidr_basic (
    id INT PRIMARY KEY,
    network CIDR,
    description VARCHAR(200)
);

-- Valid CIDR networks (host bits must be zero)
INSERT INTO test_cidr_basic VALUES (1, '192.168.1.0/24', 'Class C network');
INSERT INTO test_cidr_basic VALUES (2, '10.0.0.0/8', 'Large private network');
INSERT INTO test_cidr_basic VALUES (3, '172.16.0.0/12', 'Medium private network');
INSERT INTO test_cidr_basic VALUES (4, '192.168.0.0/16', 'Large private subnet');
INSERT INTO test_cidr_basic VALUES (5, '0.0.0.0/0', 'Default route');
INSERT INTO test_cidr_basic VALUES (6, '192.168.1.128/25', 'Half of /24 subnet');
INSERT INTO test_cidr_basic VALUES (7, '10.1.0.0/16', 'Corporate network');

-- Invalid: host bits set (would be rejected)
-- INSERT INTO test_cidr_basic VALUES (8, '192.168.1.100/24', 'Invalid - host bits set');

SELECT id, network, description FROM test_cidr_basic ORDER BY id;

-- ============================================================================
-- Section 2: CIDR vs INET Differences
-- ============================================================================

CREATE TABLE test_cidr_vs_inet (
    id INT PRIMARY KEY,
    cidr_value CIDR,
    inet_value INET,
    description VARCHAR(200)
);

-- CIDR: network address only
INSERT INTO test_cidr_vs_inet VALUES
    (1, '192.168.1.0/24', '192.168.1.100/24', 'CIDR=network, INET=host in network');

INSERT INTO test_cidr_vs_inet VALUES
    (2, '10.0.0.0/8', '10.5.10.15/8', 'CIDR=network, INET=host in network');

-- Single host: both CIDR and INET accept /32
INSERT INTO test_cidr_vs_inet VALUES
    (3, '192.168.1.1/32', '192.168.1.1/32', 'Single host - both accept');

SELECT id, cidr_value, inet_value, description FROM test_cidr_vs_inet ORDER BY id;

-- Convert INET to CIDR (extracts network)
SELECT id, inet_value, network(inet_value)::CIDR AS as_cidr
FROM test_cidr_vs_inet ORDER BY id;

-- ============================================================================
-- Section 3: IPv6 CIDR Networks
-- ============================================================================

CREATE TABLE test_cidr_ipv6 (
    id INT PRIMARY KEY,
    network CIDR,
    description VARCHAR(200)
);

INSERT INTO test_cidr_ipv6 VALUES
    (1, '::/0', 'All IPv6 addresses'),
    (2, '2001:db8::/32', 'Documentation prefix'),
    (3, 'fe80::/10', 'Link-local prefix'),
    (4, 'ff00::/8', 'Multicast prefix'),
    (5, '2001:db8:1234::/48', 'Organization network'),
    (6, '2001:db8:1234:5678::/64', 'Subnet'),
    (7, '::1/128', 'IPv6 loopback');

SELECT id, network, description FROM test_cidr_ipv6 ORDER BY id;

-- ============================================================================
-- Section 4: CIDR Functions
-- ============================================================================

CREATE TABLE test_cidr_functions (
    id INT PRIMARY KEY,
    network CIDR
);

INSERT INTO test_cidr_functions VALUES
    (1, '192.168.1.0/24'),
    (2, '10.0.0.0/8'),
    (3, '172.16.0.0/12'),
    (4, '2001:db8::/32');

-- masklen: get prefix length
SELECT id, network, masklen(network) AS prefix_len FROM test_cidr_functions ORDER BY id;

-- netmask: get netmask
SELECT id, network, netmask(network) AS netmask
FROM test_cidr_functions WHERE family(network) = 4 ORDER BY id;

-- hostmask: get host mask
SELECT id, network, hostmask(network) AS hostmask
FROM test_cidr_functions WHERE family(network) = 4 ORDER BY id;

-- broadcast: get broadcast address
SELECT id, network, broadcast(network) AS broadcast
FROM test_cidr_functions WHERE family(network) = 4 ORDER BY id;

-- network: get network address (same as input for CIDR)
SELECT id, network, network(network) AS net_addr FROM test_cidr_functions ORDER BY id;

-- abbrev: abbreviated display
SELECT id, network, abbrev(network) AS abbreviated FROM test_cidr_functions ORDER BY id;

-- ============================================================================
-- Section 5: Containment Operators
-- ============================================================================

CREATE TABLE test_cidr_containment (
    id INT PRIMARY KEY,
    network CIDR,
    description VARCHAR(200)
);

INSERT INTO test_cidr_containment VALUES
    (1, '192.168.0.0/16', 'Large network'),
    (2, '192.168.1.0/24', 'Subnet of /16'),
    (3, '192.168.1.128/25', 'Subnet of /24'),
    (4, '10.0.0.0/8', 'Different network');

-- >>= operator: is subnet of or equal to
SELECT id, description FROM test_cidr_containment
WHERE network >>= '192.168.0.0/16'::CIDR
ORDER BY id;

-- <<= operator: contains or equals
SELECT id, description FROM test_cidr_containment
WHERE network <<= '192.168.1.128/25'::CIDR
ORDER BY id;

-- >> operator: is subnet of (strict)
SELECT id, description FROM test_cidr_containment
WHERE network >> '192.168.0.0/16'::CIDR
ORDER BY id;

-- << operator: contains (strict)
SELECT id, description FROM test_cidr_containment
WHERE network << '192.168.1.128/25'::CIDR
ORDER BY id;

-- && operator: networks overlap
SELECT n1.description AS net1, n2.description AS net2
FROM test_cidr_containment n1
CROSS JOIN test_cidr_containment n2
WHERE n1.id < n2.id AND n1.network && n2.network
ORDER BY n1.id, n2.id;

-- ============================================================================
-- Section 6: CIDR Comparison
-- ============================================================================

CREATE TABLE test_cidr_comparison (
    id INT PRIMARY KEY,
    network CIDR
);

INSERT INTO test_cidr_comparison VALUES
    (1, '10.0.0.0/8'),
    (2, '172.16.0.0/12'),
    (3, '192.168.0.0/16'),
    (4, '192.168.1.0/24');

-- Equality
SELECT id FROM test_cidr_comparison WHERE network = '192.168.1.0/24'::CIDR;

-- Inequality
SELECT id FROM test_cidr_comparison WHERE network != '192.168.1.0/24'::CIDR ORDER BY id;

-- Ordering (by network address, then prefix length)
SELECT id, network FROM test_cidr_comparison ORDER BY network;

-- ============================================================================
-- Section 7: Real-world Use Case - Routing Tables
-- ============================================================================

CREATE TABLE test_cidr_routing (
    route_id INT PRIMARY KEY,
    destination CIDR,
    gateway INET,
    interface VARCHAR(50),
    metric INT
);

INSERT INTO test_cidr_routing VALUES
    (1, '0.0.0.0/0', '192.168.1.1', 'eth0', 100),
    (2, '192.168.1.0/24', '0.0.0.0', 'eth0', 0),
    (3, '10.0.0.0/8', '192.168.1.254', 'eth0', 10),
    (4, '172.16.0.0/12', '192.168.1.254', 'eth0', 10),
    (5, '192.168.100.0/24', '192.168.1.2', 'eth1', 5);

-- Find route for specific IP (longest prefix match)
SELECT destination, gateway, interface
FROM test_cidr_routing
WHERE destination >>= '10.5.10.20'::INET
ORDER BY masklen(destination) DESC
LIMIT 1;

SELECT destination, gateway, interface
FROM test_cidr_routing
WHERE destination >>= '192.168.100.50'::INET
ORDER BY masklen(destination) DESC
LIMIT 1;

-- Default route
SELECT destination, gateway FROM test_cidr_routing
WHERE destination = '0.0.0.0/0'::CIDR;

-- ============================================================================
-- Section 8: Real-world Use Case - Subnet Planning
-- ============================================================================

CREATE TABLE test_cidr_subnet_plan (
    subnet_id INT PRIMARY KEY,
    department VARCHAR(100),
    network CIDR,
    vlan_id INT
);

INSERT INTO test_cidr_subnet_plan VALUES
    (1, 'Engineering', '192.168.10.0/24', 10),
    (2, 'Sales', '192.168.20.0/24', 20),
    (3, 'HR', '192.168.30.0/24', 30),
    (4, 'Guest WiFi', '192.168.100.0/24', 100),
    (5, 'Management', '192.168.1.0/24', 1);

-- Calculate available addresses per subnet
SELECT subnet_id, department, network,
       2 ^ (32 - masklen(network)) - 2 AS usable_hosts
FROM test_cidr_subnet_plan
ORDER BY subnet_id;

-- Check for overlapping subnets
SELECT s1.department AS dept1, s2.department AS dept2
FROM test_cidr_subnet_plan s1
CROSS JOIN test_cidr_subnet_plan s2
WHERE s1.subnet_id < s2.subnet_id AND s1.network && s2.network;

-- Find subnet for department
SELECT network FROM test_cidr_subnet_plan WHERE department = 'Engineering';

-- ============================================================================
-- Section 9: Real-world Use Case - Firewall Rules
-- ============================================================================

CREATE TABLE test_cidr_firewall (
    rule_id INT PRIMARY KEY,
    source_network CIDR,
    destination_network CIDR,
    action VARCHAR(20),
    description VARCHAR(200)
);

INSERT INTO test_cidr_firewall VALUES
    (1, '192.168.0.0/16', '0.0.0.0/0', 'ALLOW', 'Internal to any'),
    (2, '0.0.0.0/0', '192.168.1.0/24', 'DENY', 'External to management'),
    (3, '10.0.0.0/8', '192.168.0.0/16', 'ALLOW', 'VPN to internal'),
    (4, '0.0.0.0/0', '192.168.100.0/24', 'ALLOW', 'Internet to DMZ');

-- Check if traffic is allowed
SELECT rule_id, action, description
FROM test_cidr_firewall
WHERE source_network >>= '192.168.10.50'::INET
  AND destination_network >>= '8.8.8.8'::INET
ORDER BY rule_id
LIMIT 1;

-- Find rules for specific network
SELECT rule_id, source_network, destination_network, action
FROM test_cidr_firewall
WHERE source_network >>= '10.8.0.0/24'::CIDR
ORDER BY rule_id;

-- ============================================================================
-- Section 10: Real-world Use Case - IP Block Allocation
-- ============================================================================

CREATE TABLE test_cidr_allocation (
    allocation_id INT PRIMARY KEY,
    organization VARCHAR(200),
    allocated_block CIDR,
    allocation_date DATE,
    contact_email VARCHAR(200)
);

INSERT INTO test_cidr_allocation VALUES
    (1, 'Company A', '203.0.113.0/24', '2024-01-01', 'admin@companya.com'),
    (2, 'Company B', '198.51.100.0/24', '2024-01-15', 'admin@companyb.com'),
    (3, 'Company C', '192.0.2.0/24', '2024-02-01', 'admin@companyc.com');

-- Find organization for IP
SELECT organization, allocated_block FROM test_cidr_allocation
WHERE allocated_block >>= '203.0.113.50'::INET;

-- List all allocations
SELECT organization, allocated_block,
       2 ^ (32 - masklen(allocated_block)) AS total_addresses
FROM test_cidr_allocation
ORDER BY allocation_date;

-- ============================================================================
-- Section 11: Subnet Subdivision
-- ============================================================================

CREATE TABLE test_cidr_subdivision (
    id INT PRIMARY KEY,
    parent_network CIDR,
    description VARCHAR(200)
);

INSERT INTO test_cidr_subdivision VALUES
    (1, '192.168.0.0/16', 'Parent network to subdivide');

-- Subdivide /16 into /24 subnets
WITH subnets AS (
    SELECT ('192.168.0.0'::INET + (i * 256))::CIDR AS subnet
    FROM generate_series(0, 255) i
)
SELECT subnet FROM subnets LIMIT 10;

-- Subdivide /24 into /25 subnets
SELECT ('192.168.1.0/25'::CIDR) AS first_half,
       ('192.168.1.128/25'::CIDR) AS second_half;

-- ============================================================================
-- Section 12: GiST Indexing
-- ============================================================================

CREATE TABLE test_cidr_indexing (
    network_id INT PRIMARY KEY,
    network CIDR,
    description VARCHAR(200)
);

CREATE INDEX idx_network_gist ON test_cidr_indexing USING GIST (network);

-- Insert sample networks
INSERT INTO test_cidr_indexing VALUES
    (1, '10.0.0.0/8', 'Private Class A'),
    (2, '172.16.0.0/12', 'Private Class B'),
    (3, '192.168.0.0/16', 'Private Class C');

-- Generate many /24 subnets
INSERT INTO test_cidr_indexing
SELECT 1000 + i,
       ('192.168.0.0'::INET + (i * 256))::CIDR,
       'Subnet ' || i
FROM generate_series(0, 255) i;

-- Queries using GiST index
SELECT network_id, network FROM test_cidr_indexing
WHERE network >>= '192.168.10.0/24'::CIDR
ORDER BY masklen(network) DESC
LIMIT 5;

SELECT network_id FROM test_cidr_indexing
WHERE network << '192.168.5.50'::INET
ORDER BY masklen(network) DESC
LIMIT 5;

-- ============================================================================
-- Section 13: Private vs Public Networks
-- ============================================================================

CREATE TABLE test_cidr_network_types (
    id INT PRIMARY KEY,
    network CIDR,
    network_type VARCHAR(50)
);

INSERT INTO test_cidr_network_types VALUES
    (1, '10.0.0.0/8', 'Private'),
    (2, '172.16.0.0/12', 'Private'),
    (3, '192.168.0.0/16', 'Private'),
    (4, '169.254.0.0/16', 'Link-local'),
    (5, '127.0.0.0/8', 'Loopback'),
    (6, '224.0.0.0/4', 'Multicast'),
    (7, '0.0.0.0/8', 'Reserved');

-- Check if IP is private
SELECT network_type FROM test_cidr_network_types
WHERE network >>= '192.168.100.50'::INET AND network_type = 'Private';

SELECT network_type FROM test_cidr_network_types
WHERE network >>= '10.5.10.20'::INET AND network_type = 'Private';

-- Find network type for IP
SELECT network, network_type FROM test_cidr_network_types
WHERE network >>= '169.254.1.1'::INET
ORDER BY masklen(network) DESC
LIMIT 1;

-- ============================================================================
-- Section 14: NULL Handling
-- ============================================================================

CREATE TABLE test_cidr_nulls (
    id INT PRIMARY KEY,
    network CIDR
);

INSERT INTO test_cidr_nulls VALUES
    (1, '192.168.1.0/24'),
    (2, NULL),
    (3, '10.0.0.0/8');

SELECT id, network, network IS NULL AS is_null FROM test_cidr_nulls ORDER BY id;

-- Operations with NULL
SELECT id, masklen(network) AS prefix FROM test_cidr_nulls ORDER BY id;
SELECT id, broadcast(network) AS bcast FROM test_cidr_nulls ORDER BY id;

-- ============================================================================
-- Section 15: CIDR Aggregation
-- ============================================================================

CREATE TABLE test_cidr_aggregation (
    id INT PRIMARY KEY,
    network CIDR
);

-- Adjacent /25 networks that could be aggregated into /24
INSERT INTO test_cidr_aggregation VALUES
    (1, '192.168.1.0/25'),
    (2, '192.168.1.128/25');

-- Show that they can be aggregated
SELECT '192.168.1.0/24'::CIDR AS aggregated_network;

-- Multiple /26 networks
INSERT INTO test_cidr_aggregation VALUES
    (3, '192.168.2.0/26'),
    (4, '192.168.2.64/26'),
    (5, '192.168.2.128/26'),
    (6, '192.168.2.192/26');

-- Can be aggregated to /24
SELECT '192.168.2.0/24'::CIDR AS aggregated_network;

-- ============================================================================
-- Section 16: Special Network Addresses
-- ============================================================================

CREATE TABLE test_cidr_special (
    id INT PRIMARY KEY,
    network CIDR,
    description VARCHAR(200)
);

INSERT INTO test_cidr_special VALUES
    (1, '0.0.0.0/8', 'Current network'),
    (2, '127.0.0.0/8', 'Loopback'),
    (3, '169.254.0.0/16', 'Link-local (APIPA)'),
    (4, '192.0.0.0/24', 'IETF protocol assignments'),
    (5, '192.0.2.0/24', 'Documentation (TEST-NET-1)'),
    (6, '198.51.100.0/24', 'Documentation (TEST-NET-2)'),
    (7, '203.0.113.0/24', 'Documentation (TEST-NET-3)'),
    (8, '224.0.0.0/4', 'Multicast'),
    (9, '240.0.0.0/4', 'Reserved');

SELECT id, network, description FROM test_cidr_special ORDER BY id;

-- ============================================================================
-- Section 17: IPv6 Special Networks
-- ============================================================================

CREATE TABLE test_cidr_ipv6_special (
    id INT PRIMARY KEY,
    network CIDR,
    description VARCHAR(200)
);

INSERT INTO test_cidr_ipv6_special VALUES
    (1, '::/128', 'Unspecified address'),
    (2, '::1/128', 'Loopback'),
    (3, 'fe80::/10', 'Link-local unicast'),
    (4, 'fc00::/7', 'Unique local unicast'),
    (5, 'ff00::/8', 'Multicast'),
    (6, '2001:db8::/32', 'Documentation'),
    (7, '2002::/16', '6to4'),
    (8, '2001::/32', 'TEREDO');

SELECT id, network, description FROM test_cidr_ipv6_special ORDER BY id;

-- ============================================================================
-- Section 18: Best Practices
-- ============================================================================

CREATE TABLE test_cidr_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_cidr_best_practices VALUES
    (1, 'Use CIDR for network addresses (no host bits set)'),
    (2, 'Use INET for individual host addresses'),
    (3, 'CIDR enforces network address format (host bits = 0)'),
    (4, 'Create GiST indexes for subnet containment queries'),
    (5, 'Use << operator for longest prefix match (routing)'),
    (6, 'Use && operator to check for overlapping subnets'),
    (7, 'CIDR ideal for: routing tables, subnet planning, firewall rules'),
    (8, 'Supports both IPv4 and IPv6 networks'),
    (9, 'Use masklen() to get prefix length'),
    (10, 'Use broadcast() for broadcast address (IPv4)'),
    (11, 'Convert INET to CIDR using network()'),
    (12, 'CIDR prevents storing invalid network addresses'),
    (13, 'Ordering: by network address, then prefix length'),
    (14, 'Use for IP block allocation, subnet subdivision'),
    (15, 'Aggregation: combine adjacent subnets into larger blocks');

SELECT id, guideline FROM test_cidr_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_cidr_best_practices;
DROP TABLE test_cidr_ipv6_special;
DROP TABLE test_cidr_special;
DROP TABLE test_cidr_aggregation;
DROP TABLE test_cidr_nulls;
DROP TABLE test_cidr_network_types;
DROP TABLE test_cidr_indexing;
DROP TABLE test_cidr_subdivision;
DROP TABLE test_cidr_allocation;
DROP TABLE test_cidr_firewall;
DROP TABLE test_cidr_subnet_plan;
DROP TABLE test_cidr_routing;
DROP TABLE test_cidr_comparison;
DROP TABLE test_cidr_containment;
DROP TABLE test_cidr_functions;
DROP TABLE test_cidr_ipv6;
DROP TABLE test_cidr_vs_inet;
DROP TABLE test_cidr_basic;

DROP DATABASE test_cidr_db;

-- End of CIDR data type tests
