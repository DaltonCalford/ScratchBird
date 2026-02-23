#coding:utf-8

"""
ID:          intfunc.math.exp
TITLE:       EXP( <number> )
DESCRIPTION: Returns the exponential e to the argument.
FBTEST:      functional.intfunc.math.exp_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select EXP(3) from rdb$database;")

expected_stdout = """
EXP
=======================
20.08553692318767
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
