#coding:utf-8

"""
ID:          intfunc.math.sin
TITLE:       SIN( <number> )
DESCRIPTION: Returns the sine of an input number that is expressed in radians.
FBTEST:      functional.intfunc.math.sin_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select CAST(SIN(12) AS DECIMAL(18,15)) from rdb$database;")

expected_stdout = """
                 CAST
=====================
   -0.536572918000435
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
