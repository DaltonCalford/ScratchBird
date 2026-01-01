-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DDL - ALTER Operations
-- Description: Comprehensive ALTER statement testing for various objects
-- ============================================================================

-- ALTER DDL: Modifying existing database objects
-- ALTER TABLE, ALTER INDEX, ALTER SEQUENCE, etc.
-- Complex modifications, restructuring, constraints

-- Create test database
CREATE DATABASE test_alter_operations_db;
USE test_alter_operations_db;

-- ============================================================================
-- Section 1: ALTER TABLE SET SCHEMA
-- ============================================================================

CREATE SCHEMA test_schema_old;
CREATE SCHEMA test_schema_new;

CREATE TABLE test_schema_old.test_move_table (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Move table to different schema
ALTER TABLE test_schema_old.test_move_table SET SCHEMA test_schema_new;

-- Verify move
SELECT
    table_schema,
    table_name
FROM information_schema.tables
WHERE table_name = 'test_move_table';

-- ============================================================================
-- Section 2: ALTER TABLE SET TABLESPACE
-- ============================================================================

CREATE TABLE test_tablespace_move (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Move to different tablespace
-- ALTER TABLE test_tablespace_move SET TABLESPACE pg_default;

-- Query tablespace
SELECT
    c.relname AS table_name,
    COALESCE(ts.spcname, 'default') AS tablespace
FROM pg_class c
LEFT JOIN pg_tablespace ts ON c.reltablespace = ts.oid
WHERE c.relname = 'test_tablespace_move';

-- ============================================================================
-- Section 3: ALTER TABLE OWNER
-- ============================================================================

CREATE ROLE test_new_owner;

CREATE TABLE test_owner_change (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Change table owner
ALTER TABLE test_owner_change OWNER TO test_new_owner;

-- Verify owner
SELECT
    c.relname AS table_name,
    r.rolname AS owner
FROM pg_class c
JOIN pg_roles r ON c.relowner = r.oid
WHERE c.relname = 'test_owner_change';

-- ============================================================================
-- Section 4: ALTER TABLE ADD/DROP Constraints
-- ============================================================================

CREATE TABLE test_constraint_changes (
    id SERIAL,
    email VARCHAR(200),
    age INT,
    balance NUMERIC(10,2)
);

-- Add PRIMARY KEY
ALTER TABLE test_constraint_changes ADD PRIMARY KEY (id);

-- Add UNIQUE constraint
ALTER TABLE test_constraint_changes ADD CONSTRAINT unique_email UNIQUE (email);

-- Add CHECK constraint
ALTER TABLE test_constraint_changes
ADD CONSTRAINT check_age CHECK (age >= 18);

-- Add NOT NULL constraint
ALTER TABLE test_constraint_changes ALTER COLUMN email SET NOT NULL;

-- Query constraints
SELECT
    conname AS constraint_name,
    contype AS constraint_type,
    pg_get_constraintdef(oid) AS definition
FROM pg_constraint
WHERE conrelid = 'test_constraint_changes'::regclass
ORDER BY conname;

-- Drop constraint
ALTER TABLE test_constraint_changes DROP CONSTRAINT check_age;

-- Drop NOT NULL
ALTER TABLE test_constraint_changes ALTER COLUMN email DROP NOT NULL;

-- ============================================================================
-- Section 5: ALTER TABLE ADD/DROP Foreign Key
-- ============================================================================

CREATE TABLE test_fk_parent (
    parent_id SERIAL PRIMARY KEY,
    name VARCHAR(100)
);

CREATE TABLE test_fk_child (
    child_id SERIAL PRIMARY KEY,
    parent_id INT,
    data TEXT
);

-- Add foreign key
ALTER TABLE test_fk_child
ADD CONSTRAINT fk_parent
FOREIGN KEY (parent_id)
REFERENCES test_fk_parent(parent_id)
ON DELETE CASCADE
ON UPDATE RESTRICT;

-- Verify FK
SELECT
    conname AS constraint_name,
    pg_get_constraintdef(oid) AS definition
FROM pg_constraint
WHERE conrelid = 'test_fk_child'::regclass
  AND contype = 'f';

-- Drop foreign key
ALTER TABLE test_fk_child DROP CONSTRAINT fk_parent;

-- ============================================================================
-- Section 6: ALTER TABLE ENABLE/DISABLE TRIGGER
-- ============================================================================

CREATE TABLE test_trigger_enable (
    id SERIAL PRIMARY KEY,
    value INT,
    updated_at TIMESTAMP
);

CREATE FUNCTION update_timestamp() RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at := CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_update_time
BEFORE UPDATE ON test_trigger_enable
FOR EACH ROW
EXECUTE FUNCTION update_timestamp();

-- Disable trigger
ALTER TABLE test_trigger_enable DISABLE TRIGGER trg_update_time;

-- Enable trigger
ALTER TABLE test_trigger_enable ENABLE TRIGGER trg_update_time;

-- Disable all triggers
ALTER TABLE test_trigger_enable DISABLE TRIGGER ALL;

-- Enable all triggers
ALTER TABLE test_trigger_enable ENABLE TRIGGER ALL;

-- Query trigger status
SELECT
    tgname AS trigger_name,
    tgenabled AS status
FROM pg_trigger
WHERE tgrelid = 'test_trigger_enable'::regclass
  AND tgname = 'trg_update_time';

-- ============================================================================
-- Section 7: ALTER TABLE INHERIT/NO INHERIT
-- ============================================================================

CREATE TABLE test_parent_inherit (
    id SERIAL PRIMARY KEY,
    common_field VARCHAR(100)
);

CREATE TABLE test_child_inherit (
    child_id SERIAL,
    child_field TEXT
);

-- Add inheritance
ALTER TABLE test_child_inherit INHERIT test_parent_inherit;

-- Query inheritance
SELECT
    c.relname AS child_table,
    p.relname AS parent_table
FROM pg_inherits i
JOIN pg_class c ON i.inhrelid = c.oid
JOIN pg_class p ON i.inhparent = p.oid
WHERE c.relname = 'test_child_inherit';

-- Remove inheritance
ALTER TABLE test_child_inherit NO INHERIT test_parent_inherit;

-- ============================================================================
-- Section 8: ALTER TABLE REPLICA IDENTITY
-- ============================================================================

CREATE TABLE test_replica_identity (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50) UNIQUE,
    data TEXT
);

-- Set replica identity to DEFAULT (uses primary key)
ALTER TABLE test_replica_identity REPLICA IDENTITY DEFAULT;

-- Set to use index
ALTER TABLE test_replica_identity
REPLICA IDENTITY USING INDEX test_replica_identity_code_key;

-- Set to FULL (all columns)
ALTER TABLE test_replica_identity REPLICA IDENTITY FULL;

-- Set to NOTHING (only transaction ID)
ALTER TABLE test_replica_identity REPLICA IDENTITY NOTHING;

-- Query replica identity
SELECT
    c.relname AS table_name,
    CASE c.relreplident
        WHEN 'd' THEN 'DEFAULT'
        WHEN 'i' THEN 'INDEX'
        WHEN 'f' THEN 'FULL'
        WHEN 'n' THEN 'NOTHING'
    END AS replica_identity
FROM pg_class c
WHERE c.relname = 'test_replica_identity';

-- ============================================================================
-- Section 9: ALTER TABLE CLUSTER ON
-- ============================================================================

CREATE TABLE test_cluster_table (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    value INT
);

CREATE INDEX idx_cluster_category ON test_cluster_table(category);

INSERT INTO test_cluster_table (category, value)
SELECT
    CASE (i % 3)
        WHEN 0 THEN 'A'
        WHEN 1 THEN 'B'
        ELSE 'C'
    END,
    i
FROM generate_series(1, 1000) i;

-- Set clustering index
ALTER TABLE test_cluster_table CLUSTER ON idx_cluster_category;

-- Physical reorder (CLUSTER command, not ALTER)
-- CLUSTER test_cluster_table;

-- Query cluster index
SELECT
    c.relname AS table_name,
    i.relname AS cluster_index
FROM pg_class c
JOIN pg_index idx ON c.oid = idx.indrelid
JOIN pg_class i ON idx.indexrelid = i.oid
WHERE idx.indisclustered = true
  AND c.relname = 'test_cluster_table';

-- Remove cluster setting
ALTER TABLE test_cluster_table SET WITHOUT CLUSTER;

-- ============================================================================
-- Section 10: ALTER TABLE SET/RESET Storage Parameters
-- ============================================================================

CREATE TABLE test_storage_alter (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Set storage parameters
ALTER TABLE test_storage_alter SET (
    fillfactor = 70,
    autovacuum_enabled = true,
    autovacuum_vacuum_threshold = 100
);

-- Query parameters
SELECT
    c.relname AS table_name,
    c.reloptions AS storage_parameters
FROM pg_class c
WHERE c.relname = 'test_storage_alter';

-- Reset parameter
ALTER TABLE test_storage_alter RESET (fillfactor);

-- Query updated parameters
SELECT reloptions FROM pg_class WHERE relname = 'test_storage_alter';

-- ============================================================================
-- Section 11: ALTER SEQUENCE RESTART/INCREMENT
-- ============================================================================

CREATE SEQUENCE test_alter_sequence START WITH 1 INCREMENT BY 1;

-- Set next value
SELECT nextval('test_alter_sequence');  -- Returns 1
SELECT nextval('test_alter_sequence');  -- Returns 2

-- Restart sequence
ALTER SEQUENCE test_alter_sequence RESTART WITH 100;

SELECT nextval('test_alter_sequence');  -- Returns 100

-- Change increment
ALTER SEQUENCE test_alter_sequence INCREMENT BY 5;

SELECT nextval('test_alter_sequence');  -- Returns 105

-- Change min/max values
ALTER SEQUENCE test_alter_sequence
    MINVALUE 1
    MAXVALUE 1000
    CYCLE;

-- Query sequence properties
SELECT
    last_value,
    start_value,
    increment_by,
    max_value,
    min_value,
    cycle
FROM test_alter_sequence;

-- ============================================================================
-- Section 12: ALTER SEQUENCE OWNED BY
-- ============================================================================

CREATE TABLE test_seq_owner (
    id INT PRIMARY KEY,
    value INT
);

CREATE SEQUENCE test_manual_sequence;

-- Set sequence ownership
ALTER SEQUENCE test_manual_sequence OWNED BY test_seq_owner.id;

-- Query ownership
SELECT
    s.relname AS sequence_name,
    t.relname AS owner_table,
    a.attname AS owner_column
FROM pg_class s
JOIN pg_depend d ON s.oid = d.objid
JOIN pg_class t ON d.refobjid = t.oid
JOIN pg_attribute a ON d.refobjid = a.attrelid AND d.refobjsubid = a.attnum
WHERE s.relkind = 'S'
  AND s.relname = 'test_manual_sequence';

-- ============================================================================
-- Section 13: ALTER INDEX RENAME/SET TABLESPACE
-- ============================================================================

CREATE TABLE test_index_alter (
    id SERIAL PRIMARY KEY,
    data TEXT
);

CREATE INDEX idx_old_name ON test_index_alter(data);

-- Rename index
ALTER INDEX idx_old_name RENAME TO idx_new_name;

-- Verify rename
SELECT indexname FROM pg_indexes WHERE tablename = 'test_index_alter';

-- Move index to tablespace
-- ALTER INDEX idx_new_name SET TABLESPACE pg_default;

-- ============================================================================
-- Section 14: ALTER VIEW
-- ============================================================================

CREATE TABLE test_view_base (
    id SERIAL PRIMARY KEY,
    value INT,
    status VARCHAR(50)
);

CREATE VIEW test_alterable_view AS
SELECT id, value FROM test_view_base;

-- Rename view
ALTER VIEW test_alterable_view RENAME TO test_renamed_view;

-- Verify rename
SELECT table_name FROM information_schema.views WHERE table_name LIKE 'test_%view';

-- Change view owner
ALTER VIEW test_renamed_view OWNER TO test_new_owner;

-- Set view schema
ALTER VIEW test_renamed_view SET SCHEMA test_schema_new;

-- ============================================================================
-- Section 15: ALTER FUNCTION/PROCEDURE
-- ============================================================================

CREATE FUNCTION test_alter_function(p_value INT)
RETURNS INT AS $$
BEGIN
    RETURN p_value * 2;
END;
$$ LANGUAGE plpgsql;

-- Rename function
ALTER FUNCTION test_alter_function(INT) RENAME TO test_renamed_function;

-- Change owner
ALTER FUNCTION test_renamed_function(INT) OWNER TO test_new_owner;

-- Change schema
ALTER FUNCTION test_renamed_function(INT) SET SCHEMA test_schema_new;

-- Set function properties
ALTER FUNCTION test_schema_new.test_renamed_function(INT)
    IMMUTABLE
    PARALLEL SAFE
    COST 10;

-- Query function properties
SELECT
    p.proname AS function_name,
    CASE p.provolatile
        WHEN 'i' THEN 'IMMUTABLE'
        WHEN 's' THEN 'STABLE'
        WHEN 'v' THEN 'VOLATILE'
    END AS volatility,
    p.proparallel AS parallel,
    p.procost AS cost
FROM pg_proc p
WHERE p.proname = 'test_renamed_function';

-- ============================================================================
-- Section 16: ALTER TYPE (Add Enum Value)
-- ============================================================================

CREATE TYPE test_alter_enum AS ENUM ('small', 'large');

-- Add enum value at end
ALTER TYPE test_alter_enum ADD VALUE 'medium';

-- Add enum value before existing
ALTER TYPE test_alter_enum ADD VALUE 'tiny' BEFORE 'small';

-- Add enum value after existing
ALTER TYPE test_alter_enum ADD VALUE 'huge' AFTER 'large';

-- Query enum values
SELECT enumlabel
FROM pg_enum
WHERE enumtypid = 'test_alter_enum'::regtype
ORDER BY enumsortorder;

-- ============================================================================
-- Section 17: ALTER DOMAIN
-- ============================================================================

CREATE DOMAIN test_alter_domain AS VARCHAR(50);

-- Add constraint
ALTER DOMAIN test_alter_domain ADD CONSTRAINT min_length CHECK (LENGTH(VALUE) >= 3);

-- Set default
ALTER DOMAIN test_alter_domain SET DEFAULT 'default_value';

-- Set NOT NULL
ALTER DOMAIN test_alter_domain SET NOT NULL;

-- Rename constraint
ALTER DOMAIN test_alter_domain RENAME CONSTRAINT min_length TO minimum_length;

-- Query domain constraints
SELECT
    conname AS constraint_name,
    pg_get_constraintdef(oid) AS definition
FROM pg_constraint
WHERE contypid = 'test_alter_domain'::regtype;

-- Drop constraint
ALTER DOMAIN test_alter_domain DROP CONSTRAINT minimum_length;

-- Drop NOT NULL
ALTER DOMAIN test_alter_domain DROP NOT NULL;

-- ============================================================================
-- Section 18: ALTER COLLATION
-- ============================================================================

-- Query collations
SELECT
    collname,
    collprovider
FROM pg_collation
WHERE collname LIKE 'en_%'
ORDER BY collname
LIMIT 5;

-- Rename collation (requires ownership)
-- ALTER COLLATION "en_US" RENAME TO "en_US_renamed";

-- ============================================================================
-- Section 19: ALTER TABLE Partition Operations
-- ============================================================================

CREATE TABLE test_partition_alter (
    id SERIAL,
    category VARCHAR(50),
    value INT,
    PRIMARY KEY (id, category)
) PARTITION BY LIST (category);

CREATE TABLE test_part_a PARTITION OF test_partition_alter
    FOR VALUES IN ('A');

-- Add new partition
CREATE TABLE test_part_b PARTITION OF test_partition_alter
    FOR VALUES IN ('B');

-- Detach partition
ALTER TABLE test_partition_alter DETACH PARTITION test_part_b;

-- Reattach partition
ALTER TABLE test_partition_alter ATTACH PARTITION test_part_b
    FOR VALUES IN ('B');

-- Query partitions
SELECT
    parent.relname AS parent_table,
    child.relname AS partition_name
FROM pg_inherits i
JOIN pg_class parent ON i.inhparent = parent.oid
JOIN pg_class child ON i.inhrelid = child.oid
WHERE parent.relname = 'test_partition_alter'
ORDER BY child.relname;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_alter_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_alter_best_practices VALUES
    (1, 'Test ALTER operations in development before production'),
    (2, 'ALTER TABLE operations may lock table (use caution)'),
    (3, 'Use CONCURRENTLY where supported (indexes, etc.)'),
    (4, 'Backup before major ALTER operations'),
    (5, 'ALTER TABLE ADD COLUMN with DEFAULT can be expensive'),
    (6, 'Consider downtime window for large table alterations'),
    (7, 'SET SCHEMA moves object and dependencies'),
    (8, 'Changing ownership affects privileges'),
    (9, 'DISABLE TRIGGER ALL affects all triggers on table'),
    (10, 'CLUSTER physically reorders data (locks table)'),
    (11, 'Storage parameters affect performance (test changes)'),
    (12, 'ALTER SEQUENCE RESTART useful for data fixes'),
    (13, 'Enum value addition is not transactional'),
    (14, 'Cannot remove enum values (only add)'),
    (15, 'ALTER DOMAIN affects all columns using domain'),
    (16, 'Partition operations may require table locks'),
    (17, 'DETACH PARTITION can be done CONCURRENTLY (PG 14+)'),
    (18, 'Monitor long-running ALTER operations'),
    (19, 'Some ALTERs require rewriting entire table'),
    (20, 'Document schema changes in migration scripts');

SELECT id, guideline FROM test_alter_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_alter_best_practices;
DROP TABLE test_part_b;
DROP TABLE test_part_a;
DROP TABLE test_partition_alter;
DROP DOMAIN test_alter_domain;
DROP TYPE test_alter_enum;
DROP TABLE test_view_base;
DROP TABLE test_index_alter;
DROP SEQUENCE test_manual_sequence;
DROP TABLE test_seq_owner;
DROP SEQUENCE test_alter_sequence;
DROP TABLE test_storage_alter;
DROP TABLE test_cluster_table;
DROP TABLE test_replica_identity;
DROP TABLE test_child_inherit;
DROP TABLE test_parent_inherit;
DROP TABLE test_trigger_enable;
DROP FUNCTION update_timestamp();
DROP TABLE test_fk_child;
DROP TABLE test_fk_parent;
DROP TABLE test_constraint_changes;
DROP TABLE test_owner_change;
DROP ROLE test_new_owner;
DROP TABLE test_tablespace_move;
DROP SCHEMA test_schema_new CASCADE;
DROP SCHEMA test_schema_old CASCADE;

DROP DATABASE test_alter_operations_db;

-- End of ALTER operations tests
