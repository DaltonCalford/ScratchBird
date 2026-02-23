#coding:utf-8

"""
ID:          intfunc.math.abs
TITLE:       ABS( <number> )
DESCRIPTION: Test of ABS( <number> ) function returns the absolute value of a number.
FBTEST:      functional.intfunc.math.abs_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select ABS( -1 ) from rdb$database;")

expected_stdout = """
ABS
=====================
1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
