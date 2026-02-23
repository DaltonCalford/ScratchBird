#coding:utf-8

"""
ID:          intfunc.avg-08
TITLE:       AVG - NULL test
DESCRIPTION:
FBTEST:      functional.intfunc.avg.08
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE test( id INTEGER);
INSERT INTO test VALUES(NULL);"""

db = db_factory(init=init_script)

act = isql_act('db', "SELECT AVG(id) FROM test;")

expected_stdout = """
AVG
=====================

<null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
