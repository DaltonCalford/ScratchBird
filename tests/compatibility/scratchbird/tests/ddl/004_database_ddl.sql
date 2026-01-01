-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DDL - Database Operations
-- Description: Comprehensive CREATE, ALTER, DROP DATABASE testing
-- ============================================================================

-- Database DDL: Database management
-- CREATE DATABASE: Create new databases
-- ALTER DATABASE: Modify databases
-- DROP DATABASE: Remove databases
-- Database properties, encoding, templates

-- Note: This test file operates from the default postgres database
-- since we need to create/drop test databases

-- ============================================================================
-- Section 1: Basic CREATE DATABASE
-- ============================================================================

-- Create simple database
CREATE DATABASE test_basic_db;

-- Verify database exists
SELECT
    datname,
    datdba::regrole AS owner,
    encoding,
    datcollate,
    datctype
FROM pg_database
WHERE datname = 'test_basic_db';

-- ============================================================================
-- Section 2: CREATE DATABASE with Owner
-- ============================================================================

-- Create database with owner
CREATE DATABASE test_owner_db OWNER CURRENT_USER;

-- Verify owner
SELECT
    datname,
    datdba::regrole AS owner
FROM pg_database
WHERE datname = 'test_owner_db';

-- ============================================================================
-- Section 3: CREATE DATABASE with Template
-- ============================================================================

-- Create database from template
CREATE DATABASE test_template_db TEMPLATE template0;

-- Verify creation
SELECT
    datname,
    datistemplate
FROM pg_database
WHERE datname = 'test_template_db';

-- ============================================================================
-- Section 4: CREATE DATABASE with Encoding
-- ============================================================================

-- Create database with specific encoding
CREATE DATABASE test_encoding_db
    ENCODING 'UTF8'
    LC_COLLATE = 'en_US.UTF-8'
    LC_CTYPE = 'en_US.UTF-8';

-- Verify encoding
SELECT
    datname,
    pg_encoding_to_char(encoding) AS encoding,
    datcollate,
    datctype
FROM pg_database
WHERE datname = 'test_encoding_db';

-- ============================================================================
-- Section 5: CREATE DATABASE with Connection Limit
-- ============================================================================

-- Create database with connection limit
CREATE DATABASE test_connlimit_db
    CONNECTION LIMIT 50;

-- Verify connection limit
SELECT
    datname,
    datconnlimit
FROM pg_database
WHERE datname = 'test_connlimit_db';

-- ============================================================================
-- Section 6: CREATE DATABASE IF NOT EXISTS (PostgreSQL 15+)
-- ============================================================================

-- Create database if doesn't exist
-- CREATE DATABASE IF NOT EXISTS test_idempotent_db;

-- Workaround for earlier versions
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_database WHERE datname = 'test_idempotent_db') THEN
        CREATE DATABASE test_idempotent_db;
    END IF;
END $$;

-- Verify creation
SELECT datname
FROM pg_database
WHERE datname = 'test_idempotent_db';

-- ============================================================================
-- Section 7: ALTER DATABASE RENAME
-- ============================================================================

CREATE DATABASE test_old_db_name;

-- Rename database (must not be connected to it)
ALTER DATABASE test_old_db_name RENAME TO test_new_db_name;

-- Verify rename
SELECT datname
FROM pg_database
WHERE datname IN ('test_old_db_name', 'test_new_db_name');

-- ============================================================================
-- Section 8: ALTER DATABASE OWNER
-- ============================================================================

CREATE DATABASE test_alter_owner_db;

-- Change owner
ALTER DATABASE test_alter_owner_db OWNER TO CURRENT_USER;

-- Verify owner
SELECT
    datname,
    datdba::regrole AS owner
FROM pg_database
WHERE datname = 'test_alter_owner_db';

-- ============================================================================
-- Section 9: ALTER DATABASE SET Configuration
-- ============================================================================

CREATE DATABASE test_config_db;

-- Set database-level configuration
ALTER DATABASE test_config_db SET timezone = 'UTC';
ALTER DATABASE test_config_db SET work_mem = '16MB';

-- Query database configuration
SELECT
    setdatabase,
    setconfig
FROM pg_db_role_setting
WHERE setdatabase = (SELECT oid FROM pg_database WHERE datname = 'test_config_db');

-- ============================================================================
-- Section 10: ALTER DATABASE Connection Limit
-- ============================================================================

CREATE DATABASE test_alter_connlimit_db;

-- Set connection limit
ALTER DATABASE test_alter_connlimit_db CONNECTION LIMIT 100;

-- Verify limit
SELECT
    datname,
    datconnlimit
FROM pg_database
WHERE datname = 'test_alter_connlimit_db';

-- ============================================================================
-- Section 11: DROP DATABASE
-- ============================================================================

CREATE DATABASE test_drop_db;

-- Drop database (must not be connected to it)
DROP DATABASE test_drop_db;

-- Verify dropped
SELECT COUNT(*) AS database_count
FROM pg_database
WHERE datname = 'test_drop_db';

-- ============================================================================
-- Section 12: DROP DATABASE IF EXISTS
-- ============================================================================

