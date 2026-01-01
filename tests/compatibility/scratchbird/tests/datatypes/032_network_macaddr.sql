-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - MACADDR
-- Description: Comprehensive MACADDR (6-byte MAC address) testing
-- ============================================================================

-- MACADDR stores 6-byte MAC (Media Access Control) addresses
-- Format: XX:XX:XX:XX:XX:XX (or various other formats)
-- Used for: network device identification, ARP tables, device inventory

-- Create test database
CREATE DATABASE test_macaddr_db;
USE test_macaddr_db;

-- ============================================================================
-- Section 1: Basic MACADDR - Various Input Formats
-- ============================================================================

CREATE TABLE test_macaddr_formats (
    id INT PRIMARY KEY,
    mac_address MACADDR,
    input_format VARCHAR(200)
);

-- Various input formats (all equivalent)
INSERT INTO test_macaddr_formats VALUES (1, '08:00:2b:01:02:03', 'Colon notation');
INSERT INTO test_macaddr_formats VALUES (2, '08-00-2b-01-02-04', 'Hyphen notation');
INSERT INTO test_macaddr_formats VALUES (3, '08002b:010205', 'Cisco format');
INSERT INTO test_macaddr_formats VALUES (4, '08002b010206', 'No separators');
INSERT INTO test_macaddr_formats VALUES (5, '0800.2b01.0207', 'Dot notation (Cisco)');

-- Output is normalized to colon notation
SELECT id, mac_address, input_format FROM test_macaddr_formats ORDER BY id;

-- ============================================================================
-- Section 2: Common MAC Addresses
-- ============================================================================

CREATE TABLE test_macaddr_common (
    id INT PRIMARY KEY,
    mac_address MACADDR,
    description VARCHAR(200)
);

INSERT INTO test_macaddr_common VALUES
    (1, '00:00:00:00:00:00', 'Null MAC'),
    (2, 'ff:ff:ff:ff:ff:ff', 'Broadcast MAC'),
    (3, '01:00:5e:00:00:00', 'IPv4 multicast base'),
    (4, '33:33:00:00:00:00', 'IPv6 multicast base'),
    (5, '00:50:56:00:00:00', 'VMware virtual adapter'),
    (6, '00:0c:29:00:00:00', 'VMware workstation'),
    (7, '08:00:27:00:00:00', 'VirtualBox'),
    (8, 'ac:de:48:00:11:22', 'Dell network card'),
    (9, '00:1b:63:84:45:e6', 'Apple device');

SELECT id, mac_address, description FROM test_macaddr_common ORDER BY id;

-- ============================================================================
-- Section 3: MAC Address Components
-- ============================================================================

CREATE TABLE test_macaddr_components (
    id INT PRIMARY KEY,
    mac_address MACADDR,
    device_type VARCHAR(100)
);

INSERT INTO test_macaddr_components VALUES
    (1, '00:11:22:33:44:55', 'Workstation'),
    (2, 'aa:bb:cc:dd:ee:ff', 'Server'),
    (3, '08:00:2b:01:02:03', 'Router');

-- First 3 bytes (OUI - Organizationally Unique Identifier)
SELECT id, mac_address,
       substring(mac_address::TEXT, 1, 8) AS oui,
       device_type
FROM test_macaddr_components ORDER BY id;

-- Check multicast bit (bit 0 of first byte)
SELECT id, mac_address,
       CASE WHEN get_byte(mac_address::BYTEA, 0) & 1 = 1
            THEN 'Multicast'
            ELSE 'Unicast'
       END AS address_type
FROM test_macaddr_components ORDER BY id;

-- Check local/global bit (bit 1 of first byte)
SELECT id, mac_address,
       CASE WHEN get_byte(mac_address::BYTEA, 0) & 2 = 2
            THEN 'Locally Administered'
            ELSE 'Globally Unique'
       END AS admin_type
FROM test_macaddr_components ORDER BY id;

-- ============================================================================
-- Section 4: MACADDR Comparison
-- ============================================================================

CREATE TABLE test_macaddr_comparison (
    id INT PRIMARY KEY,
    mac_address MACADDR
);

