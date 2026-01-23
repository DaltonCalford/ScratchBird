-- === PRELUDE SCRIPT ===
CREATE DATABASE '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260122_205139/work/functional_intfunc_avg_07.sql.fdb';
CONNECT '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260122_205139/work/functional_intfunc_avg_07.sql.fdb';
-- === INIT SCRIPT ===
CREATE TABLE test( id INTEGER);
INSERT INTO test VALUES(12);
INSERT INTO test VALUES(13);
INSERT INTO test VALUES(14);
INSERT INTO test VALUES(NULL);

-- === TEST SCRIPT ===
SELECT AVG(id) FROM test;