-- Drop if exists (no error if doesn't exist)
DROP DATABASE IF EXISTS test_nonexistent_db;

-- Create and drop
CREATE DATABASE test_temp_db;
DROP DATABASE IF EXISTS test_temp_db;

-- Try again (no error)
DROP DATABASE IF EXISTS test_temp_db;

-- ============================================================================
-- Section 13: Database Size
-- ============================================================================

CREATE DATABASE test_size_db;

-- Connect and create data
\c test_size_db

CREATE TABLE test_data (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_data (data)
SELECT 'Data ' || i FROM generate_series(1, 10000) i;

\c postgres

-- Query database size
SELECT
    datname,
    pg_size_pretty(pg_database_size(datname)) AS database_size
FROM pg_database
WHERE datname = 'test_size_db';

-- ============================================================================
-- Section 14: Database Statistics
-- ============================================================================

-- Query database statistics
SELECT
    datname,
    numbackends AS active_connections,
    xact_commit AS commits,
    xact_rollback AS rollbacks,
    blks_read AS disk_blocks_read,
    blks_hit AS cache_blocks_hit,
    tup_returned,
    tup_fetched,
    tup_inserted,
    tup_updated,
    tup_deleted,
    conflicts,
    temp_files,
    temp_bytes,
    deadlocks,
    blk_read_time,
    blk_write_time
FROM pg_stat_database
WHERE datname LIKE 'test_%'
ORDER BY datname;

-- ============================================================================
-- Section 15: Active Connections to Database
-- ============================================================================

-- Query active connections
SELECT
    datname,
    COUNT(*) AS connection_count,
    array_agg(DISTINCT usename) AS users,
    array_agg(DISTINCT application_name) AS applications
FROM pg_stat_activity
WHERE datname LIKE 'test_%'
GROUP BY datname
ORDER BY datname;

-- ============================================================================
-- Section 16: Template Databases
-- ============================================================================

-- Create template database
CREATE DATABASE test_custom_template;

-- Mark as template
UPDATE pg_database
SET datistemplate = TRUE
WHERE datname = 'test_custom_template';

-- Add objects to template
\c test_custom_template

CREATE TABLE template_table (
    id SERIAL PRIMARY KEY,
    common_field VARCHAR(100)
);

CREATE FUNCTION template_function() RETURNS TEXT AS $$
BEGIN
    RETURN 'From template';
END;
$$ LANGUAGE plpgsql;

\c postgres

-- Create database from custom template
CREATE DATABASE test_from_custom_template TEMPLATE test_custom_template;

-- Verify objects copied
\c test_from_custom_template

SELECT
    table_name
FROM information_schema.tables
WHERE table_schema = 'public'
  AND table_name = 'template_table';

SELECT
    routine_name
FROM information_schema.routines
WHERE routine_schema = 'public'
  AND routine_name = 'template_function';

\c postgres

-- ============================================================================
-- Section 17: Database Tablespace
-- ============================================================================

-- Query database tablespaces
SELECT
    datname,
    spcname AS tablespace
FROM pg_database d
JOIN pg_tablespace t ON d.dattablespace = t.oid
WHERE datname LIKE 'test_%'
ORDER BY datname;

-- ============================================================================
-- Section 18: Terminate Database Connections
-- ============================================================================

CREATE DATABASE test_terminate_connections_db;

-- Terminate all connections (to prepare for drop/rename)
SELECT pg_terminate_backend(pid)
FROM pg_stat_activity
WHERE datname = 'test_terminate_connections_db'
  AND pid <> pg_backend_pid();

-- Now safe to drop
DROP DATABASE test_terminate_connections_db;

-- ============================================================================
-- Section 19: List All Databases
-- ============================================================================

-- Query all databases
SELECT
    datname,
    datdba::regrole AS owner,
    pg_encoding_to_char(encoding) AS encoding,
    datcollate,
    datctype,
    datistemplate AS is_template,
    datallowconn AS allow_connections,
    datconnlimit AS connection_limit,
    pg_size_pretty(pg_database_size(datname)) AS size
FROM pg_database
ORDER BY datname;

-- Count databases by type
SELECT
    CASE
        WHEN datistemplate THEN 'Template'
        WHEN NOT datallowconn THEN 'No Connections Allowed'
        ELSE 'Regular'
    END AS database_type,
    COUNT(*) AS database_count
FROM pg_database
GROUP BY database_type
ORDER BY database_type;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE DATABASE test_database_ddl_best_practices;

\c test_database_ddl_best_practices

CREATE TABLE database_ddl_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO database_ddl_best_practices VALUES
    (1, 'Use descriptive database names (e.g., myapp_production, myapp_test)'),
    (2, 'Specify encoding (UTF8) explicitly for consistency'),
    (3, 'Set appropriate connection limits for production databases'),
    (4, 'Use template databases for consistent new database structure'),
    (5, 'Cannot rename or drop database while connected to it'),
    (6, 'Terminate connections before dropping database'),
    (7, 'Use database-level configuration for specific settings'),
    (8, 'Monitor database size with pg_database_size'),
    (9, 'Track database statistics with pg_stat_database'),
    (10, 'Separate databases for different applications/environments'),
    (11, 'Consider tablespaces for I/O distribution'),
    (12, 'Regular backups before DDL operations'),
    (13, 'Template0 for creating databases with different encodings'),
    (14, 'Template1 is default template database'),
    (15, 'Database-level roles for access control'),
    (16, 'Test database DDL changes before production'),
    (17, 'Document database purpose and ownership'),
    (18, 'Monitor active connections and locks'),
    (19, 'Plan for growth (size, connections, performance)'),
    (20, 'Coordinate database DDL with application deployments');

SELECT id, guideline FROM database_ddl_best_practices ORDER BY id;

\c postgres

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP DATABASE test_database_ddl_best_practices;
DROP DATABASE test_from_custom_template;
DROP DATABASE test_custom_template;
DROP DATABASE test_size_db;
DROP DATABASE test_alter_connlimit_db;
DROP DATABASE test_config_db;
DROP DATABASE test_alter_owner_db;
DROP DATABASE test_new_db_name;
DROP DATABASE test_idempotent_db;
DROP DATABASE test_connlimit_db;
DROP DATABASE test_encoding_db;
DROP DATABASE test_template_db;
DROP DATABASE test_owner_db;
DROP DATABASE test_basic_db;

-- End of database DDL tests
