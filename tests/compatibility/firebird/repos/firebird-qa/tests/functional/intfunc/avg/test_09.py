#coding:utf-8

"""
ID:          intfunc.avg-09
TITLE:       AVG - DOUBLE PRECISION
DESCRIPTION: AVG from single DOUBLE PRECISION row
FBTEST:      functional.intfunc.avg.09
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE test( id DOUBLE PRECISION NOT NULL);
INSERT INTO test VALUES(5.123456789);"""

db = db_factory(init=init_script)

act = isql_act('db', "SELECT AVG(id) FROM test;")

expected_stdout = """
AVG
=======================

5.123456789000000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
