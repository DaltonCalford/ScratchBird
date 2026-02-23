#coding:utf-8

"""
ID:          intfunc.math.atan
TITLE:       ATAN( <number> )
DESCRIPTION:
  Returns the arc tangent of a number. Returns a value in the range -PI/2 to PI/2.
FBTEST:      functional.intfunc.math.atan_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select ATAN( 1 ) from rdb$database;")

expected_stdout = """
ATAN
=======================
     0.7853981633974483
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
