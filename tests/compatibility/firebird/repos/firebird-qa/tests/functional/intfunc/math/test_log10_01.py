#coding:utf-8

"""
ID:          intfunc.math.log10
TITLE:       LOG10( <number> )
DESCRIPTION: Returns the logarithm base ten of a number.
FBTEST:      functional.intfunc.math.log10_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select log10(6) from rdb$database;")

expected_stdout = """
LOG10
=======================
0.7781512503836436
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
