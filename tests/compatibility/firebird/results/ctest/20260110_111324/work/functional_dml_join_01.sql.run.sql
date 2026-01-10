CREATE DATABASE '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260110_111324/work/functional_dml_join_01.sql.fdb';
CONNECT '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260110_111324/work/functional_dml_join_01.sql.fdb';
-- Test ID: functional.dml.join.01
-- Title: NAMED COLUMNS join
-- Description: <named columns join> ::=
<table reference> <join type> JOIN <table reference>
USING ( <column list> )
-- Firebird Version: 2.1
-- Platform: All
-- Test Type: ISQL
-- Converted from: join_01.fbt

-- === INIT SCRIPT ===
CREATE TABLE employee( id_employee INTEGER , prenom VARCHAR(20) ,id_department INTEGER, PRIMARY KEY(id_employee));
CREATE TABLE department(id_department INTEGER, name VARCHAR(20));
INSERT INTO department(id_department, name) values(1,'somme');
INSERT INTO department(id_department, name) values(2,'pas de calais');
INSERT INTO employee(id_employee, prenom,id_department) VALUES (1,'benoit',1 );
INSERT INTO employee(id_employee, prenom,id_department) VALUES (2,'tom',2 );

-- === TEST SCRIPT ===
select employee.prenom , department.name from employee join department using (id_department);
