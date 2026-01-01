-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - MACADDR8
-- Description: Comprehensive MACADDR8 (8-byte MAC address / EUI-64) testing
-- ============================================================================

-- MACADDR8 stores 8-byte MAC addresses (EUI-64 format)
-- Used for: IPv6 link-local addresses, modern network devices, IEEE 802.15.4
-- Format: XX:XX:XX:XX:XX:XX:XX:XX or XX:XX:XX:FF:FE:XX:XX:XX

-- Create test database
CREATE DATABASE test_macaddr8_db;
USE test_macaddr8_db;

-- ============================================================================
-- Section 1: Basic MACADDR8 - EUI-64 Addresses
-- ============================================================================

CREATE TABLE test_macaddr8_basic (
    id INT PRIMARY KEY,
    mac_address MACADDR8,
    description VARCHAR(200)
);

-- Full 8-byte addresses
INSERT INTO test_macaddr8_basic VALUES
    (1, '00:11:22:33:44:55:66:77', 'Full EUI-64 address'),
    (2, '08:00:2b:01:02:03:04:05', 'Another EUI-64'),
    (3, 'aa:bb:cc:dd:ee:ff:00:11', 'Example address'),
    (4, '00:00:00:00:00:00:00:00', 'Null address'),
    (5, 'ff:ff:ff:ff:ff:ff:ff:ff', 'Broadcast address');

-- EUI-64 derived from EUI-48 (6-byte MAC with FF:FE inserted)
INSERT INTO test_macaddr8_basic VALUES
    (6, '00:11:22:ff:fe:33:44:55', 'EUI-48 converted to EUI-64'),
    (7, 'ac:de:48:ff:fe:11:22:33', 'Another converted address');

SELECT id, mac_address, description FROM test_macaddr8_basic ORDER BY id;

-- ============================================================================
-- Section 2: Input Format Variations
-- ============================================================================

CREATE TABLE test_macaddr8_formats (
    id INT PRIMARY KEY,
    mac_address MACADDR8,
    input_format VARCHAR(200)
);

-- Various input formats
INSERT INTO test_macaddr8_formats VALUES
    (1, '00:11:22:33:44:55:66:77', 'Colon notation'),
    (2, '00-11-22-33-44-55-66-78', 'Hyphen notation'),
    (3, '0011223344556679', 'No separators'),
    (4, '00:11:22:ff:fe:33:44:55', 'EUI-48 to EUI-64 format');

SELECT id, mac_address, input_format FROM test_macaddr8_formats ORDER BY id;

-- ============================================================================
-- Section 3: EUI-48 to EUI-64 Conversion
-- ============================================================================

CREATE TABLE test_macaddr8_conversion (
    id INT PRIMARY KEY,
    eui48 MACADDR,
    eui64 MACADDR8,
    description VARCHAR(200)
);

-- EUI-48 (6-byte) and corresponding EUI-64 (8-byte)
INSERT INTO test_macaddr8_conversion VALUES
    (1, '00:11:22:33:44:55', '00:11:22:ff:fe:33:44:55', 'Standard conversion'),
    (2, 'ac:de:48:11:22:33', 'ac:de:48:ff:fe:11:22:33', 'Dell NIC conversion'),
    (3, '08:00:27:ab:cd:ef', '08:00:27:ff:fe:ab:cd:ef', 'VirtualBox conversion');

SELECT id, eui48, eui64, description FROM test_macaddr8_conversion ORDER BY id;

-- Convert MACADDR to MACADDR8 using macaddr8_set7bit function
SELECT id, eui48,
       macaddr8_set7bit(eui48) AS auto_converted
FROM test_macaddr8_conversion ORDER BY id;

-- ============================================================================
-- Section 4: MACADDR8 Functions
-- ============================================================================

CREATE TABLE test_macaddr8_functions (
    id INT PRIMARY KEY,
    mac_address MACADDR8
);

INSERT INTO test_macaddr8_functions VALUES
    (1, '00:11:22:33:44:55:66:77'),
    (2, 'aa:bb:cc:dd:ee:ff:00:11'),
    (3, '02:00:00:ff:fe:00:00:01');

