#!/bin/bash

#
# ScratchBird v0.5.0 - Network Data Types Tests
#
# This script tests PostgreSQL-compatible network data types in ScratchBird,
# including INET, CIDR, MACADDR, UUID, and related operators and functions.
#
# Test categories:
# 1. INET data type operations
# 2. CIDR data type operations
# 3. MACADDR data type operations
# 4. UUID data type operations
# 5. Network data type operators
# 6. Network data type functions
# 7. Network data type indexing
# 8. Network data type performance
#
# Copyright (c) 2025 ScratchBird Development Team
# All Rights Reserved.
#

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DB_DIR="$SCRIPT_DIR/test_databases"
TEST_DB="$TEST_DB_DIR/network_datatypes_test.fdb"
SB_ISQL_PATH="../gen/Release/scratchbird/bin/sb_isql"
OUTPUT_FILE="$SCRIPT_DIR/test_results.txt"

# Create test database directory
mkdir -p "$TEST_DB_DIR"

# Function to log test results
log_test_result() {
    local test_name="$1"
    local expected_result="$2"
    local command="$3"
    local start_time="$4"
    local end_time="$5"
    
    local elapsed_time=$(echo "$end_time - $start_time" | bc -l)
    
    echo "=========================================" >> "$OUTPUT_FILE"
    echo "Test: $test_name" >> "$OUTPUT_FILE"
    echo "Expected: $expected_result" >> "$OUTPUT_FILE"
    echo "Command: $command" >> "$OUTPUT_FILE"
    echo "Execution time: ${elapsed_time}s" >> "$OUTPUT_FILE"
    echo "=========================================" >> "$OUTPUT_FILE"
}

# Function to run sb_isql with test file
run_isql_test() {
    local test_file="$1"
    local output_file="$2"
    local start_time=$(date +%s.%N)
    
    # Set SCRATCHBIRD environment variable
    export SCRATCHBIRD="$SCRIPT_DIR/../gen/Release/scratchbird"
    
    # Run sb_isql with the test file
    "$SB_ISQL_PATH" -i "$test_file" -o "$output_file" 2>&1
    local exit_code=$?
    
    local end_time=$(date +%s.%N)
    echo "$start_time $end_time $exit_code"
}

# Test 1: INET Data Type Operations
echo "Testing INET data type operations..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/inet_test.sql" << 'EOF'
/* INET data type operations test */
CREATE DATABASE 'test_databases/network_datatypes_test.fdb';
CONNECT 'test_databases/network_datatypes_test.fdb';

/* Create schema for network testing */
CREATE SCHEMA network;
CREATE SCHEMA network.monitoring;

