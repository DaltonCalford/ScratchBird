-- === PRELUDE SCRIPT ===
CREATE DATABASE '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260110_121336/work/functional_dml_delete_01.sql.fdb';
CONNECT '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260110_121336/work/functional_dml_delete_01.sql.fdb';
-- === INIT SCRIPT ===
CREATE TABLE tb(id INT);
INSERT INTO tb VALUES(10);
COMMIT;

-- === TEST SCRIPT ===
DELETE FROM tb;
SELECT * FROM tb;
