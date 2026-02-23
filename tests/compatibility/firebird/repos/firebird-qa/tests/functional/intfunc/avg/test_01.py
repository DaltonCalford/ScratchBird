#coding:utf-8

"""
ID:          intfunc.avg-01
TITLE:       AVG from single integer row
DESCRIPTION:
FBTEST:      functional.intfunc.avg.01
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE test( id INTEGER NOT NULL CONSTRAINT unq UNIQUE,
                   text VARCHAR(32));
INSERT INTO test VALUES(5,null);"""

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
