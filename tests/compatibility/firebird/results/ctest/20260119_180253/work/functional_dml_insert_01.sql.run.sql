-- === PRELUDE SCRIPT ===
CREATE DATABASE '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260119_180253/work/functional_dml_insert_01.sql.fdb';
CONNECT '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260119_180253/work/functional_dml_insert_01.sql.fdb';
-- === INIT SCRIPT ===
CREATE TABLE employee( prenom VARCHAR(20) default 'anonymous' , sex CHAR(1) default 'M' );

-- === TEST SCRIPT ===
insert into employee DEFAULT VALUES;
commit;
SELECT * FROM EMPLOYEE;
insert into employee DEFAULT VALUES RETURNING prenom,sex;
SELECT * FROM EMPLOYEE;
