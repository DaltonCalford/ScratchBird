-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - INET
-- Description: Comprehensive INET (IP address with optional netmask) testing
-- ============================================================================

-- INET stores IPv4 or IPv6 addresses with optional CIDR netmask
-- Format: address or address/prefix_length
-- Used for: IP address storage, network configuration, access control

-- Create test database
CREATE DATABASE test_inet_db;
USE test_inet_db;

-- ============================================================================
-- Section 1: Basic INET - IPv4 Addresses
-- ============================================================================

CREATE TABLE test_inet_ipv4_basic (
    id INT PRIMARY KEY,
    ip_address INET,
    description VARCHAR(200)
);

-- IPv4 addresses without netmask
INSERT INTO test_inet_ipv4_basic VALUES (1, '192.168.1.1', 'Private network host');
INSERT INTO test_inet_ipv4_basic VALUES (2, '10.0.0.1', 'Private class A');
INSERT INTO test_inet_ipv4_basic VALUES (3, '172.16.0.1', 'Private class B');
INSERT INTO test_inet_ipv4_basic VALUES (4, '8.8.8.8', 'Public DNS server');
INSERT INTO test_inet_ipv4_basic VALUES (5, '127.0.0.1', 'Localhost');
INSERT INTO test_inet_ipv4_basic VALUES (6, '0.0.0.0', 'All addresses');
INSERT INTO test_inet_ipv4_basic VALUES (7, '255.255.255.255', 'Broadcast address');

SELECT id, ip_address, description FROM test_inet_ipv4_basic ORDER BY id;

-- ============================================================================
-- Section 2: INET with CIDR Notation
-- ============================================================================

CREATE TABLE test_inet_cidr (
    id INT PRIMARY KEY,
    network INET,
    description VARCHAR(200)
);

-- IPv4 with netmask
INSERT INTO test_inet_cidr VALUES (1, '192.168.1.0/24', 'Class C network');
INSERT INTO test_inet_cidr VALUES (2, '10.0.0.0/8', 'Large private network');
INSERT INTO test_inet_cidr VALUES (3, '172.16.0.0/12', 'Medium private network');
INSERT INTO test_inet_cidr VALUES (4, '192.168.1.100/32', 'Single host');
INSERT INTO test_inet_cidr VALUES (5, '0.0.0.0/0', 'All IPv4 addresses');
INSERT INTO test_inet_cidr VALUES (6, '192.168.1.128/25', 'Subnet with 126 hosts');
INSERT INTO test_inet_cidr VALUES (7, '10.1.2.3/16', 'Host in /16 network');

SELECT id, network, description FROM test_inet_cidr ORDER BY id;

-- ============================================================================
-- Section 3: IPv6 Addresses
-- ============================================================================

CREATE TABLE test_inet_ipv6 (
    id INT PRIMARY KEY,
    ip_address INET,
    description VARCHAR(200)
);

-- IPv6 addresses
INSERT INTO test_inet_ipv6 VALUES (1, '::1', 'IPv6 localhost');
INSERT INTO test_inet_ipv6 VALUES (2, '::', 'All zeros');
INSERT INTO test_inet_ipv6 VALUES (3, '2001:db8::1', 'Documentation address');
INSERT INTO test_inet_ipv6 VALUES (4, 'fe80::1', 'Link-local address');
INSERT INTO test_inet_ipv6 VALUES (5, 'ff02::1', 'Multicast all nodes');
INSERT INTO test_inet_ipv6 VALUES (6, '2001:db8:85a3::8a2e:370:7334', 'Full IPv6 address');
INSERT INTO test_inet_ipv6 VALUES (7, '::ffff:192.0.2.1', 'IPv4-mapped IPv6');

-- IPv6 with prefix length
INSERT INTO test_inet_ipv6 VALUES (8, '2001:db8::/32', 'Documentation prefix');
INSERT INTO test_inet_ipv6 VALUES (9, 'fe80::/10', 'Link-local prefix');
INSERT INTO test_inet_ipv6 VALUES (10, '2001:db8::1/128', 'Single IPv6 host');

SELECT id, ip_address, description FROM test_inet_ipv6 ORDER BY id;

-- ============================================================================
-- Section 4: INET Functions - host, masklen, netmask
-- ============================================================================

CREATE TABLE test_inet_functions (
    id INT PRIMARY KEY,
    address INET
);

INSERT INTO test_inet_functions VALUES
    (1, '192.168.1.100/24'),
    (2, '10.0.0.5/8'),
    (3, '172.16.5.10/16'),
    (4, '8.8.8.8'),
    (5, '2001:db8::1/64');

