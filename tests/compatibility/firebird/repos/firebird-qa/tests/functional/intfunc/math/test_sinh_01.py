#coding:utf-8

"""
ID:          intfunc.math.sinh
TITLE:       SINH( <number> )
DESCRIPTION: Returns the hyperbolic sine of a number.
FBTEST:      functional.intfunc.math.sinh_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select SINH(4) from rdb$database;")

expected_stdout = """
SINH
=======================
27.28991719712775
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