INSERT INTO test_macaddr_comparison VALUES
    (1, '00:00:00:00:00:01'),
    (2, '00:00:00:00:00:0a'),
    (3, '00:00:00:00:01:00'),
    (4, '00:11:22:33:44:55'),
    (5, 'ff:ff:ff:ff:ff:ff');

-- Equality
SELECT id FROM test_macaddr_comparison
WHERE mac_address = '00:11:22:33:44:55'::MACADDR;

-- Inequality
SELECT id FROM test_macaddr_comparison
WHERE mac_address != '00:11:22:33:44:55'::MACADDR
ORDER BY id;

-- Ordering (lexicographic)
SELECT id, mac_address FROM test_macaddr_comparison ORDER BY mac_address;

-- Range query
SELECT id, mac_address FROM test_macaddr_comparison
WHERE mac_address >= '00:00:00:00:01:00'::MACADDR
  AND mac_address <= '00:11:22:33:44:55'::MACADDR
ORDER BY mac_address;

-- ============================================================================
-- Section 5: MACADDR Operators
-- ============================================================================

-- Bitwise AND
SELECT '00:11:22:33:44:55'::MACADDR & 'ff:ff:ff:00:00:00'::MACADDR AS oui_mask;

-- Bitwise OR
SELECT '00:00:00:33:44:55'::MACADDR | '00:11:22:00:00:00'::MACADDR AS combined;

-- Bitwise NOT
SELECT ~'00:00:00:00:00:00'::MACADDR AS inverted;

-- ============================================================================
-- Section 6: Real-world Use Case - Device Inventory
-- ============================================================================

CREATE TABLE test_macaddr_inventory (
    device_id INT PRIMARY KEY,
    hostname VARCHAR(100),
    mac_address MACADDR,
    device_type VARCHAR(50),
    location VARCHAR(100),
    last_seen TIMESTAMP
);

CREATE INDEX idx_mac_address ON test_macaddr_inventory (mac_address);

INSERT INTO test_macaddr_inventory VALUES
    (1, 'server01', '00:50:56:a1:b2:c3', 'Server', 'Rack A1', '2024-01-15 10:00:00'),
    (2, 'workstation05', '00:0c:29:3d:4e:5f', 'Desktop', 'Floor 2 Desk 10', '2024-01-15 09:30:00'),
    (3, 'printer01', 'ac:de:48:11:22:33', 'Printer', 'Floor 1 Copy Room', '2024-01-15 08:00:00'),
    (4, 'ap-lobby', '00:1b:63:84:45:e6', 'Access Point', 'Lobby', '2024-01-15 10:15:00'),
    (5, 'switch-core', '00:11:22:aa:bb:cc', 'Switch', 'Server Room', '2024-01-15 10:20:00');

-- Find device by MAC
SELECT hostname, device_type, location FROM test_macaddr_inventory
WHERE mac_address = '00:0c:29:3d:4e:5f'::MACADDR;

-- Find all VMware devices (OUI 00:50:56 or 00:0c:29)
SELECT device_id, hostname, mac_address FROM test_macaddr_inventory
WHERE mac_address >= '00:50:56:00:00:00'::MACADDR
  AND mac_address <= '00:50:56:ff:ff:ff'::MACADDR
UNION
SELECT device_id, hostname, mac_address FROM test_macaddr_inventory
WHERE mac_address >= '00:0c:29:00:00:00'::MACADDR
  AND mac_address <= '00:0c:29:ff:ff:ff'::MACADDR
ORDER BY device_id;

-- List devices by type
SELECT device_type, COUNT(*) AS count FROM test_macaddr_inventory
GROUP BY device_type ORDER BY count DESC;

-- ============================================================================
-- Section 7: Real-world Use Case - ARP Table
-- ============================================================================