-- host: extract IP address without netmask
SELECT id, address, host(address) AS ip_only FROM test_inet_functions ORDER BY id;

-- masklen: get netmask length
SELECT id, address, masklen(address) AS prefix_length FROM test_inet_functions ORDER BY id;

-- netmask: get netmask as IP
SELECT id, address, netmask(address) AS subnet_mask
FROM test_inet_functions WHERE id <= 4 ORDER BY id;

-- hostmask: get host mask (inverse of netmask)
SELECT id, address, hostmask(address) AS host_mask
FROM test_inet_functions WHERE id <= 4 ORDER BY id;

-- network: get network address
SELECT id, address, network(address) AS network_addr FROM test_inet_functions ORDER BY id;

-- broadcast: get broadcast address (IPv4 only)
SELECT id, address, broadcast(address) AS broadcast_addr
FROM test_inet_functions WHERE id <= 4 ORDER BY id;

-- ============================================================================
-- Section 5: INET Operators - Containment
-- ============================================================================

CREATE TABLE test_inet_containment (
    id INT PRIMARY KEY,
    network INET,
    description VARCHAR(200)
);

INSERT INTO test_inet_containment VALUES
    (1, '192.168.0.0/16', 'Large private network'),
    (2, '192.168.1.0/24', 'Subnet of above'),
    (3, '192.168.1.100/32', 'Single host in subnet'),
    (4, '10.0.0.0/8', 'Different network');

-- >>= operator: is contained by or equals (subnet check)
SELECT id, description FROM test_inet_containment
WHERE network >>= '192.168.0.0/16'::INET
ORDER BY id;

-- <<= operator: contains or equals
SELECT id, description FROM test_inet_containment
WHERE network <<= '192.168.1.100'::INET
ORDER BY id;

-- >> operator: is contained by (strict)
SELECT id, description FROM test_inet_containment
WHERE network >> '192.168.0.0/16'::INET
ORDER BY id;

-- << operator: contains (strict)
SELECT id, description FROM test_inet_containment
WHERE network << '192.168.1.100'::INET
ORDER BY id;

-- && operator: overlaps (shares any addresses)
SELECT n1.description AS network1, n2.description AS network2
FROM test_inet_containment n1
CROSS JOIN test_inet_containment n2
WHERE n1.id < n2.id AND n1.network && n2.network
ORDER BY n1.id, n2.id;

-- ============================================================================
-- Section 6: INET Comparison Operators
-- ============================================================================

CREATE TABLE test_inet_comparison (
    id INT PRIMARY KEY,
    ip_address INET
);

INSERT INTO test_inet_comparison VALUES
    (1, '192.168.1.1'),
    (2, '192.168.1.10'),
    (3, '192.168.1.100'),
    (4, '192.168.2.1'),
    (5, '10.0.0.1');

-- Equality
SELECT id FROM test_inet_comparison WHERE ip_address = '192.168.1.10'::INET;

-- Less than / Greater than
SELECT id, ip_address FROM test_inet_comparison ORDER BY ip_address;

-- Range queries
SELECT id, ip_address FROM test_inet_comparison
WHERE ip_address >= '192.168.1.10'::INET
  AND ip_address <= '192.168.1.100'::INET
ORDER BY id;

-- ============================================================================
-- Section 7: INET Arithmetic
-- ============================================================================

CREATE TABLE test_inet_arithmetic (
    id INT PRIMARY KEY,
    base_address INET
);

INSERT INTO test_inet_arithmetic VALUES
    (1, '192.168.1.1'),
    (2, '10.0.0.0/24'),
    (3, '2001:db8::1');

-- Add integer to INET
SELECT id, base_address, base_address + 10 AS plus_10 FROM test_inet_arithmetic ORDER BY id;

-- Subtract integer from INET
SELECT id, base_address, base_address - 5 AS minus_5 FROM test_inet_arithmetic WHERE id = 1;

-- Difference between two INETs
SELECT '192.168.1.100'::INET - '192.168.1.50'::INET AS address_diff;
SELECT '10.0.1.0'::INET - '10.0.0.0'::INET AS address_diff;

-- ============================================================================
-- Section 8: INET Type Conversions
-- ============================================================================

-- text to INET
SELECT '192.168.1.1'::INET AS from_text;
SELECT '2001:db8::1/64'::INET AS ipv6_from_text;

