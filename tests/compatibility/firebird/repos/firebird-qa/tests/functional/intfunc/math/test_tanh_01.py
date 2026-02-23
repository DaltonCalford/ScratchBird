#coding:utf-8

"""
ID:          intfunc.math.tanh
TITLE:       TANH( <number> )
DESCRIPTION:
  Returns the hyperbolic tangent of a number.
    select tanh(x) from y;
FBTEST:      functional.intfunc.math.tanh_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select TANH(5) from rdb$database;")

expected_stdout = """
TANH
=======================
0.9999092042625951
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
