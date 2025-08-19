/*
 * ScratchBird v0.5.0 - Network Data Types Example
 * 
 * This example demonstrates ScratchBird's PostgreSQL-compatible
 * network data types: INET, CIDR, and MACADDR.
 * 
 * Features demonstrated:
 * - INET type for IP addresses (IPv4 and IPv6)
 * - CIDR type for network ranges
 * - MACADDR type for MAC addresses
 * - Network operators and functions
 * - Indexing and performance considerations
 * 
 * The contents of this file are subject to the Initial
 * Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at
 * http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 * 
 * Software distributed under the License is distributed AS IS,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the License for the specific language governing rights
 * and limitations under the License.
 * 
 * The Original Code was created by ScratchBird Development Team
 * for the ScratchBird Open Source RDBMS project.
 * 
 * Copyright (c) 2025 ScratchBird Development Team
 * and all contributors signed below.
 * 
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

-- Enable SQL Dialect 4 for advanced data types
SET SQL DIALECT 4;

-- Create schema for network examples
CREATE SCHEMA network_examples;
SET SCHEMA 'network_examples';

-- ================================================================
-- INET Type - IP Address Storage
-- ================================================================

-- Create table with INET columns
CREATE TABLE servers (
    server_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    hostname VARCHAR(100) NOT NULL,
    ip_address INET NOT NULL,
    backup_ip INET,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert IPv4 addresses
INSERT INTO servers (hostname, ip_address, backup_ip) VALUES
    ('web-server-01', '192.168.1.100', '192.168.1.101'),
    ('web-server-02', '192.168.1.102', '192.168.1.103'),
    ('db-server-01', '10.0.0.50', '10.0.0.51'),
    ('db-server-02', '10.0.0.52', '10.0.0.53'),
    ('app-server-01', '172.16.0.25', '172.16.0.26');

-- Insert IPv6 addresses
INSERT INTO servers (hostname, ip_address, backup_ip) VALUES
    ('ipv6-server-01', '2001:db8::1', '2001:db8::2'),
    ('ipv6-server-02', '2001:db8:85a3::8a2e:370:7334', '2001:db8:85a3::8a2e:370:7335'),
    ('dual-stack-01', '192.168.1.200', '2001:db8::100');

-- Insert IP addresses with CIDR notation
INSERT INTO servers (hostname, ip_address) VALUES
    ('subnet-gateway', '192.168.1.1/24'),
    ('network-monitor', '10.0.0.1/8');

-- Query IPv4 addresses
SELECT hostname, ip_address 
FROM servers 
WHERE ip_address << '192.168.1.0/24';

-- Query IPv6 addresses
SELECT hostname, ip_address 
FROM servers 
WHERE ip_address << '2001:db8::/32';

-- ================================================================
-- CIDR Type - Network Range Storage
-- ================================================================

-- Create table for network configurations
CREATE TABLE network_segments (
    segment_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    segment_name VARCHAR(100) NOT NULL,
    network_cidr CIDR NOT NULL,
    vlan_id INTEGER,
    description TEXT,
    is_active BOOLEAN DEFAULT TRUE
);

-- Insert network segments
INSERT INTO network_segments (segment_name, network_cidr, vlan_id, description) VALUES
    ('DMZ Network', '192.168.1.0/24', 10, 'Demilitarized zone for public services'),
    ('Internal Network', '10.0.0.0/8', 20, 'Private internal network'),
    ('Guest Network', '172.16.0.0/16', 30, 'Guest access network'),
    ('Management Network', '192.168.100.0/24', 40, 'Network management interfaces'),
    ('IPv6 Internal', '2001:db8::/32', 50, 'IPv6 internal network'),
    ('IoT Network', '192.168.200.0/24', 60, 'Internet of Things devices');

-- Query network segments
SELECT segment_name, network_cidr, description 
FROM network_segments 
ORDER BY segment_name;

-- Find which network segment contains a specific IP
SELECT s.segment_name, s.network_cidr, srv.hostname, srv.ip_address
FROM network_segments s
JOIN servers srv ON srv.ip_address << s.network_cidr
ORDER BY s.segment_name, srv.hostname;

-- ================================================================
-- MACADDR Type - MAC Address Storage
-- ================================================================

-- Create table for network interfaces
CREATE TABLE network_interfaces (
    interface_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    server_id UUID NOT NULL,
    interface_name VARCHAR(20) NOT NULL,
    mac_address MACADDR NOT NULL,
    ip_address INET,
    interface_type VARCHAR(20) DEFAULT 'ethernet',
    is_active BOOLEAN DEFAULT TRUE,
    speed_mbps INTEGER,
    duplex VARCHAR(10) DEFAULT 'full'
);

-- Insert network interfaces with MAC addresses
INSERT INTO network_interfaces (server_id, interface_name, mac_address, ip_address, speed_mbps) VALUES
    ((SELECT server_id FROM servers WHERE hostname = 'web-server-01'), 'eth0', '00:11:22:33:44:55', '192.168.1.100', 1000),
    ((SELECT server_id FROM servers WHERE hostname = 'web-server-01'), 'eth1', '00:11:22:33:44:56', '192.168.1.101', 1000),
    ((SELECT server_id FROM servers WHERE hostname = 'db-server-01'), 'eth0', '00:aa:bb:cc:dd:ee', '10.0.0.50', 10000),
    ((SELECT server_id FROM servers WHERE hostname = 'db-server-01'), 'eth1', '00:aa:bb:cc:dd:ef', '10.0.0.51', 10000),
    ((SELECT server_id FROM servers WHERE hostname = 'app-server-01'), 'eth0', 'aa:bb:cc:dd:ee:ff', '172.16.0.25', 1000);

-- Insert wireless interfaces
INSERT INTO network_interfaces (server_id, interface_name, mac_address, ip_address, interface_type, speed_mbps) VALUES
    ((SELECT server_id FROM servers WHERE hostname = 'web-server-02'), 'wlan0', '11:22:33:44:55:66', '192.168.1.102', 'wireless', 300),
    ((SELECT server_id FROM servers WHERE hostname = 'app-server-01'), 'wlan0', 'aa:bb:cc:11:22:33', '172.16.0.26', 'wireless', 150);

-- Query network interfaces
SELECT 
    s.hostname,
    ni.interface_name,
    ni.mac_address,
    ni.ip_address,
    ni.interface_type,
    ni.speed_mbps
FROM servers s
JOIN network_interfaces ni ON s.server_id = ni.server_id
ORDER BY s.hostname, ni.interface_name;

-- ================================================================
-- Network Operators and Functions
-- ================================================================

-- Network containment operators
SELECT 
    'IP in Network' as operation,
    srv.hostname,
    srv.ip_address,
    ns.segment_name,
    ns.network_cidr
FROM servers srv
JOIN network_segments ns ON srv.ip_address << ns.network_cidr
ORDER BY srv.hostname;

-- Network equality and comparison
SELECT 
    segment_name,
    network_cidr,
    CASE 
        WHEN network_cidr = '192.168.1.0/24' THEN 'Exact Match'
        WHEN network_cidr >> '192.168.1.0/24' THEN 'Contains Network'
        WHEN network_cidr << '192.168.1.0/24' THEN 'Contained By Network'
        ELSE 'No Relation'
    END as relationship
FROM network_segments;

-- Host address extraction
SELECT 
    hostname,
    ip_address,
    HOST(ip_address) as host_address,
    MASKLEN(ip_address) as netmask_length
FROM servers
WHERE ip_address IS NOT NULL;

-- Network address functions
SELECT 
    segment_name,
    network_cidr,
    NETWORK(network_cidr) as network_address,
    BROADCAST(network_cidr) as broadcast_address,
    MASKLEN(network_cidr) as prefix_length
FROM network_segments
ORDER BY segment_name;

-- ================================================================
-- Advanced Network Queries
-- ================================================================

-- Find servers in the same network segment
SELECT 
    s1.hostname as server1,
    s2.hostname as server2,
    s1.ip_address as ip1,
    s2.ip_address as ip2,
    ns.segment_name
FROM servers s1
JOIN servers s2 ON s1.server_id < s2.server_id
JOIN network_segments ns ON s1.ip_address << ns.network_cidr 
                          AND s2.ip_address << ns.network_cidr
ORDER BY ns.segment_name, s1.hostname;

-- Network utilization analysis
SELECT 
    ns.segment_name,
    ns.network_cidr,
    COUNT(s.server_id) as servers_count,
    -- Calculate theoretical capacity (simplified)
    CASE 
        WHEN MASKLEN(ns.network_cidr) = 24 THEN 254
        WHEN MASKLEN(ns.network_cidr) = 16 THEN 65534
        WHEN MASKLEN(ns.network_cidr) = 8 THEN 16777214
        ELSE 0
    END as theoretical_capacity
FROM network_segments ns
LEFT JOIN servers s ON s.ip_address << ns.network_cidr
GROUP BY ns.segment_name, ns.network_cidr
ORDER BY servers_count DESC;

-- MAC address vendor analysis (simplified)
SELECT 
    SUBSTRING(mac_address FROM 1 FOR 8) as oui_prefix,
    COUNT(*) as interface_count,
    CASE SUBSTRING(mac_address FROM 1 FOR 8)
        WHEN '00:11:22' THEN 'Vendor A'
        WHEN '00:aa:bb' THEN 'Vendor B'
        WHEN 'aa:bb:cc' THEN 'Vendor C'
        ELSE 'Unknown'
    END as vendor
FROM network_interfaces
GROUP BY SUBSTRING(mac_address FROM 1 FOR 8)
ORDER BY interface_count DESC;

-- ================================================================
-- Performance Optimization
-- ================================================================

-- Create indexes for network types
CREATE INDEX idx_servers_ip_address ON servers USING GIST (ip_address);
CREATE INDEX idx_network_segments_cidr ON network_segments USING GIST (network_cidr);
CREATE INDEX idx_network_interfaces_mac ON network_interfaces (mac_address);

-- Demonstrate index usage with EXPLAIN
-- (Note: EXPLAIN syntax may vary, this is conceptual)
SELECT 
    s.hostname,
    s.ip_address,
    ns.segment_name
FROM servers s
JOIN network_segments ns ON s.ip_address << ns.network_cidr
WHERE s.ip_address << '192.168.0.0/16';

-- ================================================================
-- Network Security Analysis
-- ================================================================

-- Create table for security rules
CREATE TABLE firewall_rules (
    rule_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    rule_name VARCHAR(100) NOT NULL,
    source_network CIDR,
    destination_network CIDR,
    action VARCHAR(20) DEFAULT 'ALLOW',
    protocol VARCHAR(10) DEFAULT 'TCP',
    port_range VARCHAR(20),
    is_active BOOLEAN DEFAULT TRUE
);

-- Insert firewall rules
INSERT INTO firewall_rules (rule_name, source_network, destination_network, action, protocol, port_range) VALUES
    ('Allow DMZ to Internal HTTP', '192.168.1.0/24', '10.0.0.0/8', 'ALLOW', 'TCP', '80,443'),
    ('Block Guest to Internal', '172.16.0.0/16', '10.0.0.0/8', 'DENY', 'ANY', 'ANY'),
    ('Allow Management Access', '192.168.100.0/24', '0.0.0.0/0', 'ALLOW', 'TCP', '22,23,443'),
    ('Allow Internal to DMZ', '10.0.0.0/8', '192.168.1.0/24', 'ALLOW', 'TCP', '80,443,993');

-- Security analysis: Find servers that would be affected by firewall rules
SELECT 
    fr.rule_name,
    fr.action,
    s.hostname,
    s.ip_address,
    fr.source_network,
    fr.destination_network
FROM firewall_rules fr
JOIN servers s ON s.ip_address << fr.source_network 
                OR s.ip_address << fr.destination_network
WHERE fr.is_active = TRUE
ORDER BY fr.rule_name, s.hostname;

-- ================================================================
-- IPv6 Specific Operations
-- ================================================================

-- Create table for IPv6 specific configurations
CREATE TABLE ipv6_configurations (
    config_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    server_id UUID NOT NULL,
    ipv6_address INET NOT NULL,
    prefix_length INTEGER NOT NULL,
    address_type VARCHAR(20) DEFAULT 'global',
    is_temporary BOOLEAN DEFAULT FALSE
);

-- Insert IPv6 configurations
INSERT INTO ipv6_configurations (server_id, ipv6_address, prefix_length, address_type) VALUES
    ((SELECT server_id FROM servers WHERE hostname = 'ipv6-server-01'), '2001:db8::1', 64, 'global'),
    ((SELECT server_id FROM servers WHERE hostname = 'ipv6-server-01'), 'fe80::1', 64, 'link-local'),
    ((SELECT server_id FROM servers WHERE hostname = 'ipv6-server-02'), '2001:db8:85a3::8a2e:370:7334', 64, 'global'),
    ((SELECT server_id FROM servers WHERE hostname = 'dual-stack-01'), '2001:db8::100', 64, 'global');

-- IPv6 address analysis
SELECT 
    s.hostname,
    i6.ipv6_address,
    i6.prefix_length,
    i6.address_type,
    CASE 
        WHEN i6.ipv6_address << '2001:db8::/32' THEN 'Documentation Range'
        WHEN i6.ipv6_address << 'fe80::/10' THEN 'Link-Local'
        WHEN i6.ipv6_address << 'fc00::/7' THEN 'Unique Local'
        ELSE 'Global Unicast'
    END as address_scope
FROM servers s
JOIN ipv6_configurations i6 ON s.server_id = i6.server_id
ORDER BY s.hostname;

-- ================================================================
-- Cleanup (Optional)
-- ================================================================

-- Note: Uncomment to clean up the example

-- DROP TABLE ipv6_configurations;
-- DROP TABLE firewall_rules;
-- DROP TABLE network_interfaces;
-- DROP TABLE network_segments;
-- DROP TABLE servers;
-- DROP SCHEMA network_examples;

-- ================================================================
-- Summary
-- ================================================================

-- This example demonstrated:
-- 1. INET type for storing IPv4 and IPv6 addresses
-- 2. CIDR type for network ranges and subnets
-- 3. MACADDR type for MAC address storage
-- 4. Network containment operators (<< and >>)
-- 5. Network functions (HOST, NETWORK, BROADCAST, MASKLEN)
-- 6. Advanced network queries and analysis
-- 7. Performance optimization with GIST indexes
-- 8. Security analysis with firewall rules
-- 9. IPv6 specific operations and configurations

-- Key Benefits:
-- - Full PostgreSQL compatibility for network types
-- - Efficient storage and indexing of network data
-- - Rich set of network operators and functions
-- - Support for both IPv4 and IPv6 addressing
-- - Advanced network analysis capabilities

-- Next steps:
-- - See uuid_types.sql for UUID data type examples
-- - See unsigned_integers.sql for extended numeric types
-- - See range_types.sql for range data type operations