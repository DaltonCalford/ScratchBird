-- === PRELUDE SCRIPT ===
CREATE DATABASE '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260122_144338/work/functional_dml_delete_02.sql.fdb';
CONNECT '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260122_144338/work/functional_dml_delete_02.sql.fdb';
-- === INIT SCRIPT ===
CREATE TABLE tb(id INT);
INSERT INTO tb VALUES(10);
INSERT INTO tb VALUES(10);
INSERT INTO tb VALUES(20);
COMMIT;

-- === TEST SCRIPT ===
DELETE FROM tb WHERE id>10;
SELECT * FROM tb;
