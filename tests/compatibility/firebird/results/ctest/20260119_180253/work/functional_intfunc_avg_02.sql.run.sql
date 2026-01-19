-- === PRELUDE SCRIPT ===
CREATE DATABASE '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260119_180253/work/functional_intfunc_avg_02.sql.fdb';
CONNECT '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260119_180253/work/functional_intfunc_avg_02.sql.fdb';
-- === INIT SCRIPT ===
CREATE TABLE test( id INTEGER NOT NULL);
INSERT INTO test VALUES(5);
INSERT INTO test VALUES(6);

-- === TEST SCRIPT ===
SELECT AVG(id) FROM test;