-- Convert to text
SELECT id, mac_address, mac_address::TEXT AS as_text
FROM test_macaddr8_functions ORDER BY id;

-- Convert to BYTEA
SELECT id, mac_address::BYTEA AS as_bytes
FROM test_macaddr8_functions ORDER BY id;

-- Check if locally administered (bit 1 of first byte)
SELECT id, mac_address,
       CASE WHEN get_byte(mac_address::BYTEA, 0) & 2 = 2
            THEN 'Locally Administered'
            ELSE 'Globally Unique'
       END AS admin_type
FROM test_macaddr8_functions ORDER BY id;

-- ============================================================================
-- Section 5: MACADDR8 Comparison
-- ============================================================================

CREATE TABLE test_macaddr8_comparison (
    id INT PRIMARY KEY,
    mac_address MACADDR8
);

INSERT INTO test_macaddr8_comparison VALUES
    (1, '00:00:00:00:00:00:00:01'),
    (2, '00:00:00:00:00:00:01:00'),
    (3, '00:11:22:33:44:55:66:77'),
    (4, 'aa:bb:cc:dd:ee:ff:00:11'),
    (5, 'ff:ff:ff:ff:ff:ff:ff:ff');

-- Equality
SELECT id FROM test_macaddr8_comparison
WHERE mac_address = '00:11:22:33:44:55:66:77'::MACADDR8;

-- Inequality
SELECT id FROM test_macaddr8_comparison
WHERE mac_address != '00:11:22:33:44:55:66:77'::MACADDR8
ORDER BY id;

-- Ordering
SELECT id, mac_address FROM test_macaddr8_comparison ORDER BY mac_address;

-- Range query
SELECT id, mac_address FROM test_macaddr8_comparison
WHERE mac_address >= '00:00:00:00:00:00:01:00'::MACADDR8
  AND mac_address <= 'aa:bb:cc:dd:ee:ff:00:11'::MACADDR8
ORDER BY mac_address;

-- ============================================================================
-- Section 6: MACADDR8 Operators
-- ============================================================================

-- Bitwise AND
SELECT '00:11:22:33:44:55:66:77'::MACADDR8 & 'ff:ff:ff:00:00:00:00:00'::MACADDR8 AS oui_mask;

-- Bitwise OR
SELECT '00:00:00:33:44:55:66:77'::MACADDR8 | '00:11:22:00:00:00:00:00'::MACADDR8 AS combined;

-- Bitwise NOT
SELECT ~'00:00:00:00:00:00:00:00'::MACADDR8 AS inverted;

-- ============================================================================
-- Section 7: Real-world Use Case - IPv6 Link-Local Address
-- ============================================================================

CREATE TABLE test_macaddr8_ipv6_linklocal (
    device_id INT PRIMARY KEY,
    device_name VARCHAR(100),
    mac_address MACADDR8,
    ipv6_linklocal INET
);

-- IPv6 link-local addresses derived from EUI-64
INSERT INTO test_macaddr8_ipv6_linklocal VALUES
    (1, 'server01', '00:11:22:ff:fe:33:44:55',
        'fe80::211:22ff:fe33:4455'),
    (2, 'router01', 'ac:de:48:ff:fe:11:22:33',
        'fe80::aede:48ff:fe11:2233'),
    (3, 'workstation05', '08:00:27:ff:fe:ab:cd:ef',
        'fe80::a00:27ff:feab:cdef');

SELECT device_id, device_name, mac_address, ipv6_linklocal
FROM test_macaddr8_ipv6_linklocal ORDER BY device_id;

-- Note: The IPv6 link-local address is derived by:
-- 1. Taking EUI-64 MAC address
-- 2. Inverting bit 6 of first byte (universal/local bit)
-- 3. Prepending fe80::

-- ============================================================================
-- Section 8: Real-world Use Case - Modern Device Inventory
-- ============================================================================

CREATE TABLE test_macaddr8_inventory (
    device_id INT PRIMARY KEY,
    device_name VARCHAR(100),
    mac_address MACADDR8,
    device_type VARCHAR(50),
    protocol VARCHAR(50),
    location VARCHAR(100)
);

