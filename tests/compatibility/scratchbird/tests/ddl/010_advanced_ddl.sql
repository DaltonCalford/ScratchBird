-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DDL - Advanced DDL Operations
-- Description: Extensions, foreign data wrappers, casts, operators, aggregates
-- ============================================================================

-- Advanced DDL: Extensions, casts, operators, conversions, aggregates
-- CREATE EXTENSION, CREATE CAST, CREATE OPERATOR
-- CREATE AGGREGATE, CREATE CONVERSION
-- Advanced database features

-- Create test database
CREATE DATABASE test_advanced_ddl_db;
USE test_advanced_ddl_db;

-- ============================================================================
-- Section 1: CREATE EXTENSION
-- ============================================================================

-- Check available extensions
SELECT
    name,
    default_version,
    installed_version,
    comment
FROM pg_available_extensions
WHERE name IN ('pgcrypto', 'uuid-ossp', 'hstore', 'ltree', 'pg_trgm')
ORDER BY name;

-- Create extension if available
-- CREATE EXTENSION IF NOT EXISTS pgcrypto;

-- Query installed extensions
SELECT
    extname AS extension_name,
    extversion AS version,
    nspname AS schema
FROM pg_extension e
JOIN pg_namespace n ON e.extnamespace = n.oid
ORDER BY extname;

-- ============================================================================
-- Section 2: DROP EXTENSION
-- ============================================================================

-- Drop extension (if exists)
-- DROP EXTENSION IF EXISTS pgcrypto CASCADE;

-- Verify dropped
SELECT COUNT(*) AS extension_count
FROM pg_extension
WHERE extname = 'pgcrypto';

-- ============================================================================
-- Section 3: CREATE CAST
-- ============================================================================

-- Create composite type for casting example
CREATE TYPE test_coordinates AS (
    x FLOAT,
    y FLOAT
);

-- Create function for cast
CREATE FUNCTION coordinates_to_text(test_coordinates)
RETURNS TEXT AS $$
BEGIN
    RETURN '(' || $1.x || ',' || $1.y || ')';
END;
$$ LANGUAGE plpgsql IMMUTABLE;

-- Create cast
CREATE CAST (test_coordinates AS TEXT)
WITH FUNCTION coordinates_to_text(test_coordinates)
AS ASSIGNMENT;

-- Test cast
SELECT ROW(1.5, 2.7)::test_coordinates::TEXT;

-- Query casts
SELECT
    castsource::regtype AS source_type,
    casttarget::regtype AS target_type,
    castfunc::regprocedure AS cast_function,
    castcontext AS context,
    castmethod AS method
FROM pg_cast
WHERE castsource = 'test_coordinates'::regtype
   OR casttarget = 'test_coordinates'::regtype;

-- ============================================================================
-- Section 4: DROP CAST
-- ============================================================================

-- Drop cast
DROP CAST (test_coordinates AS TEXT);

-- Verify dropped
SELECT COUNT(*) AS cast_count
FROM pg_cast
WHERE castsource = 'test_coordinates'::regtype
  AND casttarget = 'text'::regtype;

-- ============================================================================
-- Section 5: CREATE OPERATOR
-- ============================================================================

-- Create function for operator
CREATE FUNCTION coordinates_distance(test_coordinates, test_coordinates)
RETURNS FLOAT AS $$
BEGIN
    RETURN SQRT(POWER($2.x - $1.x, 2) + POWER($2.y - $1.y, 2));
END;
$$ LANGUAGE plpgsql IMMUTABLE;

-- Create custom operator
CREATE OPERATOR <-> (
    LEFTARG = test_coordinates,
    RIGHTARG = test_coordinates,
    FUNCTION = coordinates_distance,
    COMMUTATOR = <->
);

-- Test operator
SELECT ROW(0, 0)::test_coordinates <-> ROW(3, 4)::test_coordinates AS distance;

-- Query operators
SELECT
    oprname AS operator,
    oprleft::regtype AS left_type,
    oprright::regtype AS right_type,
    oprresult::regtype AS result_type,
    oprcode::regprocedure AS function
FROM pg_operator
WHERE oprname = '<->'
  AND oprleft = 'test_coordinates'::regtype;

