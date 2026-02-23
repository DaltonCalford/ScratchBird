#coding:utf-8

"""
ID:          intfunc.math.cot
TITLE:       COT( <number> )
DESCRIPTION: returns 1 / tan(argument)
FBTEST:      functional.intfunc.math.cot_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select cast(COT(1) AS DECIMAL(18,15)) from rdb$database;")

expected_stdout = """
                 CAST
=====================
    0.642092615934331
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
