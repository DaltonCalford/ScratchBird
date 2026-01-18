-- === PRELUDE SCRIPT ===
CREATE DATABASE '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260118_152429/work/functional_intfunc_avg_08.sql.fdb';
CONNECT '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260118_152429/work/functional_intfunc_avg_08.sql.fdb';
-- === INIT SCRIPT ===
CREATE TABLE test( id INTEGER);
INSERT INTO test VALUES(NULL);

-- === TEST SCRIPT ===
SELECT AVG(id) FROM test;