/* Create table with INET columns */
CREATE TABLE network.monitoring.server_addresses (
    server_id INTEGER PRIMARY KEY,
    server_name VARCHAR(100),
    ipv4_address INET,
    ipv6_address INET,
    management_ip INET,
    created_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Insert IPv4 addresses */
INSERT INTO network.monitoring.server_addresses (server_id, server_name, ipv4_address, management_ip) VALUES
(1, 'web-server-01', '192.168.1.100', '10.0.0.100'),
(2, 'db-server-01', '192.168.1.101', '10.0.0.101'),
(3, 'app-server-01', '192.168.1.102', '10.0.0.102');

/* Insert IPv6 addresses */
INSERT INTO network.monitoring.server_addresses (server_id, server_name, ipv6_address) VALUES
(4, 'web-server-02', '2001:db8::1'),
(5, 'db-server-02', '2001:db8::2'),
(6, 'app-server-02', '2001:db8::3');

/* Insert mixed IPv4/IPv6 addresses */
INSERT INTO network.monitoring.server_addresses (server_id, server_name, ipv4_address, ipv6_address) VALUES
(7, 'proxy-server-01', '203.0.113.10', '2001:db8::10');

/* Query all addresses */
SELECT server_name, ipv4_address, ipv6_address, management_ip 
FROM network.monitoring.server_addresses
ORDER BY server_id;

/* Test INET functions */
SELECT 
    server_name,
    ipv4_address,
    NETWORK(ipv4_address) AS network_addr,
    BROADCAST(ipv4_address) AS broadcast_addr,
    NETMASK(ipv4_address) AS netmask_addr
FROM network.monitoring.server_addresses 
WHERE ipv4_address IS NOT NULL;

/* Test INET comparisons */
SELECT server_name, ipv4_address 
FROM network.monitoring.server_addresses 
WHERE ipv4_address > '192.168.1.100'
ORDER BY ipv4_address;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/inet_test.sql" "$TEST_DB_DIR/inet_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "INET Data Type" "Successfully create and query INET columns with IPv4/IPv6 addresses" \
    "INSERT INTO network.monitoring.server_addresses (ipv4_address) VALUES ('192.168.1.100')" "$start_time" "$end_time"

cat "$TEST_DB_DIR/inet_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 2: CIDR Data Type Operations
echo "Testing CIDR data type operations..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/cidr_test.sql" << 'EOF'
/* CIDR data type operations test */
CONNECT 'test_databases/network_datatypes_test.fdb';

/* Create table with CIDR columns */
CREATE TABLE network.monitoring.network_segments (
    segment_id INTEGER PRIMARY KEY,
    segment_name VARCHAR(100),
    network_cidr CIDR,
    backup_cidr CIDR,
    description VARCHAR(200)
);

/* Insert CIDR network ranges */
INSERT INTO network.monitoring.network_segments (segment_id, segment_name, network_cidr, description) VALUES
(1, 'DMZ Network', '203.0.113.0/24', 'Public facing servers'),
(2, 'Internal Network', '192.168.1.0/24', 'Internal corporate network'),
(3, 'Management Network', '10.0.0.0/8', 'Network management'),
(4, 'Guest Network', '172.16.0.0/16', 'Guest access network');

/* Insert IPv6 CIDR ranges */
INSERT INTO network.monitoring.network_segments (segment_id, segment_name, network_cidr, description) VALUES
(5, 'IPv6 Main', '2001:db8::/32', 'Main IPv6 network'),
(6, 'IPv6 Test', '2001:db8:1::/48', 'IPv6 testing network');

/* Query all network segments */
SELECT segment_name, network_cidr, description 
FROM network.monitoring.network_segments
ORDER BY segment_id;

/* Test CIDR functions */
SELECT 
    segment_name,
    network_cidr,
    NETWORK(network_cidr) AS network_addr,
    BROADCAST(network_cidr) AS broadcast_addr,
    MASKLEN(network_cidr) AS mask_length
FROM network.monitoring.network_segments 
WHERE network_cidr IS NOT NULL;

/* Test CIDR containment operators */
SELECT 
    s.segment_name,
    s.network_cidr,
    a.server_name,
    a.ipv4_address
FROM network.monitoring.network_segments s
JOIN network.monitoring.server_addresses a ON a.ipv4_address << s.network_cidr
ORDER BY s.segment_id;

/* Test CIDR overlap operations */
SELECT 
    segment_name,
    network_cidr,
    CASE 
        WHEN network_cidr && '192.168.0.0/16' THEN 'Overlaps with 192.168.0.0/16'
        ELSE 'No overlap with 192.168.0.0/16'
    END AS overlap_check
FROM network.monitoring.network_segments;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/cidr_test.sql" "$TEST_DB_DIR/cidr_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "CIDR Data Type" "Successfully create and query CIDR columns with network ranges" \
    "INSERT INTO network.monitoring.network_segments (network_cidr) VALUES ('203.0.113.0/24')" "$start_time" "$end_time"

cat "$TEST_DB_DIR/cidr_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 3: MACADDR Data Type Operations
echo "Testing MACADDR data type operations..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/macaddr_test.sql" << 'EOF'
/* MACADDR data type operations test */
CONNECT 'test_databases/network_datatypes_test.fdb';

/* Create table with MACADDR columns */
CREATE TABLE network.monitoring.network_interfaces (
    interface_id INTEGER PRIMARY KEY,
    server_id INTEGER,
    interface_name VARCHAR(50),
    mac_address MACADDR,
    backup_mac MACADDR,
    interface_type VARCHAR(20),
    status VARCHAR(20)
);

/* Insert MAC addresses in different formats */
INSERT INTO network.monitoring.network_interfaces (interface_id, server_id, interface_name, mac_address, interface_type, status) VALUES
(1, 1, 'eth0', '00:11:22:33:44:55', 'Ethernet', 'Active'),
(2, 1, 'eth1', '00-11-22-33-44-56', 'Ethernet', 'Active'),
(3, 2, 'eth0', '001122334457', 'Ethernet', 'Active'),
(4, 3, 'wlan0', '00:11:22:33:44:58', 'WiFi', 'Active'),
(5, 4, 'eth0', '00:11:22:33:44:59', 'Ethernet', 'Inactive');

/* Insert additional MAC addresses for testing */
INSERT INTO network.monitoring.network_interfaces (interface_id, server_id, interface_name, mac_address, interface_type, status) VALUES
(6, 5, 'eth0', 'AA:BB:CC:DD:EE:FF', 'Ethernet', 'Active'),
(7, 6, 'eth1', 'AA:BB:CC:DD:EE:FE', 'Ethernet', 'Active'),
(8, 7, 'bond0', 'AA:BB:CC:DD:EE:FD', 'Bond', 'Active');

/* Query all network interfaces */
SELECT interface_name, mac_address, interface_type, status 
FROM network.monitoring.network_interfaces
ORDER BY interface_id;

/* Test MACADDR functions */
SELECT 
    interface_name,
    mac_address,
    TRUNC(mac_address) AS oui_prefix,
    mac_address - TRUNC(mac_address) AS device_portion
FROM network.monitoring.network_interfaces;

/* Test MACADDR comparisons and sorting */
SELECT interface_name, mac_address 
FROM network.monitoring.network_interfaces 
ORDER BY mac_address;

/* Test MACADDR arithmetic operations */
SELECT 
    interface_name,
    mac_address,
    mac_address + 1 AS next_mac,
    mac_address - 1 AS prev_mac
FROM network.monitoring.network_interfaces
WHERE interface_id <= 3;

/* Join with server addresses */
SELECT 
    sa.server_name,
    ni.interface_name,
    ni.mac_address,
    sa.ipv4_address
FROM network.monitoring.network_interfaces ni
JOIN network.monitoring.server_addresses sa ON ni.server_id = sa.server_id
WHERE ni.status = 'Active'
ORDER BY sa.server_name, ni.interface_name;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/macaddr_test.sql" "$TEST_DB_DIR/macaddr_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "MACADDR Data Type" "Successfully create and query MACADDR columns with MAC addresses" \
    "INSERT INTO network.monitoring.network_interfaces (mac_address) VALUES ('00:11:22:33:44:55')" "$start_time" "$end_time"

cat "$TEST_DB_DIR/macaddr_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 4: UUID Data Type Operations
echo "Testing UUID data type operations..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/uuid_test.sql" << 'EOF'
/* UUID data type operations test */
CONNECT 'test_databases/network_datatypes_test.fdb';

/* Create table with UUID columns */
CREATE TABLE network.monitoring.monitoring_sessions (
    session_id UUID PRIMARY KEY,
    server_id INTEGER,
    session_name VARCHAR(100),
    backup_session_id UUID,
    start_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    end_time TIMESTAMP,
    session_data VARCHAR(500)
);

/* Insert UUIDs in different formats */
INSERT INTO network.monitoring.monitoring_sessions (session_id, server_id, session_name, session_data) VALUES
('550e8400-e29b-41d4-a716-446655440000', 1, 'Web Server Monitor', 'Monitoring web server performance'),
('550e8400-e29b-41d4-a716-446655440001', 2, 'DB Server Monitor', 'Monitoring database server performance'),
('550e8400-e29b-41d4-a716-446655440002', 3, 'App Server Monitor', 'Monitoring application server performance');

/* Generate UUIDs using functions */
INSERT INTO network.monitoring.monitoring_sessions (session_id, server_id, session_name, session_data) VALUES
(UUID_GENERATE_V4(), 4, 'Dynamic Session 1', 'Generated with UUID_GENERATE_V4()'),
(UUID_GENERATE_V4(), 5, 'Dynamic Session 2', 'Generated with UUID_GENERATE_V4()'),
(UUID_GENERATE_V4(), 6, 'Dynamic Session 3', 'Generated with UUID_GENERATE_V4()');

/* Query all monitoring sessions */
SELECT session_name, session_id, server_id, session_data 
FROM network.monitoring.monitoring_sessions
ORDER BY session_name;

/* Test UUID functions */
SELECT 
    session_name,
    session_id,
    UUID_TO_STRING(session_id) AS uuid_string,
    UUID_VERSION(session_id) AS uuid_version
FROM network.monitoring.monitoring_sessions;

/* Test UUID comparisons */
SELECT session_name, session_id 
FROM network.monitoring.monitoring_sessions 
ORDER BY session_id;

/* Test UUID indexing performance */
SELECT COUNT(*) AS total_sessions 
FROM network.monitoring.monitoring_sessions;

SELECT session_name, session_id 
FROM network.monitoring.monitoring_sessions 
WHERE session_id = '550e8400-e29b-41d4-a716-446655440001';

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/uuid_test.sql" "$TEST_DB_DIR/uuid_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "UUID Data Type" "Successfully create and query UUID columns with various UUID formats" \
    "INSERT INTO network.monitoring.monitoring_sessions (session_id) VALUES ('550e8400-e29b-41d4-a716-446655440000')" "$start_time" "$end_time"

cat "$TEST_DB_DIR/uuid_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 5: Network Data Type Operators
echo "Testing network data type operators..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/network_operators_test.sql" << 'EOF'
/* Network data type operators test */
CONNECT 'test_databases/network_datatypes_test.fdb';

/* Create comprehensive test table */
CREATE TABLE network.monitoring.network_analysis (
    analysis_id INTEGER PRIMARY KEY,
    source_ip INET,
    dest_ip INET,
    source_network CIDR,
    dest_network CIDR,
    analysis_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Insert test data for operator testing */
INSERT INTO network.monitoring.network_analysis (analysis_id, source_ip, dest_ip, source_network, dest_network) VALUES
(1, '192.168.1.100', '203.0.113.10', '192.168.1.0/24', '203.0.113.0/24'),
(2, '192.168.1.101', '203.0.113.11', '192.168.1.0/24', '203.0.113.0/24'),
(3, '10.0.0.50', '192.168.1.200', '10.0.0.0/8', '192.168.1.0/24'),
(4, '172.16.0.100', '10.0.0.100', '172.16.0.0/16', '10.0.0.0/8');

/* Test containment operators */
SELECT 
    analysis_id,
    source_ip,
    source_network,
    CASE WHEN source_ip << source_network THEN 'IP is contained in network' ELSE 'IP is NOT contained in network' END AS containment_check
FROM network.monitoring.network_analysis;

/* Test network overlap operators */
SELECT 
    analysis_id,
    source_network,
    dest_network,
    CASE WHEN source_network && dest_network THEN 'Networks overlap' ELSE 'Networks do not overlap' END AS overlap_check
FROM network.monitoring.network_analysis;

/* Test network containment operators */
SELECT 
    analysis_id,
    source_network,
    dest_network,
    CASE 
        WHEN source_network >> dest_network THEN 'Source contains destination'
        WHEN dest_network >> source_network THEN 'Destination contains source'
        ELSE 'No containment relationship'
    END AS network_containment
FROM network.monitoring.network_analysis;

/* Test equality and comparison operators */
SELECT 
    analysis_id,
    source_ip,
    dest_ip,
    CASE 
        WHEN source_ip = dest_ip THEN 'Same IP'
        WHEN source_ip < dest_ip THEN 'Source < Destination'
        ELSE 'Source > Destination'
    END AS ip_comparison
FROM network.monitoring.network_analysis;

/* Test network arithmetic */
SELECT 
    analysis_id,
    source_ip,
    source_ip + 1 AS next_ip,
    source_ip - 1 AS prev_ip
FROM network.monitoring.network_analysis
WHERE analysis_id <= 2;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/network_operators_test.sql" "$TEST_DB_DIR/network_operators_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Network Operators" "Successfully use network data type operators (<<, >>, &&, +, -)" \
    "SELECT * FROM network.monitoring.network_analysis WHERE source_ip << source_network" "$start_time" "$end_time"

cat "$TEST_DB_DIR/network_operators_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 6: Network Data Type Performance
echo "Testing network data type performance..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/network_performance_test.sql" << 'EOF'
/* Network data type performance test */
CONNECT 'test_databases/network_datatypes_test.fdb';

/* Create performance test table */
CREATE TABLE network.monitoring.performance_data (
    record_id INTEGER PRIMARY KEY,
    test_inet INET,
    test_cidr CIDR,
    test_macaddr MACADDR,
    test_uuid UUID,
    test_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Create indexes for performance testing */
CREATE INDEX idx_perf_inet ON network.monitoring.performance_data (test_inet);
CREATE INDEX idx_perf_cidr ON network.monitoring.performance_data (test_cidr);
CREATE INDEX idx_perf_macaddr ON network.monitoring.performance_data (test_macaddr);
CREATE INDEX idx_perf_uuid ON network.monitoring.performance_data (test_uuid);

/* Insert performance test data */
INSERT INTO network.monitoring.performance_data (record_id, test_inet, test_cidr, test_macaddr, test_uuid) VALUES
(1, '192.168.1.1', '192.168.1.0/24', '00:11:22:33:44:01', '550e8400-e29b-41d4-a716-446655440010'),
(2, '192.168.1.2', '192.168.1.0/24', '00:11:22:33:44:02', '550e8400-e29b-41d4-a716-446655440011'),
(3, '192.168.1.3', '192.168.1.0/24', '00:11:22:33:44:03', '550e8400-e29b-41d4-a716-446655440012'),
(4, '192.168.1.4', '192.168.1.0/24', '00:11:22:33:44:04', '550e8400-e29b-41d4-a716-446655440013'),
(5, '192.168.1.5', '192.168.1.0/24', '00:11:22:33:44:05', '550e8400-e29b-41d4-a716-446655440014');

/* Performance test queries */
SELECT COUNT(*) AS total_records FROM network.monitoring.performance_data;

SELECT * FROM network.monitoring.performance_data WHERE test_inet = '192.168.1.3';

SELECT * FROM network.monitoring.performance_data WHERE test_macaddr = '00:11:22:33:44:03';

SELECT * FROM network.monitoring.performance_data WHERE test_uuid = '550e8400-e29b-41d4-a716-446655440012';

/* Test range queries */
SELECT * FROM network.monitoring.performance_data 
WHERE test_inet BETWEEN '192.168.1.2' AND '192.168.1.4'
ORDER BY test_inet;

/* Test network containment performance */
SELECT * FROM network.monitoring.performance_data 
WHERE test_inet << '192.168.1.0/24';

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/network_performance_test.sql" "$TEST_DB_DIR/network_performance_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Network Performance" "Successfully perform indexed queries on network data types" \
    "SELECT * FROM network.monitoring.performance_data WHERE test_inet = '192.168.1.3'" "$start_time" "$end_time"

cat "$TEST_DB_DIR/network_performance_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Summary
echo "Network Data Types Tests Completed" >> "$OUTPUT_FILE"
echo "Test database: $TEST_DB" >> "$OUTPUT_FILE"
echo "Test files created in: $TEST_DB_DIR" >> "$OUTPUT_FILE"
echo "=====================================." >> "$OUTPUT_FILE"

echo "Network data type tests completed successfully"
exit 0