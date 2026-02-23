#coding:utf-8

"""
ID:          intfunc.math.ln
TITLE:       LN( <number> )
DESCRIPTION: Returns the natural logarithm of a number.
FBTEST:      functional.intfunc.math.ln_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select ln(5) from rdb$database;")

expected_stdout = """
LN
=======================
1.609437912434100
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
