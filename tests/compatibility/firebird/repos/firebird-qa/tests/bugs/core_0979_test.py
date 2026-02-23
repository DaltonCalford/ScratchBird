#coding:utf-8

"""
ID:          issue-1384
ISSUE:       1384
TITLE:       Make RDB$DB_KEY in outer joins return NULL when appropriate
DESCRIPTION:
JIRA:        CORE-979
FBTEST:      bugs.core_0979
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE employee( id_employee INTEGER , prenom VARCHAR(20) ,id_department INTEGER, PRIMARY KEY(id_employee));
CREATE TABLE department(id_department INTEGER, name VARCHAR(20));
COMMIT;
INSERT INTO employee(id_employee, prenom,id_department) VALUES (1,'benoit',1 );
COMMIT;
"""

db = db_factory(init=init_script)

test_script = """select department.rdb$db_key  from employee
left OUTER JOIN department
 on department.id_department = employee.id_department;
"""

act = isql_act('db', test_script)

expected_stdout = """DB_KEY
================
<null>"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

