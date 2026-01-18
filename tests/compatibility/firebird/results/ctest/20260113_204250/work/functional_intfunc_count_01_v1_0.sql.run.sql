-- === PRELUDE SCRIPT ===
CREATE DATABASE '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260113_204250/work/functional_intfunc_count_01_v1_0.sql.fdb';
CONNECT '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260113_204250/work/functional_intfunc_count_01_v1_0.sql.fdb';
-- === INIT SCRIPT ===
CREATE TABLE test( id INTEGER);

-- === TEST SCRIPT ===
SELECT COUNT(*) FROM test;
