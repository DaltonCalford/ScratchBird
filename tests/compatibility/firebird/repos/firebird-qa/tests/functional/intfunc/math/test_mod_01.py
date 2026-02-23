#coding:utf-8

"""
ID:          intfunc.math.mod
TITLE:       MOD( <number>, <number> )
DESCRIPTION: Modulo: MOD(X, Y) returns the remainder part of the division of X by Y.
FBTEST:      functional.intfunc.math.mod_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select MOD(11,10) from rdb$database;")

expected_stdout = """
MOD
============
1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
