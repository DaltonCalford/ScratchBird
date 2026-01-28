-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DML - COPY Error Skip
-- Description: COPY ON_ERROR SKIP with MAX_ERRORS edge cases
-- ============================================================================

CREATE DATABASE test_copy_error_skip_db;
USE test_copy_error_skip_db;

CREATE TABLE copy_source (
    id INT,
    name TEXT,
    amount INT
);

INSERT INTO copy_source (id, name, amount) VALUES
    (1, 'ok', 10),
    (2, 'NULL', -5),
    (3, 'good', 20),
    (4, 'NULL', 30),
    (5, 'ok', -1);

-- Export data to CSV with NULL marker
COPY copy_source TO '/tmp/scratchbird_copy_error_skip.csv'
WITH (
    FORMAT CSV,
    HEADER true,
    NULL 'NULL'
);

CREATE TABLE copy_target (
    id INT,
    name TEXT NOT NULL,
    amount INT CHECK (amount >= 0)
);

-- ============================================================================
-- Section 1: Skip bad rows within MAX_ERRORS
-- ============================================================================

COPY copy_target FROM '/tmp/scratchbird_copy_error_skip.csv'
WITH (
    FORMAT CSV,
    HEADER true,
    NULL 'NULL',
    MAX_ERRORS 3,
    ON_ERROR SKIP
);

SELECT id, name, amount
FROM copy_target
ORDER BY id;

-- ============================================================================
-- Section 2: Exceed MAX_ERRORS
-- ============================================================================

TRUNCATE TABLE copy_target;

DO $$
BEGIN
    EXECUTE STATEMENT
        'COPY copy_target FROM ''/tmp/scratchbird_copy_error_skip.csv'' ' ||
        'WITH (FORMAT CSV, HEADER true, NULL ''NULL'', MAX_ERRORS 1, ON_ERROR SKIP)';
EXCEPTION
    WHEN OTHERS THEN
        RAISE NOTICE 'Expected COPY MAX_ERRORS exceeded';
END $$;

DROP TABLE copy_target;
DROP TABLE copy_source;
DROP DATABASE test_copy_error_skip_db;
