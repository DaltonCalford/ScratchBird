#coding:utf-8

"""
ID:          intfunc.math.ceil
TITLE:       CEIL( <number>)
DESCRIPTION:
  Returns a value representing the smallest integer that is greater than or equal to the input argument.
FBTEST:      functional.intfunc.math.ceil_01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select CEIL( 2.1) from rdb$database;
select CEIL( -2.1) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
CEIL
=====================
   3
   CEIL
=====================
   -2



"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