-- INET to text
SELECT host('192.168.1.100/24'::INET) AS ip_as_text;
SELECT text('192.168.1.0/24'::INET) AS with_netmask;

-- abbrev: abbreviated display format
SELECT id, address, abbrev(address) AS abbreviated FROM test_inet_functions ORDER BY id;

-- ============================================================================
-- Section 9: Real-world Use Case - IP Address Allocation
-- ============================================================================

CREATE TABLE test_inet_allocation (
    allocation_id INT PRIMARY KEY,
    ip_address INET,
    hostname VARCHAR(200),
    device_type VARCHAR(100),
    allocated_date DATE
);

CREATE INDEX idx_ip_address ON test_inet_allocation (ip_address);

INSERT INTO test_inet_allocation VALUES
    (1, '192.168.1.1', 'gateway.local', 'Router', '2024-01-01'),
    (2, '192.168.1.10', 'server01.local', 'Server', '2024-01-02'),
    (3, '192.168.1.20', 'workstation01', 'Desktop', '2024-01-03'),
    (4, '192.168.1.100', 'printer01', 'Printer', '2024-01-04'),
    (5, '192.168.2.10', 'server02.local', 'Server', '2024-01-05');

-- Find device by IP
SELECT hostname, device_type FROM test_inet_allocation
WHERE ip_address = '192.168.1.10'::INET;

-- Find all devices in subnet
SELECT hostname, ip_address, device_type FROM test_inet_allocation
WHERE ip_address << '192.168.1.0/24'::INET
ORDER BY ip_address;

-- Find next available IP in range
WITH allocated_ips AS (
    SELECT ip_address FROM test_inet_allocation
    WHERE ip_address << '192.168.1.0/24'::INET
),
possible_ips AS (
    SELECT ('192.168.1.0'::INET + i) AS ip
    FROM generate_series(2, 254) i
)
SELECT ip AS next_available
FROM possible_ips
WHERE ip NOT IN (SELECT ip_address FROM allocated_ips)
ORDER BY ip LIMIT 1;

-- ============================================================================
-- Section 10: Real-world Use Case - Access Control Lists
-- ============================================================================

CREATE TABLE test_inet_acl (
    rule_id INT PRIMARY KEY,
    rule_name VARCHAR(100),
    source_network INET,
    action VARCHAR(20),
    priority INT
);

INSERT INTO test_inet_acl VALUES
    (1, 'Allow Internal Network', '192.168.0.0/16', 'ALLOW', 10),
    (2, 'Allow VPN Network', '10.8.0.0/24', 'ALLOW', 10),
    (3, 'Block Suspicious Network', '203.0.113.0/24', 'DENY', 5),
    (4, 'Allow All', '0.0.0.0/0', 'ALLOW', 100);

-- Check if IP is allowed
SELECT rule_name, action FROM test_inet_acl
WHERE source_network >>= '192.168.1.50'::INET
ORDER BY priority
LIMIT 1;

SELECT rule_name, action FROM test_inet_acl
WHERE source_network >>= '203.0.113.100'::INET
ORDER BY priority
LIMIT 1;

-- Find all rules that apply to specific IP
SELECT rule_name, source_network, action, priority
FROM test_inet_acl
WHERE source_network >>= '10.8.0.50'::INET
ORDER BY priority;

-- ============================================================================
-- Section 11: Real-world Use Case - DHCP Leases
-- ============================================================================

CREATE TABLE test_inet_dhcp (
    lease_id INT PRIMARY KEY,
    ip_address INET,
    mac_address MACADDR,
    hostname VARCHAR(200),
    lease_start TIMESTAMP,
    lease_end TIMESTAMP
);

INSERT INTO test_inet_dhcp VALUES
    (1, '192.168.1.50', '00:11:22:33:44:55', 'laptop01', '2024-01-15 10:00:00', '2024-01-16 10:00:00'),
    (2, '192.168.1.51', '00:11:22:33:44:56', 'phone01', '2024-01-15 11:00:00', '2024-01-16 11:00:00'),
    (3, '192.168.1.52', '00:11:22:33:44:57', 'tablet01', '2024-01-15 12:00:00', '2024-01-16 12:00:00');

-- Active leases
SELECT ip_address, hostname FROM test_inet_dhcp
WHERE lease_end > CURRENT_TIMESTAMP
ORDER BY ip_address;

-- Find lease by IP
SELECT hostname, mac_address, lease_end FROM test_inet_dhcp
WHERE ip_address = '192.168.1.50'::INET;