CREATE INDEX idx_mac8_address ON test_macaddr8_inventory (mac_address);

INSERT INTO test_macaddr8_inventory VALUES
    (1, 'sensor01', '00:12:4b:00:01:23:45:67', 'IoT Sensor', 'IEEE 802.15.4', 'Building A Floor 1'),
    (2, 'sensor02', '00:12:4b:00:01:23:45:68', 'IoT Sensor', 'IEEE 802.15.4', 'Building A Floor 2'),
    (3, 'gateway01', 'ac:de:48:ff:fe:11:22:33', 'Gateway', 'Ethernet', 'Server Room'),
    (4, 'zigbee-coord', '00:15:8d:00:01:ab:cd:ef', 'Zigbee Coordinator', 'Zigbee', 'Building B'),
    (5, 'thread-border', '18:b4:30:ff:fe:aa:bb:cc', 'Thread Border Router', 'Thread', 'Network Closet');

-- Find device by MAC
SELECT device_name, device_type, location FROM test_macaddr8_inventory
WHERE mac_address = '00:12:4b:00:01:23:45:67'::MACADDR8;

-- List devices by protocol
SELECT device_name, mac_address FROM test_macaddr8_inventory
WHERE protocol = 'IEEE 802.15.4'
ORDER BY device_name;

-- Count devices by type
SELECT device_type, COUNT(*) AS count FROM test_macaddr8_inventory
GROUP BY device_type ORDER BY count DESC;

-- ============================================================================
-- Section 9: Real-world Use Case - IoT Device Registry
-- ============================================================================

CREATE TABLE test_macaddr8_iot (
    device_id INT PRIMARY KEY,
    mac_address MACADDR8,
    device_model VARCHAR(100),
    firmware_version VARCHAR(50),
    last_seen TIMESTAMP,
    battery_level INT
);

INSERT INTO test_macaddr8_iot VALUES
    (1, '00:12:4b:00:01:23:45:67', 'TempSensor-X100', '2.1.5', '2024-01-15 10:00:00', 85),
    (2, '00:12:4b:00:01:23:45:68', 'TempSensor-X100', '2.1.5', '2024-01-15 10:05:00', 72),
    (3, '00:15:8d:00:01:ab:cd:ef', 'MotionDetect-M50', '1.0.3', '2024-01-15 10:10:00', 92),
    (4, '18:b4:30:ff:fe:aa:bb:cc', 'SmartLock-L200', '3.2.1', '2024-01-15 10:15:00', 45);

-- Find low battery devices
SELECT device_id, mac_address, device_model, battery_level
FROM test_macaddr8_iot
WHERE battery_level < 50
ORDER BY battery_level;

-- Recently seen devices
SELECT mac_address, device_model, last_seen
FROM test_macaddr8_iot
WHERE last_seen > CURRENT_TIMESTAMP - INTERVAL '1 hour'
ORDER BY last_seen DESC;

-- Devices by firmware version
SELECT firmware_version, COUNT(*) AS device_count
FROM test_macaddr8_iot
GROUP BY firmware_version
ORDER BY firmware_version;

-- ============================================================================
-- Section 10: Real-world Use Case - Zigbee/Thread Network
-- ============================================================================

CREATE TABLE test_macaddr8_mesh_network (
    node_id INT PRIMARY KEY,
    mac_address MACADDR8,
    node_type VARCHAR(50),
    parent_mac MACADDR8,
    network_role VARCHAR(50),
    signal_strength INT
);

INSERT INTO test_macaddr8_mesh_network VALUES
    (1, '00:15:8d:00:01:00:00:01', 'Coordinator', NULL, 'Network Coordinator', NULL),
    (2, '00:15:8d:00:01:00:00:02', 'Router', '00:15:8d:00:01:00:00:01'::MACADDR8, 'Router', -45),
    (3, '00:15:8d:00:01:00:00:03', 'End Device', '00:15:8d:00:01:00:00:02'::MACADDR8, 'End Device', -55),
    (4, '00:15:8d:00:01:00:00:04', 'End Device', '00:15:8d:00:01:00:00:02'::MACADDR8, 'End Device', -60),
    (5, '00:15:8d:00:01:00:00:05', 'Router', '00:15:8d:00:01:00:00:01'::MACADDR8, 'Router', -50);

