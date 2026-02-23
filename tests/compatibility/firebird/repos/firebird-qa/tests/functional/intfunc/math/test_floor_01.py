#coding:utf-8

"""
ID:          intfunc.math.floor
TITLE:       FLOOR( <number> )
DESCRIPTION:
  Returns a value representing the largest integer that is less than or equal to the input argument
FBTEST:      functional.intfunc.math.floor_01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select FLOOR(2.1) from rdb$database;
select FLOOR(-4.4) from rdb$database;"""

act = isql_act('db', test_script)

expected_stdout = """
FLOOR
=====================
2


FLOOR
=====================
-5
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
