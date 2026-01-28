-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DDL - COMMENT ON reset paths
-- Description: COMMENT ON ... IS NULL removes existing comments
-- ============================================================================

CREATE DATABASE test_comment_reset_db;
USE test_comment_reset_db;

CREATE TABLE comment_reset_table (
    id INT,
    name TEXT
);

COMMENT ON TABLE comment_reset_table IS 'table comment';
COMMENT ON TABLE comment_reset_table IS NULL;

COMMENT ON COLUMN comment_reset_table.id IS 'id comment';
COMMENT ON COLUMN comment_reset_table.id IS NULL;

DROP TABLE comment_reset_table;
DROP DATABASE test_comment_reset_db;
