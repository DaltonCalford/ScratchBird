-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DDL - CREATE TYPE Edge Cases
-- Description: CREATE TYPE error paths and option coverage
-- ============================================================================

CREATE DATABASE test_create_type_edge_cases_db;
USE test_create_type_edge_cases_db;

-- ============================================================================
-- Section 1: IF NOT EXISTS + ENUM ordering
-- ============================================================================

CREATE TYPE test_enum_if_not_exists AS ENUM ('a', 'b');
CREATE TYPE IF NOT EXISTS test_enum_if_not_exists AS ENUM ('a', 'b');

CREATE TYPE test_enum_positions AS ENUM ('low' = 1, 'high' = 3, 'medium' = 2);

SELECT enumlabel, enumsortorder
FROM pg_enum
WHERE enumtypid = 'test_enum_positions'::regtype
ORDER BY enumsortorder;

-- ============================================================================
-- Section 2: ENUM duplicate label error
-- ============================================================================

DO $$
BEGIN
    EXECUTE STATEMENT 'CREATE TYPE test_enum_dup AS ENUM (''x'', ''x'')';
EXCEPTION
    WHEN OTHERS THEN
        RAISE NOTICE 'Expected duplicate ENUM label failure';
END $$;

-- ============================================================================
-- Section 3: RECORD duplicate field / empty record errors
-- ============================================================================

DO $$
BEGIN
    EXECUTE STATEMENT 'CREATE TYPE test_record_dup AS (a INT, a INT)';
EXCEPTION
    WHEN OTHERS THEN
        RAISE NOTICE 'Expected duplicate RECORD field failure';
END $$;

DO $$
BEGIN
    EXECUTE STATEMENT 'CREATE TYPE test_record_empty AS ()';
EXCEPTION
    WHEN OTHERS THEN
        RAISE NOTICE 'Expected empty RECORD failure';
END $$;

-- ============================================================================
-- Section 4: BASE type missing INPUT/OUTPUT error
-- ============================================================================

DO $$
BEGIN
    EXECUTE STATEMENT 'CREATE TYPE test_base_missing_io AS BASE (STORAGE=VARBINARY(16), INPUT=base_in)';
EXCEPTION
    WHEN OTHERS THEN
        RAISE NOTICE 'Expected BASE type missing OUTPUT failure';
END $$;

CREATE TYPE test_base_valid AS BASE (
    STORAGE = VARBINARY(16),
    INPUT = base_in,
    OUTPUT = base_out,
    ALIGNMENT = INT,
    STORAGE_MODE = MAIN,
    CATEGORY = 'N',
    PREFERRED = TRUE
);

-- ============================================================================
-- Section 5: RANGE + SHELL types
-- ============================================================================

CREATE TYPE test_range_money AS RANGE (
    SUBTYPE = NUMERIC(10, 2),
    SUBTYPE_DIFF = numrange_subtype_diff,
    CANONICAL = numrange_canonical,
    MULTIRANGE = TRUE
);

CREATE TYPE test_shell_type AS SHELL;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TYPE test_shell_type;
DROP TYPE test_range_money;
DROP TYPE test_base_valid;
DROP TYPE test_enum_positions;
DROP TYPE test_enum_if_not_exists;
DROP DATABASE test_create_type_edge_cases_db;
