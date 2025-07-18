/*
 * ScratchBird v0.5.0 - Schema Management Package
 * 
 * This package provides comprehensive schema management functionality
 * for ScratchBird's hierarchical schema system.
 * 
 * Features demonstrated:
 * - Package-based code organization
 * - Hierarchical schema administration
 * - Schema validation and integrity checking
 * - Schema navigation utilities
 * - Schema metadata management
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

-- Enable SQL Dialect 4 for full package support
SET SQL DIALECT 4;

-- ================================================================
-- Package Header: Schema Management
-- ================================================================

CREATE PACKAGE schema_management AS
BEGIN
    -- Package version and information
    CONSTANT PACKAGE_VERSION VARCHAR(10) = '1.0.0';
    CONSTANT PACKAGE_DESCRIPTION VARCHAR(200) = 'ScratchBird Hierarchical Schema Management Package';
    CONSTANT MAX_SCHEMA_DEPTH INTEGER = 8;
    CONSTANT MAX_SCHEMA_PATH_LENGTH INTEGER = 511;
    
    -- Exception declarations
    EXCEPTION INVALID_SCHEMA_PATH 'Invalid schema path format';
    EXCEPTION SCHEMA_DEPTH_EXCEEDED 'Schema depth exceeds maximum limit of 8 levels';
    EXCEPTION SCHEMA_NOT_FOUND 'Schema not found';
    EXCEPTION CIRCULAR_REFERENCE 'Circular reference detected in schema hierarchy';
    EXCEPTION ORPHANED_SCHEMA 'Orphaned schema detected';
    
    -- Type declarations
    TYPE schema_info_type AS (
        schema_name VARCHAR(511),
        parent_schema VARCHAR(511),
        schema_level INTEGER,
        schema_path VARCHAR(511),
        object_count INTEGER,
        created_at TIMESTAMP,
        is_valid BOOLEAN
    );
    
    -- Function declarations
    FUNCTION get_package_version() RETURNS VARCHAR(10);
    FUNCTION validate_schema_path(schema_path VARCHAR(511)) RETURNS BOOLEAN;
    FUNCTION calculate_schema_level(schema_path VARCHAR(511)) RETURNS INTEGER;
    FUNCTION get_parent_schema(schema_path VARCHAR(511)) RETURNS VARCHAR(511);
    FUNCTION get_schema_root(schema_path VARCHAR(511)) RETURNS VARCHAR(511);
    FUNCTION build_schema_path(parent_path VARCHAR(511), schema_name VARCHAR(63)) RETURNS VARCHAR(511);
    FUNCTION schema_exists(schema_path VARCHAR(511)) RETURNS BOOLEAN;
    FUNCTION get_schema_children(parent_schema VARCHAR(511)) RETURNS INTEGER;
    FUNCTION get_schema_depth(schema_path VARCHAR(511)) RETURNS INTEGER;
    FUNCTION normalize_schema_path(schema_path VARCHAR(511)) RETURNS VARCHAR(511);
    
    -- Procedure declarations
    PROCEDURE create_schema_hierarchy(base_schema VARCHAR(511), hierarchy_definition VARCHAR(2000));
    PROCEDURE validate_schema_integrity();
    PROCEDURE cleanup_orphaned_schemas();
    PROCEDURE get_schema_tree(root_schema VARCHAR(511))
        RETURNS (schema_name VARCHAR(511), level INTEGER, path VARCHAR(511), parent VARCHAR(511));
    PROCEDURE get_schema_statistics()
        RETURNS (level INTEGER, schema_count INTEGER, avg_path_length NUMERIC(10,2));
    PROCEDURE migrate_schema(old_path VARCHAR(511), new_path VARCHAR(511));
    PROCEDURE backup_schema_metadata(backup_name VARCHAR(100));
    PROCEDURE restore_schema_metadata(backup_name VARCHAR(100));
    
    -- Administrative procedures
    PROCEDURE rebuild_schema_cache();
    PROCEDURE analyze_schema_performance();
    PROCEDURE optimize_schema_structure();
    PROCEDURE generate_schema_report(format_type VARCHAR(20) DEFAULT 'TEXT');
END;

-- ================================================================
-- Package Body: Schema Management Implementation
-- ================================================================

CREATE PACKAGE BODY schema_management AS
BEGIN
    -- Private variables
    VARIABLE last_validation_timestamp TIMESTAMP;
    VARIABLE cache_refresh_interval INTEGER = 3600; -- 1 hour
    
    -- ================================================================
    -- Utility Functions
    -- ================================================================
    
    FUNCTION get_package_version() RETURNS VARCHAR(10) AS
    BEGIN
        RETURN PACKAGE_VERSION;
    END
    
    FUNCTION validate_schema_path(schema_path VARCHAR(511)) RETURNS BOOLEAN AS
    BEGIN
        -- Check for null or empty path
        IF (schema_path IS NULL OR TRIM(schema_path) = '') THEN
            RETURN FALSE;
        END IF;
        
        -- Check path length
        IF (CHARACTER_LENGTH(schema_path) > MAX_SCHEMA_PATH_LENGTH) THEN
            RETURN FALSE;
        END IF;
        
        -- Check for invalid characters
        IF (schema_path CONTAINING '..') THEN
            RETURN FALSE;
        END IF;
        
        -- Check for leading/trailing dots
        IF (schema_path STARTING WITH '.' OR schema_path ENDING WITH '.') THEN
            RETURN FALSE;
        END IF;
        
        -- Check schema depth
        IF (calculate_schema_level(schema_path) > MAX_SCHEMA_DEPTH) THEN
            RETURN FALSE;
        END IF;
        
        RETURN TRUE;
    END
    
    FUNCTION calculate_schema_level(schema_path VARCHAR(511)) RETURNS INTEGER AS
    DECLARE VARIABLE level_count INTEGER;
    DECLARE VARIABLE pos INTEGER;
    DECLARE VARIABLE search_pos INTEGER;
    BEGIN
        IF (schema_path IS NULL OR TRIM(schema_path) = '') THEN
            RETURN 0;
        END IF;
        
        level_count = 1;
        search_pos = 1;
        
        WHILE (search_pos <= CHARACTER_LENGTH(schema_path)) DO
        BEGIN
            pos = POSITION('.', schema_path, search_pos);
            IF (pos = 0) THEN
                BREAK;
            END IF;
            level_count = level_count + 1;
            search_pos = pos + 1;
        END
        
        RETURN level_count;
    END
    
    FUNCTION get_parent_schema(schema_path VARCHAR(511)) RETURNS VARCHAR(511) AS
    DECLARE VARIABLE last_dot_pos INTEGER;
    BEGIN
        IF (schema_path IS NULL OR TRIM(schema_path) = '') THEN
            RETURN NULL;
        END IF;
        
        last_dot_pos = CHARACTER_LENGTH(schema_path) - POSITION('.', REVERSE(schema_path)) + 1;
        
        IF (last_dot_pos <= 1) THEN
            RETURN NULL; -- Root schema has no parent
        END IF;
        
        RETURN SUBSTRING(schema_path FROM 1 FOR last_dot_pos - 1);
    END
    
    FUNCTION get_schema_root(schema_path VARCHAR(511)) RETURNS VARCHAR(511) AS
    DECLARE VARIABLE first_dot_pos INTEGER;
    BEGIN
        IF (schema_path IS NULL OR TRIM(schema_path) = '') THEN
            RETURN NULL;
        END IF;
        
        first_dot_pos = POSITION('.', schema_path);
        
        IF (first_dot_pos = 0) THEN
            RETURN schema_path; -- Already root schema
        END IF;
        
        RETURN SUBSTRING(schema_path FROM 1 FOR first_dot_pos - 1);
    END
    
    FUNCTION build_schema_path(parent_path VARCHAR(511), schema_name VARCHAR(63)) RETURNS VARCHAR(511) AS
    DECLARE VARIABLE result_path VARCHAR(511);
    BEGIN
        IF (parent_path IS NULL OR TRIM(parent_path) = '') THEN
            result_path = schema_name;
        ELSE
            result_path = parent_path || '.' || schema_name;
        END IF;
        
        -- Validate the resulting path
        IF (NOT validate_schema_path(result_path)) THEN
            EXCEPTION INVALID_SCHEMA_PATH;
        END IF;
        
        RETURN result_path;
    END
    
    FUNCTION schema_exists(schema_path VARCHAR(511)) RETURNS BOOLEAN AS
    DECLARE VARIABLE schema_count INTEGER;
    BEGIN
        SELECT COUNT(*)
        FROM RDB$SCHEMAS
        WHERE RDB$SCHEMA_PATH = :schema_path
        INTO :schema_count;
        
        RETURN (schema_count > 0);
    END
    
    FUNCTION get_schema_children(parent_schema VARCHAR(511)) RETURNS INTEGER AS
    DECLARE VARIABLE child_count INTEGER;
    BEGIN
        SELECT COUNT(*)
        FROM RDB$SCHEMAS
        WHERE RDB$PARENT_SCHEMA_NAME = :parent_schema
        INTO :child_count;
        
        RETURN child_count;
    END
    
    FUNCTION get_schema_depth(schema_path VARCHAR(511)) RETURNS INTEGER AS
    DECLARE VARIABLE max_depth INTEGER;
    BEGIN
        WITH RECURSIVE schema_tree AS (
            SELECT 
                RDB$SCHEMA_NAME,
                RDB$SCHEMA_PATH,
                RDB$SCHEMA_LEVEL,
                1 as depth
            FROM RDB$SCHEMAS
            WHERE RDB$SCHEMA_PATH = :schema_path
            
            UNION ALL
            
            SELECT 
                s.RDB$SCHEMA_NAME,
                s.RDB$SCHEMA_PATH,
                s.RDB$SCHEMA_LEVEL,
                st.depth + 1
            FROM RDB$SCHEMAS s
            JOIN schema_tree st ON s.RDB$PARENT_SCHEMA_NAME = st.RDB$SCHEMA_NAME
        )
        SELECT MAX(depth)
        FROM schema_tree
        INTO :max_depth;
        
        RETURN COALESCE(max_depth, 0);
    END
    
    FUNCTION normalize_schema_path(schema_path VARCHAR(511)) RETURNS VARCHAR(511) AS
    DECLARE VARIABLE normalized_path VARCHAR(511);
    BEGIN
        -- Remove leading/trailing whitespace
        normalized_path = TRIM(schema_path);
        
        -- Convert to lowercase for consistency
        normalized_path = LOWER(normalized_path);
        
        -- Remove any double dots that might have been introduced
        WHILE (normalized_path CONTAINING '..') DO
        BEGIN
            normalized_path = REPLACE(normalized_path, '..', '.');
        END
        
        RETURN normalized_path;
    END
    
    -- ================================================================
    -- Schema Hierarchy Management
    -- ================================================================
    
    PROCEDURE create_schema_hierarchy(base_schema VARCHAR(511), hierarchy_definition VARCHAR(2000)) AS
    DECLARE VARIABLE schema_line VARCHAR(511);
    DECLARE VARIABLE schema_name VARCHAR(63);
    DECLARE VARIABLE parent_path VARCHAR(511);
    DECLARE VARIABLE full_path VARCHAR(511);
    DECLARE VARIABLE line_pos INTEGER;
    DECLARE VARIABLE next_line_pos INTEGER;
    DECLARE VARIABLE current_line VARCHAR(511);
    BEGIN
        -- Parse hierarchy definition (newline-separated schema names)
        line_pos = 1;
        
        WHILE (line_pos <= CHARACTER_LENGTH(hierarchy_definition)) DO
        BEGIN
            next_line_pos = POSITION(ASCII_CHAR(10), hierarchy_definition, line_pos);
            IF (next_line_pos = 0) THEN
                next_line_pos = CHARACTER_LENGTH(hierarchy_definition) + 1;
            END IF;
            
            current_line = TRIM(SUBSTRING(hierarchy_definition FROM line_pos FOR next_line_pos - line_pos));
            
            IF (current_line <> '') THEN
            BEGIN
                -- Determine parent path
                IF (current_line NOT CONTAINING '.') THEN
                    parent_path = base_schema;
                ELSE
                    parent_path = base_schema || '.' || get_parent_schema(current_line);
                END IF;
                
                -- Extract schema name
                schema_name = SUBSTRING(current_line FROM POSITION('.', REVERSE(current_line || '.')) + 1);
                
                -- Build full path
                full_path = build_schema_path(parent_path, schema_name);
                
                -- Create schema if it doesn't exist
                IF (NOT schema_exists(full_path)) THEN
                BEGIN
                    EXECUTE STATEMENT 'CREATE SCHEMA ' || full_path;
                END
            END
            
            line_pos = next_line_pos + 1;
        END
    END
    
    PROCEDURE validate_schema_integrity() AS
    DECLARE VARIABLE schema_name VARCHAR(511);
    DECLARE VARIABLE parent_name VARCHAR(511);
    DECLARE VARIABLE schema_path VARCHAR(511);
    DECLARE VARIABLE schema_level INTEGER;
    DECLARE VARIABLE issue_count INTEGER = 0;
    BEGIN
        -- Check for orphaned schemas
        FOR SELECT 
                s.RDB$SCHEMA_NAME,
                s.RDB$PARENT_SCHEMA_NAME,
                s.RDB$SCHEMA_PATH,
                s.RDB$SCHEMA_LEVEL
            FROM RDB$SCHEMAS s
            LEFT JOIN RDB$SCHEMAS p ON p.RDB$SCHEMA_NAME = s.RDB$PARENT_SCHEMA_NAME
            WHERE s.RDB$PARENT_SCHEMA_NAME IS NOT NULL
              AND p.RDB$SCHEMA_NAME IS NULL
            INTO :schema_name, :parent_name, :schema_path, :schema_level
        DO
        BEGIN
            -- Log orphaned schema
            INSERT INTO schema_validation_log (
                issue_type, 
                schema_name, 
                issue_description, 
                detected_at
            ) VALUES (
                'ORPHANED_SCHEMA',
                :schema_name,
                'Schema ' || :schema_name || ' references non-existent parent ' || :parent_name,
                CURRENT_TIMESTAMP
            );
            
            issue_count = issue_count + 1;
        END
        
        -- Check for circular references
        FOR SELECT 
                RDB$SCHEMA_NAME,
                RDB$SCHEMA_PATH
            FROM RDB$SCHEMAS
            WHERE RDB$SCHEMA_LEVEL > 1
            INTO :schema_name, :schema_path
        DO
        BEGIN
            -- Check if schema appears in its own path multiple times
            IF (schema_path CONTAINING schema_name || '.' || schema_name) THEN
            BEGIN
                INSERT INTO schema_validation_log (
                    issue_type, 
                    schema_name, 
                    issue_description, 
                    detected_at
                ) VALUES (
                    'CIRCULAR_REFERENCE',
                    :schema_name,
                    'Circular reference detected in schema path: ' || :schema_path,
                    CURRENT_TIMESTAMP
                );
                
                issue_count = issue_count + 1;
            END
        END
        
        -- Check for invalid schema paths
        FOR SELECT 
                RDB$SCHEMA_NAME,
                RDB$SCHEMA_PATH
            FROM RDB$SCHEMAS
            INTO :schema_name, :schema_path
        DO
        BEGIN
            IF (NOT validate_schema_path(schema_path)) THEN
            BEGIN
                INSERT INTO schema_validation_log (
                    issue_type, 
                    schema_name, 
                    issue_description, 
                    detected_at
                ) VALUES (
                    'INVALID_PATH',
                    :schema_name,
                    'Invalid schema path format: ' || :schema_path,
                    CURRENT_TIMESTAMP
                );
                
                issue_count = issue_count + 1;
            END
        END
        
        -- Update validation timestamp
        last_validation_timestamp = CURRENT_TIMESTAMP;
        
        -- Raise exception if critical issues found
        IF (issue_count > 0) THEN
        BEGIN
            EXCEPTION SCHEMA_VALIDATION_FAILED 'Schema integrity validation failed with ' || issue_count || ' issues';
        END
    END
    
    PROCEDURE cleanup_orphaned_schemas() AS
    DECLARE VARIABLE schema_name VARCHAR(511);
    DECLARE VARIABLE cleanup_count INTEGER = 0;
    BEGIN
        -- Find and remove orphaned schemas
        FOR SELECT s.RDB$SCHEMA_NAME
            FROM RDB$SCHEMAS s
            LEFT JOIN RDB$SCHEMAS p ON p.RDB$SCHEMA_NAME = s.RDB$PARENT_SCHEMA_NAME
            WHERE s.RDB$PARENT_SCHEMA_NAME IS NOT NULL
              AND p.RDB$SCHEMA_NAME IS NULL
            INTO :schema_name
        DO
        BEGIN
            -- Log cleanup action
            INSERT INTO schema_cleanup_log (
                action_type,
                schema_name,
                action_description,
                action_timestamp
            ) VALUES (
                'ORPHANED_CLEANUP',
                :schema_name,
                'Removed orphaned schema: ' || :schema_name,
                CURRENT_TIMESTAMP
            );
            
            -- Remove orphaned schema
            EXECUTE STATEMENT 'DROP SCHEMA ' || schema_name;
            
            cleanup_count = cleanup_count + 1;
        END
        
        -- Log summary
        INSERT INTO schema_cleanup_log (
            action_type,
            schema_name,
            action_description,
            action_timestamp
        ) VALUES (
            'CLEANUP_SUMMARY',
            NULL,
            'Cleanup completed: ' || cleanup_count || ' orphaned schemas removed',
            CURRENT_TIMESTAMP
        );
    END
    
    PROCEDURE get_schema_tree(root_schema VARCHAR(511))
        RETURNS (schema_name VARCHAR(511), level INTEGER, path VARCHAR(511), parent VARCHAR(511)) AS
    BEGIN
        FOR WITH RECURSIVE schema_tree AS (
                SELECT 
                    RDB$SCHEMA_NAME,
                    RDB$SCHEMA_LEVEL,
                    RDB$SCHEMA_PATH,
                    RDB$PARENT_SCHEMA_NAME,
                    1 as tree_level
                FROM RDB$SCHEMAS
                WHERE RDB$SCHEMA_PATH = :root_schema
                
                UNION ALL
                
                SELECT 
                    s.RDB$SCHEMA_NAME,
                    s.RDB$SCHEMA_LEVEL,
                    s.RDB$SCHEMA_PATH,
                    s.RDB$PARENT_SCHEMA_NAME,
                    st.tree_level + 1
                FROM RDB$SCHEMAS s
                JOIN schema_tree st ON s.RDB$PARENT_SCHEMA_NAME = st.RDB$SCHEMA_NAME
            )
            SELECT 
                RDB$SCHEMA_NAME,
                tree_level,
                RDB$SCHEMA_PATH,
                RDB$PARENT_SCHEMA_NAME
            FROM schema_tree
            ORDER BY tree_level, RDB$SCHEMA_NAME
            INTO :schema_name, :level, :path, :parent
        DO
        BEGIN
            SUSPEND;
        END
    END
    
    PROCEDURE get_schema_statistics()
        RETURNS (level INTEGER, schema_count INTEGER, avg_path_length NUMERIC(10,2)) AS
    BEGIN
        FOR SELECT 
                RDB$SCHEMA_LEVEL,
                COUNT(*),
                AVG(CHARACTER_LENGTH(RDB$SCHEMA_PATH))
            FROM RDB$SCHEMAS
            GROUP BY RDB$SCHEMA_LEVEL
            ORDER BY RDB$SCHEMA_LEVEL
            INTO :level, :schema_count, :avg_path_length
        DO
        BEGIN
            SUSPEND;
        END
    END
    
    PROCEDURE migrate_schema(old_path VARCHAR(511), new_path VARCHAR(511)) AS
    DECLARE VARIABLE child_schema VARCHAR(511);
    DECLARE VARIABLE new_child_path VARCHAR(511);
    BEGIN
        -- Validate paths
        IF (NOT validate_schema_path(old_path)) THEN
            EXCEPTION INVALID_SCHEMA_PATH;
        END IF;
        
        IF (NOT validate_schema_path(new_path)) THEN
            EXCEPTION INVALID_SCHEMA_PATH;
        END IF;
        
        -- Check if source schema exists
        IF (NOT schema_exists(old_path)) THEN
            EXCEPTION SCHEMA_NOT_FOUND;
        END IF;
        
        -- Migrate child schemas first
        FOR SELECT RDB$SCHEMA_PATH
            FROM RDB$SCHEMAS
            WHERE RDB$PARENT_SCHEMA_NAME = :old_path
            INTO :child_schema
        DO
        BEGIN
            new_child_path = REPLACE(child_schema, old_path, new_path);
            EXECUTE PROCEDURE migrate_schema(:child_schema, :new_child_path);
        END
        
        -- Update schema path
        UPDATE RDB$SCHEMAS
        SET RDB$SCHEMA_PATH = :new_path,
            RDB$PARENT_SCHEMA_NAME = get_parent_schema(:new_path)
        WHERE RDB$SCHEMA_PATH = :old_path;
        
        -- Log migration
        INSERT INTO schema_migration_log (
            old_path,
            new_path,
            migration_timestamp,
            migration_user
        ) VALUES (
            :old_path,
            :new_path,
            CURRENT_TIMESTAMP,
            USER
        );
    END
    
    PROCEDURE backup_schema_metadata(backup_name VARCHAR(100)) AS
    DECLARE VARIABLE backup_timestamp TIMESTAMP;
    DECLARE VARIABLE record_count INTEGER;
    BEGIN
        backup_timestamp = CURRENT_TIMESTAMP;
        
        -- Create backup table
        EXECUTE STATEMENT '
            CREATE TABLE schema_backup_' || backup_name || ' (
                schema_name VARCHAR(511),
                parent_schema_name VARCHAR(511),
                schema_level INTEGER,
                schema_path VARCHAR(511),
                backup_timestamp TIMESTAMP
            )
        ';
        
        -- Insert schema metadata
        EXECUTE STATEMENT '
            INSERT INTO schema_backup_' || backup_name || '
            SELECT 
                RDB$SCHEMA_NAME,
                RDB$PARENT_SCHEMA_NAME,
                RDB$SCHEMA_LEVEL,
                RDB$SCHEMA_PATH,
                ''' || backup_timestamp || '''
            FROM RDB$SCHEMAS
        ';
        
        -- Get record count
        EXECUTE STATEMENT '
            SELECT COUNT(*) FROM schema_backup_' || backup_name
        INTO :record_count;
        
        -- Log backup
        INSERT INTO schema_backup_log (
            backup_name,
            backup_timestamp,
            record_count,
            backup_user
        ) VALUES (
            :backup_name,
            :backup_timestamp,
            :record_count,
            USER
        );
    END
    
    PROCEDURE restore_schema_metadata(backup_name VARCHAR(100)) AS
    DECLARE VARIABLE record_count INTEGER;
    BEGIN
        -- Get backup record count
        EXECUTE STATEMENT '
            SELECT COUNT(*) FROM schema_backup_' || backup_name
        INTO :record_count;
        
        -- Clear current schema metadata (dangerous!)
        DELETE FROM RDB$SCHEMAS;
        
        -- Restore from backup
        EXECUTE STATEMENT '
            INSERT INTO RDB$SCHEMAS (
                RDB$SCHEMA_NAME,
                RDB$PARENT_SCHEMA_NAME,
                RDB$SCHEMA_LEVEL,
                RDB$SCHEMA_PATH
            )
            SELECT 
                schema_name,
                parent_schema_name,
                schema_level,
                schema_path
            FROM schema_backup_' || backup_name
        ';
        
        -- Log restore
        INSERT INTO schema_restore_log (
            backup_name,
            restore_timestamp,
            record_count,
            restore_user
        ) VALUES (
            :backup_name,
            CURRENT_TIMESTAMP,
            :record_count,
            USER
        );
    END
    
    -- ================================================================
    -- Administrative Procedures
    -- ================================================================
    
    PROCEDURE rebuild_schema_cache() AS
    BEGIN
        -- Clear existing cache
        DELETE FROM schema_cache;
        
        -- Rebuild cache
        INSERT INTO schema_cache (
            schema_name,
            schema_path,
            schema_level,
            parent_schema,
            child_count,
            cache_timestamp
        )
        SELECT 
            s.RDB$SCHEMA_NAME,
            s.RDB$SCHEMA_PATH,
            s.RDB$SCHEMA_LEVEL,
            s.RDB$PARENT_SCHEMA_NAME,
            (SELECT COUNT(*) FROM RDB$SCHEMAS c WHERE c.RDB$PARENT_SCHEMA_NAME = s.RDB$SCHEMA_NAME),
            CURRENT_TIMESTAMP
        FROM RDB$SCHEMAS s;
        
        -- Update cache statistics
        UPDATE schema_cache_stats
        SET last_rebuild = CURRENT_TIMESTAMP,
            cache_size = (SELECT COUNT(*) FROM schema_cache);
    END
    
    PROCEDURE analyze_schema_performance() AS
    DECLARE VARIABLE avg_query_time NUMERIC(10,3);
    DECLARE VARIABLE slow_queries INTEGER;
    BEGIN
        -- Analyze schema query performance
        SELECT 
            AVG(query_duration),
            COUNT(*)
        FROM schema_query_log
        WHERE query_timestamp > CURRENT_TIMESTAMP - INTERVAL 1 HOUR
          AND query_duration > 1000 -- queries taking more than 1 second
        INTO :avg_query_time, :slow_queries;
        
        -- Log performance analysis
        INSERT INTO schema_performance_log (
            analysis_timestamp,
            avg_query_time_ms,
            slow_query_count,
            recommendation
        ) VALUES (
            CURRENT_TIMESTAMP,
            :avg_query_time,
            :slow_queries,
            CASE 
                WHEN :slow_queries > 10 THEN 'Consider rebuilding schema cache'
                WHEN :avg_query_time > 500 THEN 'Optimize schema indexes'
                ELSE 'Performance acceptable'
            END
        );
    END
    
    PROCEDURE optimize_schema_structure() AS
    DECLARE VARIABLE deep_schemas INTEGER;
    DECLARE VARIABLE long_paths INTEGER;
    BEGIN
        -- Check for overly deep schemas
        SELECT COUNT(*)
        FROM RDB$SCHEMAS
        WHERE RDB$SCHEMA_LEVEL > 6
        INTO :deep_schemas;
        
        -- Check for overly long paths
        SELECT COUNT(*)
        FROM RDB$SCHEMAS
        WHERE CHARACTER_LENGTH(RDB$SCHEMA_PATH) > 400
        INTO :long_paths;
        
        -- Generate optimization recommendations
        IF (:deep_schemas > 0) THEN
        BEGIN
            INSERT INTO schema_optimization_log (
                optimization_type,
                issue_count,
                recommendation,
                optimization_timestamp
            ) VALUES (
                'DEEP_SCHEMAS',
                :deep_schemas,
                'Consider restructuring schemas with depth > 6 levels',
                CURRENT_TIMESTAMP
            );
        END
        
        IF (:long_paths > 0) THEN
        BEGIN
            INSERT INTO schema_optimization_log (
                optimization_type,
                issue_count,
                recommendation,
                optimization_timestamp
            ) VALUES (
                'LONG_PATHS',
                :long_paths,
                'Consider shortening schema names for paths > 400 characters',
                CURRENT_TIMESTAMP
            );
        END
    END
    
    PROCEDURE generate_schema_report(format_type VARCHAR(20) DEFAULT 'TEXT') AS
    DECLARE VARIABLE report_line VARCHAR(500);
    BEGIN
        -- Generate comprehensive schema report
        IF (format_type = 'TEXT') THEN
        BEGIN
            report_line = '=== ScratchBird Schema Management Report ===';
            SUSPEND;
            
            report_line = 'Generated: ' || CURRENT_TIMESTAMP;
            SUSPEND;
            
            report_line = 'Package Version: ' || PACKAGE_VERSION;
            SUSPEND;
            
            report_line = '';
            SUSPEND;
            
            -- Schema statistics
            report_line = 'Schema Statistics:';
            SUSPEND;
            
            FOR SELECT 
                    'Level ' || level || ': ' || schema_count || ' schemas (avg path length: ' || avg_path_length || ')'
                FROM get_schema_statistics()
                INTO :report_line
            DO
            BEGIN
                SUSPEND;
            END
            
            -- Recent validation results
            report_line = '';
            SUSPEND;
            report_line = 'Last Validation: ' || COALESCE(last_validation_timestamp, 'Never');
            SUSPEND;
        END
    END
    
    -- ================================================================
    -- Package Initialization
    -- ================================================================
    
    -- Initialize package variables
    last_validation_timestamp = NULL;
    
    -- Create supporting tables if they don't exist
    EXECUTE STATEMENT '
        CREATE TABLE IF NOT EXISTS schema_validation_log (
            log_id UUID DEFAULT GEN_UUID(7),
            issue_type VARCHAR(50),
            schema_name VARCHAR(511),
            issue_description VARCHAR(1000),
            detected_at TIMESTAMP,
            resolved_at TIMESTAMP
        )
    ';
    
    EXECUTE STATEMENT '
        CREATE TABLE IF NOT EXISTS schema_cleanup_log (
            log_id UUID DEFAULT GEN_UUID(7),
            action_type VARCHAR(50),
            schema_name VARCHAR(511),
            action_description VARCHAR(1000),
            action_timestamp TIMESTAMP
        )
    ';
    
    EXECUTE STATEMENT '
        CREATE TABLE IF NOT EXISTS schema_migration_log (
            log_id UUID DEFAULT GEN_UUID(7),
            old_path VARCHAR(511),
            new_path VARCHAR(511),
            migration_timestamp TIMESTAMP,
            migration_user VARCHAR(50)
        )
    ';
    
    EXECUTE STATEMENT '
        CREATE TABLE IF NOT EXISTS schema_backup_log (
            log_id UUID DEFAULT GEN_UUID(7),
            backup_name VARCHAR(100),
            backup_timestamp TIMESTAMP,
            record_count INTEGER,
            backup_user VARCHAR(50)
        )
    ';
END;

-- ================================================================
-- Package Usage Examples
-- ================================================================

-- Example 1: Basic schema validation
SELECT schema_management.validate_schema_path('company.finance.accounting.reports') as is_valid;

-- Example 2: Calculate schema level
SELECT schema_management.calculate_schema_level('company.finance.accounting.reports') as level;

-- Example 3: Get parent schema
SELECT schema_management.get_parent_schema('company.finance.accounting.reports') as parent;

-- Example 4: Create schema hierarchy
EXECUTE PROCEDURE schema_management.create_schema_hierarchy('test_company', 'finance
accounting
reports
users
products');

-- Example 5: Get schema tree
SELECT * FROM schema_management.get_schema_tree('test_company');

-- Example 6: Get schema statistics
SELECT * FROM schema_management.get_schema_statistics();

-- Example 7: Validate schema integrity
EXECUTE PROCEDURE schema_management.validate_schema_integrity();

-- Example 8: Generate schema report
SELECT * FROM schema_management.generate_schema_report('TEXT');

-- ================================================================
-- Package Grants and Security
-- ================================================================

-- Grant usage to specific roles
GRANT USAGE ON PACKAGE schema_management TO ROLE SCHEMA_ADMIN;
GRANT USAGE ON PACKAGE schema_management TO ROLE DBA;

-- Grant execute permissions for specific functions
GRANT EXECUTE ON FUNCTION schema_management.validate_schema_path TO PUBLIC;
GRANT EXECUTE ON FUNCTION schema_management.calculate_schema_level TO PUBLIC;
GRANT EXECUTE ON FUNCTION schema_management.get_parent_schema TO PUBLIC;

-- Restrict administrative procedures
GRANT EXECUTE ON PROCEDURE schema_management.cleanup_orphaned_schemas TO ROLE SCHEMA_ADMIN;
GRANT EXECUTE ON PROCEDURE schema_management.migrate_schema TO ROLE DBA;
GRANT EXECUTE ON PROCEDURE schema_management.backup_schema_metadata TO ROLE DBA;
GRANT EXECUTE ON PROCEDURE schema_management.restore_schema_metadata TO ROLE DBA;

COMMIT;

/*
 * This schema management package provides:
 * 
 * 1. Comprehensive schema validation and integrity checking
 * 2. Hierarchical schema navigation utilities
 * 3. Schema migration and backup capabilities
 * 4. Performance analysis and optimization tools
 * 5. Administrative procedures for schema management
 * 6. Reporting and monitoring capabilities
 * 
 * Key Features:
 * - Supports up to 8-level deep hierarchies
 * - Validates schema paths and detects circular references
 * - Provides schema tree navigation
 * - Includes backup and restore functionality
 * - Offers performance optimization recommendations
 * - Generates comprehensive reports
 * 
 * Usage:
 * - Install the package in your ScratchBird database
 * - Grant appropriate permissions to users/roles
 * - Use the functions and procedures for schema management
 * - Monitor schema health with validation procedures
 * - Generate reports for schema analysis
 */