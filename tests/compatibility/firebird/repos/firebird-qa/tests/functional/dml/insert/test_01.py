#coding:utf-8

"""
ID:          dml.insert-01
FBTEST:      functional.dml.insert.01
TITLE:       INSERT with Defaults
DESCRIPTION:
  INSERT INTO <table>
    DEFAULT VALUES
    [RETURNING <values>]
"""

import pytest
from firebird.qa import *


db = db_factory(init="CREATE TABLE employee( prenom VARCHAR(20) default 'anonymous' , sex CHAR(1) default 'M' );")

test_script = """insert into employee DEFAULT VALUES;
commit;
SELECT * FROM EMPLOYEE;
insert into employee DEFAULT VALUES RETURNING prenom,sex;
SELECT * FROM EMPLOYEE;
"""

act = isql_act('db', test_script)

expected_stdout = """
PRENOM               SEX
==================== ======
anonymous            M


PRENOM               SEX
==================== ======
anonymous            M


PRENOM               SEX
==================== ======
anonymous            M
anonymous            M
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
