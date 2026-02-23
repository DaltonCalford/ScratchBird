#coding:utf-8

"""
ID:          intfunc.math.asin
TITLE:       ASIN( <number> )
DESCRIPTION:
  Returns the arc sine of a number. The argument to ASIN must be in the range -1 to 1.
  It returns a result in the range -PI/2 to PI/2.
FBTEST:      functional.intfunc.math.asin_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select asin( 1 ) from rdb$database;")

expected_stdout = """
ASIN
=======================
1.570796326794897"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