-- ============================================================================
-- Section 6: DROP OPERATOR
-- ============================================================================

-- Drop operator
DROP OPERATOR <-> (test_coordinates, test_coordinates);

-- Verify dropped
SELECT COUNT(*) AS operator_count
FROM pg_operator
WHERE oprname = '<->'
  AND oprleft = 'test_coordinates'::regtype;

-- ============================================================================
-- Section 7: CREATE AGGREGATE
-- ============================================================================

-- Create state transition function
CREATE FUNCTION sum_product_sfunc(NUMERIC, NUMERIC, NUMERIC)
RETURNS NUMERIC AS $$
BEGIN
    RETURN $1 + ($2 * $3);
END;
$$ LANGUAGE plpgsql IMMUTABLE;

-- Create custom aggregate function
CREATE AGGREGATE test_sum_product(NUMERIC, NUMERIC) (
    SFUNC = sum_product_sfunc,
    STYPE = NUMERIC,
    INITCOND = '0'
);

-- Test aggregate
CREATE TABLE test_aggregate_data (
    quantity INT,
    price NUMERIC(10,2)
);

INSERT INTO test_aggregate_data VALUES
    (10, 5.50),
    (20, 3.25),
    (15, 7.00);

SELECT test_sum_product(quantity, price) AS total FROM test_aggregate_data;

-- Query aggregates
SELECT
    p.proname AS aggregate_name,
    format_type(p.prorettype, NULL) AS return_type,
    pg_get_function_arguments(p.oid) AS arguments
FROM pg_proc p
WHERE p.prokind = 'a'
  AND p.proname = 'test_sum_product';

-- ============================================================================
-- Section 8: DROP AGGREGATE
-- ============================================================================

-- Drop aggregate
DROP AGGREGATE test_sum_product(NUMERIC, NUMERIC);

-- Verify dropped
SELECT COUNT(*) AS aggregate_count
FROM pg_proc
WHERE proname = 'test_sum_product'
  AND prokind = 'a';

-- ============================================================================
-- Section 9: CREATE CONVERSION
-- ============================================================================

-- Query available conversions
SELECT
    conname AS conversion_name,
    pg_encoding_to_char(conforencoding) AS from_encoding,
    pg_encoding_to_char(contoencoding) AS to_encoding,
    condefault AS is_default
FROM pg_conversion
WHERE conforencoding = pg_char_to_encoding('UTF8')
   OR contoencoding = pg_char_to_encoding('UTF8')
ORDER BY conname
LIMIT 10;

-- Create custom conversion (requires function)
-- CREATE CONVERSION test_conversion
--     FOR 'LATIN1' TO 'UTF8'
--     FROM latin1_to_utf8;

-- ============================================================================
-- Section 10: CREATE COLLATION
-- ============================================================================

-- Query available collations
SELECT
    collname,
    collprovider,
    collencoding
FROM pg_collation
WHERE collencoding = pg_char_to_encoding('UTF8')
ORDER BY collname
LIMIT 10;

-- Create custom collation (if ICU available)
-- CREATE COLLATION test_collation (
--     PROVIDER = icu,
--     LOCALE = 'en-US-x-icu'
-- );

-- ============================================================================
-- Section 11: CREATE TEXT SEARCH CONFIGURATION
-- ============================================================================

-- Query text search configurations
SELECT
    cfgname AS config_name,
    cfgnamespace::regnamespace AS schema,
    cfgowner::regrole AS owner
FROM pg_ts_config
ORDER BY cfgname;

-- Create custom text search configuration
CREATE TEXT SEARCH CONFIGURATION test_ts_config (COPY = english);

-- Alter configuration
ALTER TEXT SEARCH CONFIGURATION test_ts_config
    ALTER MAPPING FOR word WITH simple;

-- Query configuration details
SELECT
    ts_token_type('default') AS token_types;

-- ============================================================================
-- Section 12: CREATE TEXT SEARCH DICTIONARY
-- ============================================================================

-- Query text search dictionaries
SELECT
    dictname AS dictionary_name,
    dictnamespace::regnamespace AS schema,
    dictowner::regrole AS owner
FROM pg_ts_dict
ORDER BY dictname
LIMIT 10;

