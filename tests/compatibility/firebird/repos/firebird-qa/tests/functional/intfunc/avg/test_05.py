#coding:utf-8

"""
ID:          intfunc.avg-05
TITLE:       AVG - DISTINCT
DESCRIPTION:
FBTEST:      functional.intfunc.avg.05
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE test( id INTEGER NOT NULL);
INSERT INTO test VALUES(5);
INSERT INTO test VALUES(5);
INSERT INTO test VALUES(7);"""

db = db_factory(init=init_script)

act = isql_act('db', "SELECT AVG(DISTINCT id) FROM test;")

expected_stdout = """
AVG
=====================

6
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
