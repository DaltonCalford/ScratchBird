#coding:utf-8

"""
ID:          intfunc.math.pi
TITLE:       PI()
DESCRIPTION: Returns the PI constant (3.1459...).
FBTEST:      functional.intfunc.math.pi_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select PI() from rdb$database;")

expected_stdout = """
PI
=======================
3.141592653589793
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