-- Find all children of a router
SELECT node_id, mac_address, node_type, signal_strength
FROM test_macaddr8_mesh_network
WHERE parent_mac = '00:15:8d:00:01:00:00:02'::MACADDR8
ORDER BY signal_strength DESC;

-- Network topology
SELECT n.mac_address AS node,
       n.node_type,
       p.mac_address AS parent,
       n.signal_strength
FROM test_macaddr8_mesh_network n
LEFT JOIN test_macaddr8_mesh_network p ON n.parent_mac = p.mac_address
ORDER BY n.node_id;

-- Weak signal devices
SELECT mac_address, node_type, signal_strength
FROM test_macaddr8_mesh_network
WHERE signal_strength < -55
ORDER BY signal_strength;

-- ============================================================================
-- Section 11: OUI-64 (Extended OUI for EUI-64)
-- ============================================================================

CREATE TABLE test_macaddr8_oui64 (
    oui_prefix VARCHAR(11) PRIMARY KEY,
    manufacturer VARCHAR(200),
    address_type VARCHAR(50)
);

INSERT INTO test_macaddr8_oui64 VALUES
    ('00:12:4b', 'Texas Instruments', 'IEEE 802.15.4'),
    ('00:15:8d', 'Silicon Labs', 'Zigbee'),
    ('18:b4:30', 'Google/Nest', 'Thread'),
    ('ac:de:48', 'Dell Inc.', 'Ethernet'),
    ('08:00:27', 'Oracle VirtualBox', 'Virtual');

-- Join with inventory
SELECT i.device_name, i.mac_address, o.manufacturer, o.address_type
FROM test_macaddr8_inventory i
LEFT JOIN test_macaddr8_oui64 o
    ON substring(i.mac_address::TEXT, 1, 8) = o.oui_prefix
ORDER BY i.device_id;

-- Count devices by manufacturer
SELECT o.manufacturer, COUNT(*) AS device_count
FROM test_macaddr8_inventory i
LEFT JOIN test_macaddr8_oui64 o
    ON substring(i.mac_address::TEXT, 1, 8) = o.oui_prefix
GROUP BY o.manufacturer
ORDER BY device_count DESC;

-- ============================================================================
-- Section 12: MACADDR vs MACADDR8 Comparison
-- ============================================================================

CREATE TABLE test_macaddr8_comparison_types (
    id INT PRIMARY KEY,
    mac6 MACADDR,
    mac8 MACADDR8,
    description VARCHAR(200)
);

INSERT INTO test_macaddr8_comparison_types VALUES
    (1, '00:11:22:33:44:55', '00:11:22:ff:fe:33:44:55', 'Same device, different formats'),
    (2, 'ac:de:48:11:22:33', 'ac:de:48:ff:fe:11:22:33', 'Converted EUI-48 to EUI-64');

SELECT id, mac6, mac8, description FROM test_macaddr8_comparison_types ORDER BY id;

-- Convert MACADDR to MACADDR8
SELECT id, mac6,
       macaddr8_set7bit(mac6) AS converted_to_mac8
FROM test_macaddr8_comparison_types ORDER BY id;

-- ============================================================================
-- Section 13: NULL Handling
-- ============================================================================

CREATE TABLE test_macaddr8_nulls (
    id INT PRIMARY KEY,
    mac_address MACADDR8
);

INSERT INTO test_macaddr8_nulls VALUES
    (1, '00:11:22:33:44:55:66:77'),
    (2, NULL),
    (3, 'aa:bb:cc:dd:ee:ff:00:11');

SELECT id, mac_address, mac_address IS NULL AS is_null FROM test_macaddr8_nulls ORDER BY id;

-- ============================================================================
-- Section 14: Address Type Classification
-- ============================================================================

CREATE TABLE test_macaddr8_classification (
    id INT PRIMARY KEY,
    mac_address MACADDR8,
    description VARCHAR(200)
);

