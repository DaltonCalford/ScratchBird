/*
 * ScratchBird v0.5.0 - UDR Installation Script
 * 
 * This script installs the ScratchBird UDR (User Defined Routines)
 * functions, procedures, and triggers into a ScratchBird database.
 * 
 * Prerequisites:
 * - libScratchBirdUDR.so must be compiled and placed in the plugins directory
 * - Database must be created with SQL Dialect 4
 * - User must have administrative privileges
 * 
 * The contents of this file are subject to the Initial
 * Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at
 * http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 * 
 * Copyright (c) 2025 ScratchBird Development Team
 * All Rights Reserved.
 */

-- Enable SQL Dialect 4 for full feature support
SET SQL DIALECT 4;

-- Create schema for UDR examples
CREATE SCHEMA IF NOT EXISTS udr_examples;
SET SCHEMA 'udr_examples';

-- ================================================================
-- UDR Functions Installation
-- ================================================================

-- Function: Generate hierarchical schema path
CREATE FUNCTION generate_schema_path(
    schema_name VARCHAR(63),
    level INTEGER,
    parent_name VARCHAR(63)
)
RETURNS VARCHAR(511)
EXTERNAL NAME 'libScratchBirdUDR!generate_schema_path'
ENGINE UDR;

-- Function: Validate network address
CREATE FUNCTION validate_network_address(
    ip_address VARCHAR(45),
    address_type VARCHAR(20)
)
RETURNS (
    is_valid BOOLEAN,
    validation_message VARCHAR(100)
)
EXTERNAL NAME 'libScratchBirdUDR!validate_network_address'
ENGINE UDR;

-- Function: Calculate subnet information
CREATE FUNCTION calculate_subnet_info(
    cidr_notation VARCHAR(18)
)
RETURNS (
    network_address VARCHAR(15),
    broadcast_address VARCHAR(15),
    host_count INTEGER,
    prefix_length INTEGER
)
EXTERNAL NAME 'libScratchBirdUDR!calculate_subnet_info'
ENGINE UDR;

-- Function: JSON path extraction
CREATE FUNCTION json_extract_path(
    json_data VARCHAR(8191),
    json_path VARCHAR(100)
)
RETURNS VARCHAR(1000)
EXTERNAL NAME 'libScratchBirdUDR!json_extract_path'
ENGINE UDR;

-- ================================================================
-- UDR Procedures Installation
-- ================================================================

-- Procedure: Create hierarchical schema structure
CREATE PROCEDURE create_schema_hierarchy(
    base_schema_path VARCHAR(511),
    max_levels INTEGER
)
RETURNS (
    created_schema VARCHAR(511),
    schema_level INTEGER,
    status_message VARCHAR(100)
)
EXTERNAL NAME 'libScratchBirdUDR!create_schema_hierarchy'
ENGINE UDR;

-- Procedure: Network diagnostics
CREATE PROCEDURE network_diagnostics(
    target_ip VARCHAR(45),
    diagnostic_type VARCHAR(20)
)
RETURNS (
    diagnostic_result VARCHAR(100),
    test_timestamp TIMESTAMP,
    response_time_ms INTEGER
)
EXTERNAL NAME 'libScratchBirdUDR!network_diagnostics'
ENGINE UDR;

-- ================================================================
-- Test Tables for UDR Examples
-- ================================================================

