-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Transactions - RELEASE SAVEPOINT
-- Description: Focused RELEASE SAVEPOINT behavior
-- ============================================================================

CREATE DATABASE test_release_savepoint_db;
USE test_release_savepoint_db;

CREATE TABLE test_release_sp (
    id INT,
    note TEXT
);

BEGIN;

INSERT INTO test_release_sp (id, note) VALUES (1, 'before');
SAVEPOINT sp_release;

INSERT INTO test_release_sp (id, note) VALUES (2, 'after');
RELEASE SAVEPOINT sp_release;

DO $$
BEGIN
    ROLLBACK TO SAVEPOINT sp_release;
EXCEPTION
    WHEN OTHERS THEN
        RAISE NOTICE 'Expected failure: savepoint released';
END $$;

COMMIT;

SELECT id, note FROM test_release_sp ORDER BY id;

DROP TABLE test_release_sp;
DROP DATABASE test_release_savepoint_db;