-- Expired leases
SELECT ip_address, hostname FROM test_inet_dhcp
WHERE lease_end < CURRENT_TIMESTAMP
ORDER BY lease_end;

-- ============================================================================
-- Section 12: Real-world Use Case - Network Monitoring
-- ============================================================================

CREATE TABLE test_inet_monitoring (
    event_id INT PRIMARY KEY,
    source_ip INET,
    destination_ip INET,
    event_type VARCHAR(50),
    event_time TIMESTAMP,
    bytes_transferred BIGINT
);

INSERT INTO test_inet_monitoring VALUES
    (1, '192.168.1.100', '8.8.8.8', 'DNS_QUERY', '2024-01-15 10:00:00', 512),
    (2, '192.168.1.100', '93.184.216.34', 'HTTP_REQUEST', '2024-01-15 10:01:00', 1024),
    (3, '203.0.113.50', '192.168.1.10', 'SSH_ATTEMPT', '2024-01-15 10:02:00', 256),
    (4, '192.168.1.50', '10.0.0.5', 'FILE_TRANSFER', '2024-01-15 10:03:00', 1048576);

-- Find external traffic
SELECT event_id, source_ip, destination_ip, event_type
FROM test_inet_monitoring
WHERE NOT (source_ip << '192.168.0.0/16'::INET AND destination_ip << '192.168.0.0/16'::INET)
ORDER BY event_id;

-- Find traffic from specific subnet
SELECT event_id, destination_ip, event_type, bytes_transferred
FROM test_inet_monitoring
WHERE source_ip << '192.168.1.0/24'::INET
ORDER BY event_id;

-- Suspicious SSH attempts from external IPs
SELECT event_id, source_ip, event_time
FROM test_inet_monitoring
WHERE event_type = 'SSH_ATTEMPT'
  AND NOT (source_ip << '192.168.0.0/16'::INET)
ORDER BY event_time;

-- ============================================================================
-- Section 13: GiST Indexing for Network Queries
-- ============================================================================

CREATE TABLE test_inet_gist (
    record_id INT PRIMARY KEY,
    ip_address INET
);

CREATE INDEX idx_ip_gist ON test_inet_gist USING GIST (ip_address);

INSERT INTO test_inet_gist
SELECT i, ('192.168.0.0'::INET + i)
FROM generate_series(1, 10000) i;

-- Queries using GiST index
SELECT record_id FROM test_inet_gist
WHERE ip_address << '192.168.1.0/24'::INET
ORDER BY record_id LIMIT 10;

SELECT record_id FROM test_inet_gist
WHERE ip_address = '192.168.1.100'::INET;

-- ============================================================================
-- Section 14: IPv4 vs IPv6 Handling
-- ============================================================================

CREATE TABLE test_inet_mixed (
    id INT PRIMARY KEY,
    ip_address INET,
    ip_version INT
);

INSERT INTO test_inet_mixed VALUES
    (1, '192.168.1.1', 4),
    (2, '10.0.0.1', 4),
    (3, '::1', 6),
    (4, '2001:db8::1', 6),
    (5, '::ffff:192.0.2.1', 6);

-- Filter by IP version using family()
SELECT id, ip_address FROM test_inet_mixed
WHERE family(ip_address) = 4
ORDER BY id;

SELECT id, ip_address FROM test_inet_mixed
WHERE family(ip_address) = 6
ORDER BY id;

-- inet_same_family: check if same family
SELECT i1.ip_address AS ip1,
       i2.ip_address AS ip2,
       inet_same_family(i1.ip_address, i2.ip_address) AS same_family
FROM test_inet_mixed i1, test_inet_mixed i2
WHERE i1.id = 1 AND i2.id IN (2, 3)
ORDER BY i2.id;

-- ============================================================================
-- Section 15: Subnet Calculations
-- ============================================================================

CREATE TABLE test_inet_subnets (
    subnet_id INT PRIMARY KEY,
    network INET,
    description VARCHAR(200)
);

INSERT INTO test_inet_subnets VALUES
    (1, '192.168.1.0/24', 'Main office network'),
    (2, '192.168.2.0/24', 'Branch office network'),
    (3, '10.0.0.0/8', 'Large private network');

-- Calculate number of hosts
SELECT subnet_id, network,
       2 ^ (32 - masklen(network)) - 2 AS usable_hosts
FROM test_inet_subnets
WHERE family(network) = 4
ORDER BY subnet_id;

-- First and last usable host
SELECT subnet_id, network,
       network(network) + 1 AS first_host,
       broadcast(network) - 1 AS last_host
