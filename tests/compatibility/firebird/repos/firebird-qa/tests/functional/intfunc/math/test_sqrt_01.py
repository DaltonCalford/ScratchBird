#coding:utf-8

"""
ID:          intfunc.math.sqrt
TITLE:       SQRT( <number> )
DESCRIPTION: Returns the square root of a number.
FBTEST:      functional.intfunc.math.sqrt_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select SQRT(4) from rdb$database;")

expected_stdout = """
SQRT
=======================
2.000000000000000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
