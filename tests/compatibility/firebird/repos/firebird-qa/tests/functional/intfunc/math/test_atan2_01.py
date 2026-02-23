#coding:utf-8

"""
ID:          intfunc.math.atan2
TITLE:       ATAN2( <number>, <number> )
DESCRIPTION:
  Returns the arc tangent of the first number / the second number.
  Returns a value in the range -PI to PI.
FBTEST:      functional.intfunc.math.atan2_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select ATAN2( 1, 1) from rdb$database;")

expected_stdout = """
ATAN2
=======================
     0.7853981633974483
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
