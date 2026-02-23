#coding:utf-8

"""
ID:          intfunc.math.tan
TITLE:       TAN( <number> )
DESCRIPTION: Returns the tangent of an input number that is expressed in radians.
FBTEST:      functional.intfunc.math.tan_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select TAN(43) from rdb$database;")

expected_stdout = """
TAN
=======================
-1.498387338855171
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