-- Table for testing schema functions
CREATE TABLE schema_test_data (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    base_schema VARCHAR(63),
    schema_level INTEGER,
    parent_schema VARCHAR(63),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Table for testing network functions
CREATE TABLE network_test_data (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    ip_address INET,
    mac_address MACADDR,
    network_cidr CIDR,
    description VARCHAR(200),
    last_updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Table for testing JSON functions
CREATE TABLE json_test_data (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    json_content VARCHAR(8191),
    document_type VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Audit table for schema changes
CREATE TABLE schema_audit_log (
    audit_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    schema_name VARCHAR(511),
    action_type VARCHAR(20),
    old_value VARCHAR(511),
    new_value VARCHAR(511),
    action_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    action_user VARCHAR(50) DEFAULT USER
);

-- ================================================================
-- Sample Data for Testing
-- ================================================================

-- Insert test data for schema functions
INSERT INTO schema_test_data (base_schema, schema_level, parent_schema) VALUES
    ('company', 1, NULL),
    ('finance', 2, 'company'),
    ('accounting', 3, 'company.finance'),
    ('reports', 4, 'company.finance.accounting'),
    ('monthly', 5, 'company.finance.accounting.reports');

-- Insert test data for network functions
INSERT INTO network_test_data (ip_address, mac_address, network_cidr, description) VALUES
    ('192.168.1.100', '00:11:22:33:44:55', '192.168.1.0/24', 'Web server'),
    ('10.0.0.50', '00:aa:bb:cc:dd:ee', '10.0.0.0/8', 'Database server'),
    ('172.16.0.25', 'aa:bb:cc:dd:ee:ff', '172.16.0.0/16', 'Application server'),
    ('2001:db8::1', '11:22:33:44:55:66', '2001:db8::/32', 'IPv6 server');

-- Insert test data for JSON functions
INSERT INTO json_test_data (json_content, document_type) VALUES
    ('{"name": "John Doe", "age": 30, "city": "New York", "active": true}', 'user_profile'),
    ('{"product": "Widget", "price": 29.99, "inventory": 150, "category": "electronics"}', 'product_info'),
    ('{"order_id": "12345", "items": [{"name": "Item1", "qty": 2}, {"name": "Item2", "qty": 1}]}', 'order_data'),
    ('{"server": "web-01", "status": "running", "cpu": 45.2, "memory": 78.5}', 'monitoring_data');

-- ================================================================
-- UDR Function Usage Examples
-- ================================================================

-- Example 1: Generate schema paths
SELECT 
    s.base_schema,
    s.schema_level,
    s.parent_schema,
    generate_schema_path(s.base_schema, s.schema_level, s.parent_schema) as generated_path
FROM schema_test_data s
ORDER BY s.schema_level;

-- Example 2: Validate network addresses
SELECT 
    n.ip_address,
    n.description,
    v.is_valid,
    v.validation_message
FROM network_test_data n
CROSS JOIN validate_network_address(CAST(n.ip_address AS VARCHAR(45)), 'IPv4') v;

-- Example 3: Calculate subnet information
SELECT 
    n.network_cidr,
    n.description,
    s.network_address,
    s.broadcast_address,
    s.host_count,
    s.prefix_length
FROM network_test_data n
CROSS JOIN calculate_subnet_info(CAST(n.network_cidr AS VARCHAR(18))) s
WHERE n.network_cidr IS NOT NULL;

-- Example 4: Extract JSON data
SELECT 
    j.document_type,
    j.json_content,
    json_extract_path(j.json_content, 'name') as extracted_name,
    json_extract_path(j.json_content, 'price') as extracted_price,
    json_extract_path(j.json_content, 'active') as extracted_active
FROM json_test_data j;

-- ================================================================
-- UDR Procedure Usage Examples
-- ================================================================

-- Example 1: Create schema hierarchy
SELECT 
    created_schema,
    schema_level,
    status_message
FROM create_schema_hierarchy('test_hierarchy', 4);

-- Example 2: Network diagnostics
SELECT 
    diagnostic_result,
    test_timestamp,
    response_time_ms
FROM network_diagnostics('192.168.1.100', 'ALL');

-- ================================================================
-- UDR Trigger Installation
-- ================================================================

-- Note: Triggers would be created on existing tables
-- For demonstration, we'll create triggers on our test tables

-- Create trigger for schema audit
CREATE TRIGGER schema_audit_trigger FOR schema_test_data
ACTIVE BEFORE INSERT OR UPDATE OR DELETE POSITION 0
EXTERNAL NAME 'libScratchBirdUDR!schema_audit_trigger'
ENGINE UDR;

-- Create trigger for network data validation
CREATE TRIGGER network_validation_trigger FOR network_test_data
ACTIVE BEFORE INSERT OR UPDATE POSITION 0
EXTERNAL NAME 'libScratchBirdUDR!network_data_validation_trigger'
ENGINE UDR;

-- ================================================================
-- Advanced UDR Usage Examples
-- ================================================================

-- Combined example: Schema management with validation
WITH schema_hierarchy AS (
    SELECT 
        created_schema,
        schema_level,
        status_message
    FROM create_schema_hierarchy('production', 6)
)
SELECT 
    sh.created_schema,
    sh.schema_level,
    generate_schema_path(
        SUBSTRING(sh.created_schema FROM POSITION('.' IN REVERSE(sh.created_schema || '.')) + 1),
        sh.schema_level,
        SUBSTRING(sh.created_schema FROM 1 FOR LENGTH(sh.created_schema) - POSITION('.' IN REVERSE(sh.created_schema || '.')) - 1)
    ) as validated_path
FROM schema_hierarchy sh;

-- Network analysis with UDR functions
SELECT 
    n.ip_address,
    n.network_cidr,
    n.description,
    v.is_valid as ip_valid,
    v.validation_message,
    s.network_address,
    s.host_count
FROM network_test_data n
CROSS JOIN validate_network_address(CAST(n.ip_address AS VARCHAR(45)), 'IPv4') v
CROSS JOIN calculate_subnet_info(CAST(n.network_cidr AS VARCHAR(18))) s
WHERE n.network_cidr IS NOT NULL;

-- JSON data processing pipeline
SELECT 
    j.document_type,
    json_extract_path(j.json_content, 'name') as name,
    json_extract_path(j.json_content, 'price') as price,
    json_extract_path(j.json_content, 'active') as active_status,
    CASE 
        WHEN json_extract_path(j.json_content, 'active') = 'true' THEN 'Active'
        WHEN json_extract_path(j.json_content, 'active') = 'false' THEN 'Inactive'
        ELSE 'Unknown'
    END as status_description
FROM json_test_data j
WHERE j.document_type IN ('user_profile', 'product_info');

-- ================================================================
-- UDR Performance Testing
-- ================================================================

-- Test UDR function performance
SELECT 
    COUNT(*) as total_validations,
    AVG(CASE WHEN v.is_valid THEN 1 ELSE 0 END) as validation_success_rate
FROM network_test_data n
CROSS JOIN validate_network_address(CAST(n.ip_address AS VARCHAR(45)), 'IPv4') v;

-- Test UDR procedure performance
SELECT 
    COUNT(*) as total_schemas_created,
    AVG(schema_level) as avg_schema_level
FROM create_schema_hierarchy('performance_test', 8);

-- ================================================================
-- Cleanup (Optional)
-- ================================================================

-- Note: Uncomment to remove UDR functions, procedures, and triggers

-- Drop triggers
-- DROP TRIGGER schema_audit_trigger;
-- DROP TRIGGER network_validation_trigger;

-- Drop procedures
-- DROP PROCEDURE create_schema_hierarchy;
-- DROP PROCEDURE network_diagnostics;

-- Drop functions
-- DROP FUNCTION generate_schema_path;
-- DROP FUNCTION validate_network_address;
-- DROP FUNCTION calculate_subnet_info;
-- DROP FUNCTION json_extract_path;

-- Drop test tables
-- DROP TABLE schema_audit_log;
-- DROP TABLE json_test_data;
-- DROP TABLE network_test_data;
-- DROP TABLE schema_test_data;

-- ================================================================
-- Summary
-- ================================================================

-- This installation script demonstrated:
-- 1. UDR function installation and usage
-- 2. UDR procedure installation and usage
-- 3. UDR trigger installation and configuration
-- 4. Advanced UDR usage patterns
-- 5. Performance testing approaches
-- 6. Integration with hierarchical schemas
-- 7. PostgreSQL-compatible data type support

-- Key Benefits:
-- - Extend ScratchBird with custom business logic
-- - Integrate with hierarchical schema system
-- - Support for PostgreSQL-compatible data types
-- - Modern C++ development environment
-- - High-performance native code execution
-- - Seamless integration with SQL queries

-- Next steps:
-- - Develop custom UDR functions for specific business needs
-- - Create UDR procedures for complex data processing
-- - Implement UDR triggers for advanced data validation
-- - Build UDR libraries for specialized functionality

COMMIT;