CREATE TABLE test_macaddr_arp (
    entry_id INT PRIMARY KEY,
    ip_address INET,
    mac_address MACADDR,
    interface VARCHAR(20),
    entry_type VARCHAR(20),
    last_updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_macaddr_arp VALUES
    (1, '192.168.1.1', '00:11:22:aa:bb:cc', 'eth0', 'static', '2024-01-15 10:00:00'),
    (2, '192.168.1.10', '00:50:56:a1:b2:c3', 'eth0', 'dynamic', '2024-01-15 10:05:00'),
    (3, '192.168.1.20', '00:0c:29:3d:4e:5f', 'eth0', 'dynamic', '2024-01-15 10:10:00'),
    (4, '192.168.1.100', 'ac:de:48:11:22:33', 'eth0', 'dynamic', '2024-01-15 10:15:00');

-- Find MAC for IP
SELECT mac_address FROM test_macaddr_arp WHERE ip_address = '192.168.1.10'::INET;

-- Find IP for MAC
SELECT ip_address FROM test_macaddr_arp WHERE mac_address = '00:0c:29:3d:4e:5f'::MACADDR;

-- List all ARP entries
SELECT ip_address, mac_address, interface, entry_type
FROM test_macaddr_arp
ORDER BY ip_address;

-- Find stale entries (older than 5 minutes)
SELECT ip_address, mac_address FROM test_macaddr_arp
WHERE entry_type = 'dynamic'
  AND last_updated < CURRENT_TIMESTAMP - INTERVAL '5 minutes'
ORDER BY last_updated;

-- ============================================================================
-- Section 8: Real-world Use Case - DHCP Leases
-- ============================================================================

CREATE TABLE test_macaddr_dhcp (
    lease_id INT PRIMARY KEY,
    mac_address MACADDR,
    ip_address INET,
    hostname VARCHAR(100),
    lease_start TIMESTAMP,
    lease_end TIMESTAMP
);

CREATE INDEX idx_dhcp_mac ON test_macaddr_dhcp (mac_address);

INSERT INTO test_macaddr_dhcp VALUES
    (1, '00:11:22:33:44:55', '192.168.1.50', 'laptop01', '2024-01-15 09:00:00', '2024-01-16 09:00:00'),
    (2, 'aa:bb:cc:dd:ee:ff', '192.168.1.51', 'phone01', '2024-01-15 10:00:00', '2024-01-16 10:00:00'),
    (3, '08:00:2b:01:02:03', '192.168.1.52', 'tablet01', '2024-01-15 11:00:00', '2024-01-16 11:00:00');

-- Find IP assigned to MAC
SELECT ip_address, hostname FROM test_macaddr_dhcp
WHERE mac_address = '00:11:22:33:44:55'::MACADDR
  AND lease_end > CURRENT_TIMESTAMP;

-- Find MAC for IP
SELECT mac_address, hostname FROM test_macaddr_dhcp
WHERE ip_address = '192.168.1.51'::INET
  AND lease_end > CURRENT_TIMESTAMP;

-- Active leases
SELECT mac_address, ip_address, hostname FROM test_macaddr_dhcp
WHERE lease_end > CURRENT_TIMESTAMP
ORDER BY mac_address;

-- Expired leases
SELECT mac_address, ip_address, lease_end FROM test_macaddr_dhcp
WHERE lease_end < CURRENT_TIMESTAMP
ORDER BY lease_end;

-- ============================================================================
-- Section 9: Real-world Use Case - MAC Access Control
-- ============================================================================

CREATE TABLE test_macaddr_acl (
    acl_id INT PRIMARY KEY,
    mac_address MACADDR,
    user_name VARCHAR(100),
    access_level VARCHAR(50),
    allowed BOOLEAN,
    notes VARCHAR(200)
);

INSERT INTO test_macaddr_acl VALUES
    (1, '00:11:22:33:44:55', 'Alice Johnson', 'Full Access', true, 'IT Administrator'),
    (2, 'aa:bb:cc:dd:ee:ff', 'Bob Smith', 'Limited', true, 'Regular User'),
    (3, '08:00:2b:01:02:03', 'Charlie Brown', 'Guest', true, 'Guest Access'),
    (4, 'de:ad:be:ef:00:00', 'Unknown', 'Blocked', false, 'Blacklisted device');

-- Check if MAC is allowed
SELECT allowed, access_level FROM test_macaddr_acl
WHERE mac_address = '00:11:22:33:44:55'::MACADDR;

-- List all allowed MACs
SELECT mac_address, user_name, access_level FROM test_macaddr_acl
WHERE allowed = true
ORDER BY access_level, user_name;

-- Blacklisted MACs
SELECT mac_address, notes FROM test_macaddr_acl
WHERE allowed = false
ORDER BY mac_address;

-- ============================================================================
-- Section 10: Real-world Use Case - Switch Port Mapping
-- ============================================================================

CREATE TABLE test_macaddr_switch_ports (
    mapping_id INT PRIMARY KEY,
    switch_name VARCHAR(100),
    port_number VARCHAR(20),
    mac_address MACADDR,
    vlan_id INT,
    last_seen TIMESTAMP
);

INSERT INTO test_macaddr_switch_ports VALUES
    (1, 'switch-floor1', 'Gi1/0/1', '00:11:22:33:44:55', 10, '2024-01-15 10:00:00'),
    (2, 'switch-floor1', 'Gi1/0/2', 'aa:bb:cc:dd:ee:ff', 10, '2024-01-15 10:05:00'),
    (3, 'switch-floor1', 'Gi1/0/3', '08:00:2b:01:02:03', 20, '2024-01-15 10:10:00'),
    (4, 'switch-floor2', 'Gi1/0/1', 'ac:de:48:11:22:33', 10, '2024-01-15 10:15:00');

-- Find port for MAC address
SELECT switch_name, port_number, vlan_id FROM test_macaddr_switch_ports
WHERE mac_address = '00:11:22:33:44:55'::MACADDR;

-- List all devices on switch
SELECT port_number, mac_address, vlan_id FROM test_macaddr_switch_ports
WHERE switch_name = 'switch-floor1'
ORDER BY port_number;

-- Find all devices on VLAN
SELECT switch_name, port_number, mac_address FROM test_macaddr_switch_ports
WHERE vlan_id = 10
ORDER BY switch_name, port_number;

-- ============================================================================
-- Section 11: Manufacturer Identification (OUI)
-- ============================================================================

CREATE TABLE test_macaddr_oui (
    oui VARCHAR(8) PRIMARY KEY,
    manufacturer VARCHAR(200)
);

INSERT INTO test_macaddr_oui VALUES
    ('00:50:56', 'VMware, Inc.'),
    ('00:0c:29', 'VMware, Inc.'),
    ('08:00:27', 'Oracle VirtualBox'),
    ('ac:de:48', 'Dell Inc.'),
    ('00:1b:63', 'Apple, Inc.'),
    ('00:11:22', 'Example Manufacturer');

-- Join device inventory with manufacturer
SELECT i.device_id, i.hostname, i.mac_address, o.manufacturer
FROM test_macaddr_inventory i
LEFT JOIN test_macaddr_oui o
    ON substring(i.mac_address::TEXT, 1, 8) = o.oui
ORDER BY i.device_id;

-- Count devices by manufacturer
SELECT o.manufacturer, COUNT(*) AS device_count
FROM test_macaddr_inventory i
LEFT JOIN test_macaddr_oui o
    ON substring(i.mac_address::TEXT, 1, 8) = o.oui
GROUP BY o.manufacturer
ORDER BY device_count DESC;

-- ============================================================================
-- Section 12: Duplicate MAC Detection
-- ============================================================================

CREATE TABLE test_macaddr_duplicates (
    record_id INT PRIMARY KEY,
    mac_address MACADDR,
    ip_address INET,
    interface VARCHAR(20),
    detected_time TIMESTAMP
);

INSERT INTO test_macaddr_duplicates VALUES
    (1, '00:11:22:33:44:55', '192.168.1.10', 'eth0', '2024-01-15 10:00:00'),
    (2, '00:11:22:33:44:55', '192.168.1.20', 'eth0', '2024-01-15 10:05:00'),
    (3, 'aa:bb:cc:dd:ee:ff', '192.168.1.30', 'eth1', '2024-01-15 10:10:00');

-- Find duplicate MACs
SELECT mac_address, COUNT(*) AS occurrences,
       array_agg(ip_address::TEXT) AS conflicting_ips
FROM test_macaddr_duplicates
GROUP BY mac_address
HAVING COUNT(*) > 1
ORDER BY mac_address;

-- ============================================================================
-- Section 13: NULL Handling
-- ============================================================================

CREATE TABLE test_macaddr_nulls (
    id INT PRIMARY KEY,
    mac_address MACADDR
);

INSERT INTO test_macaddr_nulls VALUES
    (1, '00:11:22:33:44:55'),
    (2, NULL),
    (3, 'aa:bb:cc:dd:ee:ff');

SELECT id, mac_address, mac_address IS NULL AS is_null FROM test_macaddr_nulls ORDER BY id;

-- ============================================================================
-- Section 14: MAC Address Generation
-- ============================================================================

-- Generate random locally-administered MAC (bit 1 of first byte set)
CREATE TABLE test_macaddr_generated (
    id INT PRIMARY KEY,
    generated_mac MACADDR
);

-- Example: generate MACs with specific OUI
INSERT INTO test_macaddr_generated VALUES
    (1, '02:00:00:00:00:01'),  -- Locally administered
    (2, '02:00:00:00:00:02'),
    (3, '02:00:00:00:00:03');

SELECT id, generated_mac FROM test_macaddr_generated ORDER BY id;

-- ============================================================================
-- Section 15: Conversion and Display
-- ============================================================================

CREATE TABLE test_macaddr_display (
    id INT PRIMARY KEY,
    mac_address MACADDR
);

INSERT INTO test_macaddr_display VALUES
    (1, '00:11:22:33:44:55'),
    (2, 'aa:bb:cc:dd:ee:ff');

-- Convert to text (standard format)
SELECT id, mac_address::TEXT AS mac_text FROM test_macaddr_display ORDER BY id;

-- Convert to bytea
SELECT id, mac_address::TEXT AS mac_string,
       mac_address::BYTEA AS mac_bytes
FROM test_macaddr_display ORDER BY id;

-- ============================================================================
-- Section 16: Best Practices
-- ============================================================================

CREATE TABLE test_macaddr_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_macaddr_best_practices VALUES
    (1, 'Use MACADDR for 6-byte MAC addresses (EUI-48)'),
    (2, 'Use MACADDR8 for 8-byte MAC addresses (EUI-64)'),
    (3, 'Supports multiple input formats (colon, hyphen, Cisco)'),
    (4, 'Output normalized to XX:XX:XX:XX:XX:XX format'),
    (5, 'First 3 bytes (OUI) identify manufacturer'),
    (6, 'Create indexes on MAC for device lookups'),
    (7, 'Use for: device inventory, ARP tables, DHCP, access control'),
    (8, 'Bit 0 of first byte: 0=unicast, 1=multicast'),
    (9, 'Bit 1 of first byte: 0=globally unique, 1=locally administered'),
    (10, 'Comparison operators: =, !=, <, >, <=, >='),
    (11, 'Bitwise operators: &, |, ~ (AND, OR, NOT)'),
    (12, 'Use OUI lookups to identify device manufacturers'),
    (13, 'Detect duplicate MACs by grouping and counting'),
    (14, 'MACADDR ideal for network device management'),
    (15, 'Store MAC-IP mappings for network monitoring');

SELECT id, guideline FROM test_macaddr_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_macaddr_best_practices;
DROP TABLE test_macaddr_display;
DROP TABLE test_macaddr_generated;
DROP TABLE test_macaddr_nulls;
DROP TABLE test_macaddr_duplicates;
DROP TABLE test_macaddr_oui;
DROP TABLE test_macaddr_switch_ports;
DROP TABLE test_macaddr_acl;
DROP TABLE test_macaddr_dhcp;
DROP TABLE test_macaddr_arp;
DROP TABLE test_macaddr_inventory;
DROP TABLE test_macaddr_comparison;
DROP TABLE test_macaddr_components;
DROP TABLE test_macaddr_common;
DROP TABLE test_macaddr_formats;

DROP DATABASE test_macaddr_db;

-- End of MACADDR data type tests