INSERT INTO test_macaddr8_classification VALUES
    (1, '00:11:22:33:44:55:66:77', 'Globally unique unicast'),
    (2, '02:00:00:00:00:00:00:01', 'Locally administered unicast'),
    (3, '01:00:5e:00:00:00:00:00', 'Multicast'),
    (4, 'ff:ff:ff:ff:ff:ff:ff:ff', 'Broadcast');

-- Classify addresses
SELECT id, mac_address,
       CASE
           WHEN get_byte(mac_address::BYTEA, 0) & 1 = 1 THEN 'Multicast/Broadcast'
           ELSE 'Unicast'
       END AS cast_type,
       CASE
           WHEN get_byte(mac_address::BYTEA, 0) & 2 = 2 THEN 'Locally Administered'
           ELSE 'Globally Unique'
       END AS admin_type
FROM test_macaddr8_classification ORDER BY id;

-- ============================================================================
-- Section 15: Performance Considerations
-- ============================================================================

CREATE TABLE test_macaddr8_performance (
    id INT PRIMARY KEY,
    note TEXT
);

INSERT INTO test_macaddr8_performance VALUES
    (1, 'MACADDR8 is 8 bytes vs MACADDR 6 bytes (33% larger)'),
    (2, 'Use MACADDR8 for EUI-64 addresses (IPv6, IEEE 802.15.4)'),
    (3, 'Use MACADDR for traditional Ethernet (EUI-48)'),
    (4, 'B-tree indexes work well for equality and range queries'),
    (5, 'Comparison and sorting are byte-wise'),
    (6, 'Bitwise operators (&, |, ~) available'),
    (7, 'Convert MACADDR to MACADDR8 with macaddr8_set7bit()'),
    (8, 'IPv6 link-local addresses derived from EUI-64');

SELECT id, note FROM test_macaddr8_performance ORDER BY id;

-- ============================================================================
-- Section 16: Best Practices
-- ============================================================================

CREATE TABLE test_macaddr8_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_macaddr8_best_practices VALUES
    (1, 'Use MACADDR8 for 8-byte MAC addresses (EUI-64)'),
    (2, 'Use MACADDR for 6-byte MAC addresses (EUI-48)'),
    (3, 'MACADDR8 required for: IPv6 link-local, Zigbee, Thread, 802.15.4'),
    (4, 'Output format: XX:XX:XX:XX:XX:XX:XX:XX'),
    (5, 'First 3 bytes identify manufacturer (OUI)'),
    (6, 'FF:FE in middle indicates conversion from EUI-48'),
    (7, 'Use macaddr8_set7bit() to convert MACADDR to MACADDR8'),
    (8, 'Bit 0 of first byte: 0=unicast, 1=multicast'),
    (9, 'Bit 1 of first byte: 0=globally unique, 1=locally administered'),
    (10, 'Create indexes for device lookups'),
    (11, 'Ideal for: IoT devices, mesh networks, modern protocols'),
    (12, 'IPv6 link-local derived by inverting bit 6 of first byte'),
    (13, 'Supports same comparison operators as MACADDR'),
    (14, 'Bitwise operations useful for address manipulation'),
    (15, 'Store manufacturer info separately for efficient lookups');

SELECT id, guideline FROM test_macaddr8_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_macaddr8_best_practices;
DROP TABLE test_macaddr8_performance;
DROP TABLE test_macaddr8_classification;
DROP TABLE test_macaddr8_nulls;
DROP TABLE test_macaddr8_comparison_types;
DROP TABLE test_macaddr8_oui64;
DROP TABLE test_macaddr8_mesh_network;
DROP TABLE test_macaddr8_iot;
DROP TABLE test_macaddr8_inventory;
DROP TABLE test_macaddr8_ipv6_linklocal;
DROP TABLE test_macaddr8_comparison;
DROP TABLE test_macaddr8_functions;
DROP TABLE test_macaddr8_conversion;
DROP TABLE test_macaddr8_formats;
DROP TABLE test_macaddr8_basic;

DROP DATABASE test_macaddr8_db;

-- End of MACADDR8 data type tests
