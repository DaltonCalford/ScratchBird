/*
 * ScratchBird v0.5.0 - Deep Hierarchical Schema Operations
 * 
 * This example demonstrates ScratchBird's industry-leading 8-level
 * hierarchical schema support, exceeding PostgreSQL's capabilities.
 * 
 * Features demonstrated:
 * - 8-level deep schema nesting
 * - Complex schema path navigation
 * - Schema inheritance patterns
 * - Enterprise-grade schema organization
 * - Performance considerations
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

-- Enable SQL Dialect 4 for full hierarchical schema support
SET SQL DIALECT 4;

-- ================================================================
-- Enterprise Schema Hierarchy Example
-- ================================================================

-- Level 1: Company
CREATE SCHEMA company;

-- Level 2: Divisions
CREATE SCHEMA company.finance;
CREATE SCHEMA company.operations;
CREATE SCHEMA company.technology;

-- Level 3: Departments
CREATE SCHEMA company.finance.accounting;
CREATE SCHEMA company.finance.treasury;
CREATE SCHEMA company.operations.manufacturing;
CREATE SCHEMA company.operations.logistics;
CREATE SCHEMA company.technology.development;
CREATE SCHEMA company.technology.infrastructure;

-- Level 4: Teams
CREATE SCHEMA company.finance.accounting.payroll;
CREATE SCHEMA company.finance.accounting.reporting;
CREATE SCHEMA company.technology.development.backend;
CREATE SCHEMA company.technology.development.frontend;
CREATE SCHEMA company.technology.infrastructure.security;

-- Level 5: Projects
CREATE SCHEMA company.technology.development.backend.api;
CREATE SCHEMA company.technology.development.backend.database;
CREATE SCHEMA company.technology.development.frontend.web;
CREATE SCHEMA company.technology.development.frontend.mobile;

-- Level 6: Environments
CREATE SCHEMA company.technology.development.backend.api.production;
CREATE SCHEMA company.technology.development.backend.api.staging;
CREATE SCHEMA company.technology.development.backend.api.testing;
CREATE SCHEMA company.technology.development.backend.database.production;
CREATE SCHEMA company.technology.development.backend.database.staging;

-- Level 7: Components
CREATE SCHEMA company.technology.development.backend.api.production.auth;
CREATE SCHEMA company.technology.development.backend.api.production.billing;
CREATE SCHEMA company.technology.development.backend.database.production.core;
CREATE SCHEMA company.technology.development.backend.database.production.analytics;

-- Level 8: Modules (Maximum depth)
CREATE SCHEMA company.technology.development.backend.api.production.auth.oauth;
CREATE SCHEMA company.technology.development.backend.api.production.auth.session;
CREATE SCHEMA company.technology.development.backend.api.production.billing.invoicing;
CREATE SCHEMA company.technology.development.backend.database.production.core.users;

-- ================================================================
-- Schema Hierarchy Visualization
-- ================================================================

-- Display the complete schema hierarchy
SELECT 
    RDB$SCHEMA_NAME,
    RDB$PARENT_SCHEMA_NAME,
    RDB$SCHEMA_LEVEL,
    RDB$SCHEMA_PATH,
    LPAD('', (RDB$SCHEMA_LEVEL - 1) * 2, ' ') || 
    SUBSTRING(RDB$SCHEMA_NAME FROM POSITION('.' IN REVERSE(RDB$SCHEMA_NAME || '.')) + 1) as HIERARCHY_DISPLAY
FROM RDB$SCHEMAS
WHERE RDB$SCHEMA_PATH LIKE 'company%'
ORDER BY RDB$SCHEMA_PATH;

-- ================================================================
-- Complex Schema Navigation
-- ================================================================

-- Navigate to deep schema level
SET SCHEMA 'company.technology.development.backend.api.production.auth';
SHOW SCHEMA;

-- Create table in deep schema
CREATE TABLE auth_tokens (
    token_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    user_id UUID NOT NULL,
    token_hash VARCHAR(255) NOT NULL,
    expires_at TIMESTAMP NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_used_at TIMESTAMP,
    ip_address INET,
    user_agent VARCHAR(500)
);

-- Insert sample data
INSERT INTO auth_tokens (user_id, token_hash, expires_at, ip_address, user_agent)
VALUES 
    (GEN_UUID(7), 'hash123', CURRENT_TIMESTAMP + INTERVAL 1 HOUR, '192.168.1.100', 'Mozilla/5.0 Chrome'),
    (GEN_UUID(7), 'hash456', CURRENT_TIMESTAMP + INTERVAL 2 HOUR, '10.0.0.50', 'Mozilla/5.0 Firefox'),
    (GEN_UUID(7), 'hash789', CURRENT_TIMESTAMP + INTERVAL 30 MINUTE, '172.16.0.25', 'Mobile App v1.0');

-- Navigate to maximum depth schema
SET SCHEMA 'company.technology.development.backend.api.production.auth.oauth';
SHOW SCHEMA;

-- Create table at maximum depth
CREATE TABLE oauth_clients (
    client_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    client_secret VARCHAR(255) NOT NULL,
    client_name VARCHAR(100) NOT NULL,
    redirect_uris TEXT,
    scopes VARCHAR(500),
    is_confidential BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert OAuth client data
INSERT INTO oauth_clients (client_secret, client_name, redirect_uris, scopes)
VALUES 
    ('secret123', 'Web Application', 'https://app.example.com/callback', 'read write admin'),
    ('secret456', 'Mobile App', 'myapp://oauth/callback', 'read write'),
    ('secret789', 'Third Party Integration', 'https://partner.example.com/callback', 'read');

-- ================================================================
-- Cross-Level Schema Queries
-- ================================================================

-- Query across multiple schema levels
SELECT 
    'Level 8 (OAuth)' as SCHEMA_LEVEL,
    client_name,
    created_at
FROM company.technology.development.backend.api.production.auth.oauth.oauth_clients
UNION ALL
SELECT 
    'Level 7 (Auth)' as SCHEMA_LEVEL,
    'Token for user: ' || SUBSTRING(CAST(user_id AS VARCHAR(36)) FROM 1 FOR 8),
    created_at
FROM company.technology.development.backend.api.production.auth.auth_tokens
ORDER BY created_at;

-- ================================================================
-- Schema Path Analysis
-- ================================================================

-- Analyze schema path lengths and depths
SELECT 
    RDB$SCHEMA_LEVEL,
    COUNT(*) as SCHEMA_COUNT,
    AVG(CHARACTER_LENGTH(RDB$SCHEMA_PATH)) as AVG_PATH_LENGTH,
    MAX(CHARACTER_LENGTH(RDB$SCHEMA_PATH)) as MAX_PATH_LENGTH
FROM RDB$SCHEMAS
WHERE RDB$SCHEMA_PATH LIKE 'company%'
GROUP BY RDB$SCHEMA_LEVEL
ORDER BY RDB$SCHEMA_LEVEL;

-- Find schemas at maximum depth
SELECT 
    RDB$SCHEMA_NAME,
    RDB$SCHEMA_PATH,
    CHARACTER_LENGTH(RDB$SCHEMA_PATH) as PATH_LENGTH
FROM RDB$SCHEMAS
WHERE RDB$SCHEMA_LEVEL = 8
ORDER BY CHARACTER_LENGTH(RDB$SCHEMA_PATH) DESC;

-- ================================================================
-- Schema Inheritance Patterns
-- ================================================================

-- Create a view that shows schema hierarchy relationships
CREATE VIEW schema_hierarchy_view AS
SELECT 
    child.RDB$SCHEMA_NAME as CHILD_SCHEMA,
    child.RDB$PARENT_SCHEMA_NAME as PARENT_SCHEMA,
    child.RDB$SCHEMA_LEVEL as CHILD_LEVEL,
    parent.RDB$SCHEMA_LEVEL as PARENT_LEVEL,
    child.RDB$SCHEMA_PATH as FULL_PATH
FROM RDB$SCHEMAS child
LEFT JOIN RDB$SCHEMAS parent ON parent.RDB$SCHEMA_NAME = child.RDB$PARENT_SCHEMA_NAME
WHERE child.RDB$SCHEMA_PATH LIKE 'company%'
ORDER BY child.RDB$SCHEMA_LEVEL, child.RDB$SCHEMA_NAME;

-- Query the hierarchy view
SELECT * FROM schema_hierarchy_view;

-- ================================================================
-- Performance Considerations
-- ================================================================

-- Demonstrate schema-specific queries for performance
SET SCHEMA 'company.technology.development.backend.api.production';

-- Create index on frequently queried columns
CREATE INDEX idx_auth_tokens_user_id 
ON auth.auth_tokens(user_id);

CREATE INDEX idx_oauth_clients_name 
ON auth.oauth.oauth_clients(client_name);

-- Efficient query using schema context
SELECT 
    at.token_id,
    at.user_id,
    at.expires_at,
    oc.client_name
FROM auth.auth_tokens at
JOIN auth.oauth.oauth_clients oc ON at.user_id = oc.client_id
WHERE at.expires_at > CURRENT_TIMESTAMP;

-- ================================================================
-- Schema Administration at Scale
-- ================================================================

-- Count objects in each schema level
SELECT 
    s.RDB$SCHEMA_LEVEL,
    COUNT(DISTINCT s.RDB$SCHEMA_NAME) as SCHEMA_COUNT,
    COUNT(r.RDB$RELATION_NAME) as TABLE_COUNT,
    COUNT(i.RDB$INDEX_NAME) as INDEX_COUNT
FROM RDB$SCHEMAS s
LEFT JOIN RDB$RELATIONS r ON r.RDB$SCHEMA_NAME = s.RDB$SCHEMA_NAME
LEFT JOIN RDB$INDICES i ON i.RDB$SCHEMA_NAME = s.RDB$SCHEMA_NAME
WHERE s.RDB$SCHEMA_PATH LIKE 'company%'
GROUP BY s.RDB$SCHEMA_LEVEL
ORDER BY s.RDB$SCHEMA_LEVEL;

-- Find schemas with most objects
SELECT 
    s.RDB$SCHEMA_NAME,
    s.RDB$SCHEMA_LEVEL,
    COUNT(r.RDB$RELATION_NAME) as OBJECT_COUNT
FROM RDB$SCHEMAS s
LEFT JOIN RDB$RELATIONS r ON r.RDB$SCHEMA_NAME = s.RDB$SCHEMA_NAME
WHERE s.RDB$SCHEMA_PATH LIKE 'company%'
GROUP BY s.RDB$SCHEMA_NAME, s.RDB$SCHEMA_LEVEL
HAVING COUNT(r.RDB$RELATION_NAME) > 0
ORDER BY OBJECT_COUNT DESC;

-- ================================================================
-- Schema Path Validation
-- ================================================================

-- Validate schema path integrity
SELECT 
    RDB$SCHEMA_NAME,
    RDB$SCHEMA_PATH,
    CASE 
        WHEN RDB$SCHEMA_LEVEL = 1 AND RDB$PARENT_SCHEMA_NAME IS NULL THEN 'Valid Root'
        WHEN RDB$SCHEMA_LEVEL > 1 AND RDB$PARENT_SCHEMA_NAME IS NOT NULL THEN 'Valid Child'
        ELSE 'Invalid Structure'
    END as VALIDATION_STATUS
FROM RDB$SCHEMAS
WHERE RDB$SCHEMA_PATH LIKE 'company%'
ORDER BY RDB$SCHEMA_LEVEL, RDB$SCHEMA_NAME;

-- Check for orphaned schemas
SELECT 
    child.RDB$SCHEMA_NAME,
    child.RDB$PARENT_SCHEMA_NAME,
    'Orphaned Schema' as ISSUE
FROM RDB$SCHEMAS child
LEFT JOIN RDB$SCHEMAS parent ON parent.RDB$SCHEMA_NAME = child.RDB$PARENT_SCHEMA_NAME
WHERE child.RDB$PARENT_SCHEMA_NAME IS NOT NULL
  AND parent.RDB$SCHEMA_NAME IS NULL
  AND child.RDB$SCHEMA_PATH LIKE 'company%';

-- ================================================================
-- Cleanup (Optional)
-- ================================================================

-- Note: Uncomment to clean up the example
-- WARNING: This will drop all tables and schemas created in this example

-- Drop tables first
-- DROP TABLE company.technology.development.backend.api.production.auth.oauth.oauth_clients;
-- DROP TABLE company.technology.development.backend.api.production.auth.auth_tokens;
-- DROP VIEW schema_hierarchy_view;

-- Drop schemas from deepest to shallowest level
-- DROP SCHEMA company.technology.development.backend.api.production.auth.oauth;
-- DROP SCHEMA company.technology.development.backend.api.production.auth.session;
-- DROP SCHEMA company.technology.development.backend.api.production.billing.invoicing;
-- DROP SCHEMA company.technology.development.backend.database.production.core.users;
-- DROP SCHEMA company.technology.development.backend.api.production.auth;
-- DROP SCHEMA company.technology.development.backend.api.production.billing;
-- DROP SCHEMA company.technology.development.backend.database.production.core;
-- DROP SCHEMA company.technology.development.backend.database.production.analytics;
-- DROP SCHEMA company.technology.development.backend.api.production;
-- DROP SCHEMA company.technology.development.backend.api.staging;
-- DROP SCHEMA company.technology.development.backend.api.testing;
-- DROP SCHEMA company.technology.development.backend.database.production;
-- DROP SCHEMA company.technology.development.backend.database.staging;
-- DROP SCHEMA company.technology.development.backend.api;
-- DROP SCHEMA company.technology.development.backend.database;
-- DROP SCHEMA company.technology.development.frontend.web;
-- DROP SCHEMA company.technology.development.frontend.mobile;
-- DROP SCHEMA company.finance.accounting.payroll;
-- DROP SCHEMA company.finance.accounting.reporting;
-- DROP SCHEMA company.technology.development.backend;
-- DROP SCHEMA company.technology.development.frontend;
-- DROP SCHEMA company.technology.infrastructure.security;
-- DROP SCHEMA company.finance.accounting;
-- DROP SCHEMA company.finance.treasury;
-- DROP SCHEMA company.operations.manufacturing;
-- DROP SCHEMA company.operations.logistics;
-- DROP SCHEMA company.technology.development;
-- DROP SCHEMA company.technology.infrastructure;
-- DROP SCHEMA company.finance;
-- DROP SCHEMA company.operations;
-- DROP SCHEMA company.technology;
-- DROP SCHEMA company;

-- ================================================================
-- Summary
-- ================================================================

-- This example demonstrated:
-- 1. 8-level deep hierarchical schema creation (exceeds PostgreSQL)
-- 2. Complex enterprise schema organization patterns
-- 3. Schema path navigation and analysis
-- 4. Cross-level schema queries and joins
-- 5. Performance considerations for deep hierarchies
-- 6. Schema administration at scale
-- 7. Schema validation and integrity checking
-- 8. Maximum schema depth and path length limits

-- Key Benefits:
-- - Exceeds PostgreSQL's schema capabilities (PostgreSQL has no nesting)
-- - 511-character path length accommodates complex hierarchies
-- - Enterprise-grade organization with 8-level depth
-- - High-performance caching for schema resolution
-- - Administrative tools for managing complex hierarchies

-- Next steps:
-- - See schema_management.sql for advanced administration
-- - See schema_navigation.sql for navigation patterns
-- - See database_links examples for distributed schema access