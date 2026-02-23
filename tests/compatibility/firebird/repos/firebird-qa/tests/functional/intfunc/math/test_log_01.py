#coding:utf-8

"""
ID:          intfunc.math.log
TITLE:       LOG( <number>, <number> )
DESCRIPTION: Returns the logarithm base x of y.
FBTEST:      functional.intfunc.math.log_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select log(6, 10) from rdb$database;")

expected_stdout = """
LOG
=======================
1.285097208938469
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
