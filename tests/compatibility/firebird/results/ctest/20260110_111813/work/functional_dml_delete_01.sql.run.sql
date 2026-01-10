-- === PRELUDE SCRIPT ===
CREATE DATABASE '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260110_111813/work/functional_dml_delete_01.sql.fdb';
CONNECT '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260110_111813/work/functional_dml_delete_01.sql.fdb';
-- Test ID: functional.dml.delete.01
-- Title: DELETE
-- Firebird Version: 1.0
-- Platform: All
-- Test Type: ISQL
-- Converted from: delete_01.fbt

-- === INIT SCRIPT ===
CREATE TABLE tb(id INT);
INSERT INTO tb VALUES(10);
COMMIT;

-- === TEST SCRIPT ===
DELETE FROM tb;
SELECT * FROM tb;
