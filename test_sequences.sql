-- Test SEQUENCE Implementation
-- This tests the new DDL operations and functions for ScratchBird
-- ALPHA Phase 1 - Sequences

-- ===== Test 1: CREATE SEQUENCE with defaults =====
CREATE SEQUENCE seq1;

-- Expected: seq1 created with defaults (increment=1, start=1)

-- ===== Test 2: CREATE SEQUENCE with custom parameters =====
CREATE SEQUENCE seq2
    INCREMENT BY 5
    MINVALUE 10
    MAXVALUE 50
    START WITH 10
    CACHE 1
    CYCLE;

-- Expected: seq2 created with custom parameters

-- ===== Test 3: CREATE SEQUENCE with START =====
CREATE SEQUENCE seq3
    START WITH 100;

-- Expected: seq3 created, starts at 100

-- ===== Test 4: CREATE SEQUENCE with NO MINVALUE/MAXVALUE =====
CREATE SEQUENCE seq4
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE;

-- Expected: seq4 created with no bounds

-- ===== Test 5: CREATE SEQUENCE with negative increment =====
CREATE SEQUENCE seq5
    INCREMENT BY -1
    MINVALUE 1
    MAXVALUE 100
    START WITH 100;

-- Expected: seq5 counts down from 100

-- ===== Test 6: NEXTVAL function (when fully implemented) =====
-- SELECT NEXTVAL('seq1');  -- Should return 1
-- SELECT NEXTVAL('seq1');  -- Should return 2
-- SELECT NEXTVAL('seq1');  -- Should return 3

-- ===== Test 7: NEXTVAL with custom increment (when fully implemented) =====
-- SELECT NEXTVAL('seq2');  -- Should return 10
-- SELECT NEXTVAL('seq2');  -- Should return 15
-- SELECT NEXTVAL('seq2');  -- Should return 20

-- ===== Test 8: CURRVAL function (when fully implemented) =====
-- SELECT NEXTVAL('seq3');  -- Should return 100
-- SELECT CURRVAL('seq3');  -- Should return 100 (last value from this session)
-- SELECT CURRVAL('seq3');  -- Should return 100 (unchanged)

-- ===== Test 9: SETVAL function (when fully implemented) =====
-- SELECT SETVAL('seq1', 50);      -- Set to 50, mark as called
-- SELECT NEXTVAL('seq1');          -- Should return 51

-- SELECT SETVAL('seq1', 100, false); -- Set to 100, mark as not called
-- SELECT NEXTVAL('seq1');          -- Should return 100 (not 101)

-- ===== Test 10: Sequence cycle (when fully implemented) =====
-- SELECT NEXTVAL('seq2');  -- 10
-- SELECT NEXTVAL('seq2');  -- 15
-- SELECT NEXTVAL('seq2');  -- 20
-- SELECT NEXTVAL('seq2');  -- 25
-- SELECT NEXTVAL('seq2');  -- 30
-- SELECT NEXTVAL('seq2');  -- 35
-- SELECT NEXTVAL('seq2');  -- 40
-- SELECT NEXTVAL('seq2');  -- 45
-- SELECT NEXTVAL('seq2');  -- 50 (max reached)
-- SELECT NEXTVAL('seq2');  -- 10 (cycles back to min)

-- ===== Test 11: Negative increment (when fully implemented) =====
-- SELECT NEXTVAL('seq5');  -- 100
-- SELECT NEXTVAL('seq5');  -- 99
-- SELECT NEXTVAL('seq5');  -- 98

-- ===== Test 12: ALTER SEQUENCE - INCREMENT BY (when fully implemented) =====
-- ALTER SEQUENCE seq1 INCREMENT BY 10;
-- SELECT NEXTVAL('seq1');  -- Should increment by 10

-- ===== Test 13: ALTER SEQUENCE - RESTART (when fully implemented) =====
-- ALTER SEQUENCE seq1 RESTART WITH 200;
-- SELECT NEXTVAL('seq1');  -- Should return 200

-- ===== Test 14: ALTER SEQUENCE - MINVALUE/MAXVALUE (when fully implemented) =====
-- ALTER SEQUENCE seq1 MINVALUE 1 MAXVALUE 1000;

-- ===== Test 15: ALTER SEQUENCE - CYCLE/NO CYCLE (when fully implemented) =====
-- ALTER SEQUENCE seq1 CYCLE;
-- ALTER SEQUENCE seq1 NO CYCLE;

-- ===== Test 16: DROP SEQUENCE =====
-- DROP SEQUENCE seq1;
-- DROP SEQUENCE IF EXISTS seq1;  -- Should not error

-- ===== Test 17: DROP SEQUENCE CASCADE (when fully implemented) =====
-- CREATE TABLE test_table (
--     id INTEGER DEFAULT NEXTVAL('seq2'),
--     name VARCHAR(100)
-- );
-- DROP SEQUENCE seq2 RESTRICT;  -- Should ERROR: dependent objects exist
-- DROP SEQUENCE seq2 CASCADE;   -- Should succeed, remove DEFAULT

-- ===== Test 18: Error handling - CURRVAL before NEXTVAL (when fully implemented) =====
-- CREATE SEQUENCE seq_err1;
-- SELECT CURRVAL('seq_err1');  -- ERROR: not yet defined in this session

-- ===== Test 19: Error handling - Sequence exhausted NO CYCLE (when fully implemented) =====
-- CREATE SEQUENCE seq_err2
--     INCREMENT BY 1
--     MINVALUE 1
--     MAXVALUE 3
--     START WITH 1
--     NO CYCLE;
-- SELECT NEXTVAL('seq_err2');  -- 1
-- SELECT NEXTVAL('seq_err2');  -- 2
-- SELECT NEXTVAL('seq_err2');  -- 3
-- SELECT NEXTVAL('seq_err2');  -- ERROR: sequence has reached its maximum value

-- ===== Test 20: Error handling - Invalid parameters (when fully implemented) =====
-- CREATE SEQUENCE seq_err3
--     MINVALUE 10
--     MAXVALUE 5;  -- ERROR: MINVALUE must be less than MAXVALUE

-- CREATE SEQUENCE seq_err4
--     START WITH 100
--     MINVALUE 1
--     MAXVALUE 50;  -- ERROR: START value cannot be greater than MAXVALUE

-- ===== Clean up =====
DROP SEQUENCE IF EXISTS seq1;
DROP SEQUENCE IF EXISTS seq2;
DROP SEQUENCE IF EXISTS seq3;
DROP SEQUENCE IF EXISTS seq4;
DROP SEQUENCE IF EXISTS seq5;

-- ===== Notes =====
-- This test file includes:
-- 1. CREATE SEQUENCE with various parameter combinations
-- 2. NEXTVAL/CURRVAL/SETVAL functions (commented out until name-to-ID lookup implemented)
-- 3. ALTER SEQUENCE with all supported options
-- 4. DROP SEQUENCE with IF EXISTS and CASCADE
-- 5. Cycle behavior testing
-- 6. Negative increment testing
-- 7. Error handling scenarios
--
-- Most SELECT tests are commented out because they require the executor stub methods
-- to be completed (name-to-ID lookup in catalog). Once that's implemented, uncomment
-- these tests to verify full functionality.
--
-- Status: CREATE SEQUENCE works NOW (fully functional)
--         ALTER/DROP/NEXTVAL/CURRVAL/SETVAL need name-to-ID mapping
