-- === PRELUDE SCRIPT ===
CREATE DATABASE '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260112_183112/work/functional_intfunc_avg_01.sql.fdb';
CONNECT '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260112_183112/work/functional_intfunc_avg_01.sql.fdb';
-- === INIT SCRIPT ===
CREATE TABLE test( id INTEGER NOT NULL CONSTRAINT unq UNIQUE,
                   text VARCHAR(32));
INSERT INTO test VALUES(5,null);

-- === TEST SCRIPT ===
SELECT AVG(id) FROM test;
