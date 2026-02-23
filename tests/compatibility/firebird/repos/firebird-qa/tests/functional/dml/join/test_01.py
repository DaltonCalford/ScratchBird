#coding:utf-8

"""
ID:          dml.join-01
FBTEST:      functional.dml.join.01
TITLE:       NAMED COLUMNS join
DESCRIPTION:
  <named columns join> ::=
    <table reference> <join type> JOIN <table reference>
    USING ( <column list> )
"""

import pytest
from firebird.qa import *

init_script = """
CREATE TABLE employee( id_employee INTEGER , prenom VARCHAR(20) ,id_department INTEGER, PRIMARY KEY(id_employee));
CREATE TABLE department(id_department INTEGER, name VARCHAR(20));
INSERT INTO department(id_department, name) values(1,'somme');
INSERT INTO department(id_department, name) values(2,'pas de calais');
INSERT INTO employee(id_employee, prenom,id_department) VALUES (1,'benoit',1 );
INSERT INTO employee(id_employee, prenom,id_department) VALUES (2,'tom',2 );"""

db = db_factory(init=init_script)

act = isql_act('db', "select employee.prenom, department.name from employee join department using (id_department);")

expected_stdout = """
PRENOM               NAME
==================== ====================
benoit               somme
tom                  pas de calais
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
