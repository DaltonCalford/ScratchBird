#coding:utf-8

"""
ID:          intfunc.math.power
TITLE:       POWER( <number>, <number> )
DESCRIPTION: POWER(X, Y) returns X to the power of Y.
FBTEST:      functional.intfunc.math.power_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select power(2, 3) from rdb$database;")

expected_stdout = """
POWER
=======================
8.000000000000000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
