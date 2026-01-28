-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DML - COPY Operations
-- Description: COPY option parsing and CSV/TEXT paths
-- ============================================================================

CREATE DATABASE test_copy_operations_db;
USE test_copy_operations_db;

CREATE TABLE copy_target (
    id INT,
    name TEXT,
    amount INT
);

INSERT INTO copy_target (id, name, amount) VALUES
    (1, 'alpha', 10),
    (2, 'beta', 20),
    (3, 'gamma', 30);

-- CSV export with full option set
COPY copy_target TO '/tmp/scratchbird_copy_out.csv'
WITH (
    FORMAT CSV,
    HEADER true,
    DELIMITER ',',
    NULL 'NULL',
    QUOTE '"',
    ESCAPE '"',
    ENCODING 'UTF8',
    BATCH_SIZE 2,
    MAX_ERRORS 1,
    ON_ERROR SKIP
);

TRUNCATE TABLE copy_target;

-- CSV import with full option set
COPY copy_target FROM '/tmp/scratchbird_copy_out.csv'
WITH (
    FORMAT CSV,
    HEADER true,
    DELIMITER ',',
    NULL 'NULL',
    QUOTE '"',
    ESCAPE '"',
    ENCODING 'UTF8',
    BATCH_SIZE 2,
    MAX_ERRORS 1,
    ON_ERROR SKIP
);

SELECT * FROM copy_target ORDER BY id;

-- TEXT export
COPY copy_target TO '/tmp/scratchbird_copy_out.txt'
WITH (
    FORMAT TEXT,
    DELIMITER '\t',
    NULL '\\N',
    ENCODING 'UTF8',
    BATCH_SIZE 3,
    MAX_ERRORS 0,
    ON_ERROR ABORT
);

DROP TABLE copy_target;
DROP DATABASE test_copy_operations_db;
