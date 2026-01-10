-- === PRELUDE SCRIPT ===
CREATE DATABASE '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260110_111813/work/functional_dml_delete_02.sql.fdb';
CONNECT '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260110_111813/work/functional_dml_delete_02.sql.fdb';
-- Test ID: functional.dml.delete.02
-- Title: DELETE with WHERE
-- Firebird Version: 1.0
-- Platform: All
-- Test Type: ISQL
-- Converted from: delete_02.fbt

-- === INIT SCRIPT ===
CREATE TABLE tb(id INT);
INSERT INTO tb VALUES(10);
INSERT INTO tb VALUES(10);
INSERT INTO tb VALUES(20);
COMMIT;

-- === TEST SCRIPT ===
DELETE FROM tb WHERE id>10;
SELECT * FROM tb;
