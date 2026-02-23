#coding:utf-8

"""
ID:          intfunc.avg-04
TITLE:       AVG - Test for INTEGER
DESCRIPTION:
  Round up (17/3 = 5,66666666666667)
  But it's from INTEGER columns, so 5 is expected.
FBTEST:      functional.intfunc.avg.04
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE test( id INTEGER NOT NULL);
INSERT INTO test VALUES(5);
INSERT INTO test VALUES(6);
INSERT INTO test VALUES(6);"""

db = db_factory(init=init_script)

act = isql_act('db', "SELECT AVG(id) FROM test;")

expected_stdout = """
AVG
=====================

5
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
