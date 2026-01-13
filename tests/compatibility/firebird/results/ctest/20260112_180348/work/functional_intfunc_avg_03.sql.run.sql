-- === PRELUDE SCRIPT ===
CREATE DATABASE '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260112_180348/work/functional_intfunc_avg_03.sql.fdb';
CONNECT '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260112_180348/work/functional_intfunc_avg_03.sql.fdb';
-- === INIT SCRIPT ===
CREATE TABLE test( id INTEGER NOT NULL);
INSERT INTO test VALUES(5);
INSERT INTO test VALUES(5);
INSERT INTO test VALUES(6);

-- === TEST SCRIPT ===
SELECT AVG(id) FROM test;