-- Create custom dictionary
CREATE TEXT SEARCH DICTIONARY test_dict (
    TEMPLATE = simple
);

-- Test dictionary
SELECT ts_lexize('test_dict', 'testing');

-- ============================================================================
-- Section 13: CREATE RULE
-- ============================================================================

CREATE TABLE test_rule_target (
    id SERIAL PRIMARY KEY,
    data TEXT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_rule_log (
    log_id SERIAL PRIMARY KEY,
    operation TEXT,
    data TEXT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Create rule for INSERT (log to separate table)
CREATE RULE test_insert_rule AS
    ON INSERT TO test_rule_target
    DO ALSO
    INSERT INTO test_rule_log (operation, data)
    VALUES ('INSERT', NEW.data);

-- Test rule
INSERT INTO test_rule_target (data) VALUES ('Test data');

-- Check both tables
SELECT * FROM test_rule_target;
SELECT * FROM test_rule_log;

-- Query rules
SELECT
    rulename AS rule_name,
    tablename,
    definition
FROM pg_rules
WHERE tablename = 'test_rule_target';

-- ============================================================================
-- Section 14: DROP RULE
-- ============================================================================

-- Drop rule
DROP RULE test_insert_rule ON test_rule_target;

-- Verify dropped
SELECT COUNT(*) AS rule_count
FROM pg_rules
WHERE tablename = 'test_rule_target';

-- ============================================================================
-- Section 15: CREATE EVENT TRIGGER
-- ============================================================================

-- Create function for event trigger
CREATE FUNCTION test_event_trigger_function()
RETURNS event_trigger AS $$
BEGIN
    RAISE NOTICE 'DDL command executed: %', tg_tag;
END;
$$ LANGUAGE plpgsql;

-- Create event trigger
CREATE EVENT TRIGGER test_ddl_trigger
    ON ddl_command_end
    EXECUTE FUNCTION test_event_trigger_function();

-- Test event trigger (creates notice)
CREATE TABLE test_event_trigger_test (id INT);

-- Query event triggers
SELECT
    evtname AS trigger_name,
    evtevent AS event,
    evtenabled AS enabled,
    evtfoid::regproc AS function
FROM pg_event_trigger
WHERE evtname = 'test_ddl_trigger';

-- Disable event trigger
ALTER EVENT TRIGGER test_ddl_trigger DISABLE;

-- Drop event trigger
DROP EVENT TRIGGER test_ddl_trigger;

DROP TABLE test_event_trigger_test;

-- ============================================================================
-- Section 16: CREATE PUBLICATION (Logical Replication)
-- ============================================================================

CREATE TABLE test_publication_table (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Create publication
CREATE PUBLICATION test_pub FOR TABLE test_publication_table;

-- Add table to publication
ALTER PUBLICATION test_pub ADD TABLE test_aggregate_data;

-- Query publications
SELECT
    pubname AS publication_name,
    pubowner::regrole AS owner,
    puballtables AS all_tables,
    pubinsert,
    pubupdate,
    pubdelete,
    pubtruncate
FROM pg_publication
WHERE pubname = 'test_pub';

-- Query publication tables
SELECT
    p.pubname AS publication,
    c.relname AS table_name
FROM pg_publication p
JOIN pg_publication_rel pr ON p.oid = pr.prpubid
JOIN pg_class c ON pr.prrelid = c.oid
WHERE p.pubname = 'test_pub'
ORDER BY c.relname;

-- Drop publication
DROP PUBLICATION test_pub;

-- ============================================================================
-- Section 17: CREATE SUBSCRIPTION (Logical Replication)
-- ============================================================================

-- Query subscriptions
SELECT
    subname AS subscription_name,
    subowner::regrole AS owner,
    subenabled AS enabled,
    subconninfo AS connection_info
FROM pg_subscription
ORDER BY subname;

-- Create subscription (requires replication setup)
-- CREATE SUBSCRIPTION test_sub
--     CONNECTION 'host=replica_host dbname=test_db'
--     PUBLICATION test_pub;

-- ============================================================================
-- Section 18: CREATE STATISTICS
-- ============================================================================

CREATE TABLE test_statistics_table (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    subcategory VARCHAR(50),
    value INT
);

INSERT INTO test_statistics_table (category, subcategory, value)
SELECT
    CASE (i % 3) WHEN 0 THEN 'A' WHEN 1 THEN 'B' ELSE 'C' END,
    CASE (i % 2) WHEN 0 THEN 'X' ELSE 'Y' END,
    i
FROM generate_series(1, 1000) i;

-- Create extended statistics
CREATE STATISTICS test_stats (dependencies)
ON category, subcategory
FROM test_statistics_table;

-- Analyze to populate statistics
ANALYZE test_statistics_table;

-- Query statistics
SELECT
    stxname AS statistics_name,
    stxnamespace::regnamespace AS schema,
    stxowner::regrole AS owner,
    stxrelid::regclass AS table_name
FROM pg_statistic_ext
WHERE stxname = 'test_stats';

-- Drop statistics
DROP STATISTICS test_stats;

-- ============================================================================
-- Section 19: CREATE POLICY (Row Level Security)
-- ============================================================================

CREATE TABLE test_policy_table (
    id SERIAL PRIMARY KEY,
    user_id INT,
    data TEXT
);

-- Enable RLS
ALTER TABLE test_policy_table ENABLE ROW LEVEL SECURITY;

-- Create restrictive policy
CREATE POLICY test_select_policy ON test_policy_table
    FOR SELECT
    USING (user_id = current_setting('app.user_id', true)::INT);

-- Create permissive policy for INSERT
CREATE POLICY test_insert_policy ON test_policy_table
    FOR INSERT
    WITH CHECK (user_id = current_setting('app.user_id', true)::INT);

-- Query policies
SELECT
    schemaname,
    tablename,
    policyname,
    permissive,
    roles,
    cmd AS command,
    qual AS using_expression,
    with_check
FROM pg_policies
WHERE tablename = 'test_policy_table'
ORDER BY policyname;

-- Drop policy
DROP POLICY test_select_policy ON test_policy_table;
DROP POLICY test_insert_policy ON test_policy_table;

-- Disable RLS
ALTER TABLE test_policy_table DISABLE ROW LEVEL SECURITY;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_advanced_ddl_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_advanced_ddl_best_practices VALUES
    (1, 'Use extensions for additional functionality (pgcrypto, uuid-ossp)'),
    (2, 'Test custom casts thoroughly (type safety)'),
    (3, 'Document custom operators clearly'),
    (4, 'Custom aggregates useful for complex calculations'),
    (5, 'Use built-in aggregates when possible (better optimized)'),
    (6, 'Event triggers monitor DDL changes'),
    (7, 'Publications for logical replication setup'),
    (8, 'Extended statistics improve query planning'),
    (9, 'Row Level Security (RLS) for multi-tenant security'),
    (10, 'Test RLS policies thoroughly (security critical)'),
    (11, 'Rules can be complex (consider triggers instead)'),
    (12, 'Text search for full-text search capabilities'),
    (13, 'Custom collations for specific sorting needs'),
    (14, 'Monitor extension updates for security patches'),
    (15, 'Document custom types and operators'),
    (16, 'Use CREATE OR REPLACE where supported'),
    (17, 'Test advanced features in development first'),
    (18, 'Consider performance impact of custom functions'),
    (19, 'Backup before creating complex objects'),
    (20, 'Keep advanced DDL organized and documented');

SELECT id, guideline FROM test_advanced_ddl_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_advanced_ddl_best_practices;
DROP TABLE test_policy_table;
DROP TABLE test_statistics_table;
DROP TABLE test_publication_table;
DROP FUNCTION test_event_trigger_function();
DROP TABLE test_rule_log;
DROP TABLE test_rule_target;
DROP TEXT SEARCH DICTIONARY test_dict;
DROP TEXT SEARCH CONFIGURATION test_ts_config;
DROP TABLE test_aggregate_data;
DROP FUNCTION sum_product_sfunc(NUMERIC, NUMERIC, NUMERIC);
DROP FUNCTION coordinates_distance(test_coordinates, test_coordinates);
DROP FUNCTION coordinates_to_text(test_coordinates);
DROP TYPE test_coordinates;

DROP DATABASE test_advanced_ddl_db;

-- End of advanced DDL tests
