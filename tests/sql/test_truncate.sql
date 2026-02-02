-- ScratchBird
-- Copyright (c) 2025-2026 Dalton Calford
--
-- Licensed under the Initial Developer's Public License Version 1.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at:
-- https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/

-- Test TRUNCATE TABLE ASYNC implementation
-- This tests the new DDL operation for ScratchBird

-- Create a test table
CREATE TABLE test_truncate (
    id INTEGER,
    name VARCHAR(100),
    value INTEGER
);

-- Insert some test data
INSERT INTO test_truncate VALUES (1, 'Alice', 100);
INSERT INTO test_truncate VALUES (2, 'Bob', 200);
INSERT INTO test_truncate VALUES (3, 'Charlie', 300);
INSERT INTO test_truncate VALUES (4, 'David', 400);
INSERT INTO test_truncate VALUES (5, 'Eve', 500);

-- Verify data exists
SELECT * FROM test_truncate;

-- Test TRUNCATE TABLE ASYNC (default mode)
TRUNCATE TABLE test_truncate;

-- Wait a moment for async operation to complete
-- (In production, would query job status)

-- Test TRUNCATE TABLE SYNC (blocks until complete)
-- First insert more data
INSERT INTO test_truncate VALUES (6, 'Frank', 600);
INSERT INTO test_truncate VALUES (7, 'Grace', 700);

-- Truncate synchronously
TRUNCATE TABLE test_truncate SYNC;

-- Verify table is empty
SELECT * FROM test_truncate;

-- Test with explicit ASYNC keyword
INSERT INTO test_truncate VALUES (8, 'Henry', 800);
TRUNCATE TABLE test_truncate ASYNC;

-- Clean up
-- DROP TABLE test_truncate;