FROM test_inet_subnets
WHERE family(network) = 4
ORDER BY subnet_id;

-- ============================================================================
-- Section 16: NULL Handling
-- ============================================================================

CREATE TABLE test_inet_nulls (
    id INT PRIMARY KEY,
    ip_address INET
);

INSERT INTO test_inet_nulls VALUES
    (1, '192.168.1.1'),
    (2, NULL),
    (3, '10.0.0.1');

SELECT id, ip_address, ip_address IS NULL AS is_null FROM test_inet_nulls ORDER BY id;

-- Operations with NULL
SELECT id, host(ip_address) AS host_addr FROM test_inet_nulls ORDER BY id;
SELECT id, masklen(ip_address) AS mask_len FROM test_inet_nulls ORDER BY id;

-- ============================================================================
-- Section 17: Special Addresses
-- ============================================================================

CREATE TABLE test_inet_special (
    id INT PRIMARY KEY,
    address INET,
    description VARCHAR(200)
);

INSERT INTO test_inet_special VALUES
    (1, '0.0.0.0', 'Default route'),
    (2, '127.0.0.1', 'Loopback'),
    (3, '169.254.0.0/16', 'Link-local'),
    (4, '224.0.0.0/4', 'Multicast'),
    (5, '255.255.255.255', 'Limited broadcast'),
    (6, '::1', 'IPv6 loopback'),
    (7, 'fe80::/10', 'IPv6 link-local'),
    (8, 'ff00::/8', 'IPv6 multicast');

SELECT id, address, description FROM test_inet_special ORDER BY id;

-- Check if address is in special range
SELECT id FROM test_inet_special
WHERE '127.0.0.1'::INET << address
ORDER BY id;

-- ============================================================================
-- Section 18: Performance Comparison
-- ============================================================================

CREATE TABLE test_inet_performance (
    id INT PRIMARY KEY,
    note TEXT
);

INSERT INTO test_inet_performance VALUES
    (1, 'INET stores both address and netmask'),
    (2, 'GiST indexes enable fast subnet containment queries'),
    (3, 'B-tree indexes good for equality and range queries'),
    (4, 'IPv6 addresses take more space than IPv4'),
    (5, 'Use host() to extract IP without netmask for display'),
    (6, 'Containment operators (<<, >>) very efficient with GiST'),
    (7, 'INET can store both IPv4 and IPv6 in same column'),
    (8, 'Arithmetic operations (+, -) useful for IP ranges');

SELECT id, note FROM test_inet_performance ORDER BY id;

-- ============================================================================
-- Section 19: Best Practices
-- ============================================================================

CREATE TABLE test_inet_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_inet_best_practices VALUES
    (1, 'Use INET for IP addresses with optional netmask'),
    (2, 'Use CIDR if netmask is always required'),
    (3, 'Create GiST indexes for subnet containment queries (<<, >>)'),
    (4, 'Create B-tree indexes for equality and range queries'),
    (5, 'Use << operator to check if IP is in subnet'),
    (6, 'Use && operator to check if networks overlap'),
    (7, 'Use host() to display IP without netmask'),
    (8, 'Use masklen() to get prefix length'),
    (9, 'Use network() to get network address'),
    (10, 'Use broadcast() to get broadcast address (IPv4)'),
    (11, 'INET ideal for: IP allocation, ACLs, DHCP, network monitoring'),
    (12, 'family() returns 4 for IPv4, 6 for IPv6'),
    (13, 'Supports both IPv4 and IPv6 in same column'),
    (14, 'Arithmetic: add/subtract integers to get next/previous IPs'),
    (15, 'Use abbrev() for compact display format');

SELECT id, guideline FROM test_inet_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_inet_best_practices;
DROP TABLE test_inet_performance;
DROP TABLE test_inet_special;
DROP TABLE test_inet_nulls;
DROP TABLE test_inet_subnets;
DROP TABLE test_inet_mixed;
DROP TABLE test_inet_gist;
DROP TABLE test_inet_monitoring;
DROP TABLE test_inet_dhcp;
DROP TABLE test_inet_acl;
DROP TABLE test_inet_allocation;
DROP TABLE test_inet_arithmetic;
DROP TABLE test_inet_comparison;
DROP TABLE test_inet_containment;
DROP TABLE test_inet_functions;
DROP TABLE test_inet_ipv6;
DROP TABLE test_inet_cidr;
DROP TABLE test_inet_ipv4_basic;

DROP DATABASE test_inet_db;

-- End of INET data type tests
