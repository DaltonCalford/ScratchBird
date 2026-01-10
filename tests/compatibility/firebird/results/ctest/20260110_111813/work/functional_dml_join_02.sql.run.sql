-- === PRELUDE SCRIPT ===
CREATE DATABASE '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260110_111813/work/functional_dml_join_02.sql.fdb';
CONNECT '/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260110_111813/work/functional_dml_join_02.sql.fdb';
-- Test ID: functional.dml.join.02
-- Title: NATURAL join
-- Description: <natural join> ::=
<table reference> NATURAL <join type> JOIN <table primary>
-- Firebird Version: 2.1
-- Platform: All
-- Test Type: ISQL
-- Converted from: join_02.fbt

-- === INIT SCRIPT ===
CREATE TABLE employee( id_employee INTEGER , prenom VARCHAR(20) ,id_department INTEGER, PRIMARY KEY(id_employee));
CREATE TABLE department(id_department INTEGER, name VARCHAR(20));
INSERT INTO department(id_department, name) values(1,'somme');
INSERT INTO department(id_department, name) values(2,'pas de calais');
INSERT INTO employee(id_employee, prenom,id_department) VALUES (1,'benoit',1 );
INSERT INTO employee(id_employee, prenom,id_department) VALUES (2,'tom',2 );

-- === TEST SCRIPT ===
select employee.prenom , department.name from employee natural join department;
