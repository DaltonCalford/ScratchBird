#coding:utf-8

"""
ID:          intfunc.math.acos
TITLE:       ACOS( <number> )
DESCRIPTION:
  Returns the arc cosine of a number. Argument to ACOS must be in the range -1 to 1.
  Returns a value in the range 0 to PI. er.
FBTEST:      functional.intfunc.math.acos_01
"""

import pytest
from firebird.qa import db_factory, isql_act, Action

db = db_factory()

act = isql_act('db', "select cast( ACOS( 1 ) AS DECIMAL(18,15)) from rdb$database;")

expected_stdout = """
                 CAST
=====================
    0.000000000000000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
