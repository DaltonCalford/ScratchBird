#coding:utf-8

"""
ID:          intfunc.math.maxvalue
TITLE:       MAXVALUE( <value> [, <value> ...] )
DESCRIPTION: Returns the maximum value of a list of values.
FBTEST:      functional.intfunc.math.maxvalue_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select maxvalue(54, 87, 10) from rdb$database;")

expected_stdout = """
MAXVALUE
============
87
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
