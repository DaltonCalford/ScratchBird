#coding:utf-8

"""
ID:          intfunc.math.minvalue
TITLE:       MINVALUE( <value> [, <value> ... )
DESCRIPTION: Returns the minimun value of a list of values.
FBTEST:      functional.intfunc.math.minvalue_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select minvalue(9, 7, 10) from rdb$database;")

expected_stdout = """
MINVALUE
============
7
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
