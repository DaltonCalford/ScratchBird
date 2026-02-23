#coding:utf-8

"""
ID:          intfunc.math.round
TITLE:       ROUND( <number>, <number> )
DESCRIPTION: Returns a number rounded to the specified scale.
FBTEST:      functional.intfunc.math.round_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select ROUND(5.7778, 3) from rdb$database;")

expected_stdout = """
ROUND
=====================
5.7780
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
