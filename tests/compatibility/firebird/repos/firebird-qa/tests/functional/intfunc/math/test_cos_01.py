#coding:utf-8

"""
ID:          intfunc.math.cos
TITLE:       COS( <number>)
DESCRIPTION:
  Returns the cosine of a number. The angle is specified in radians and returns a value in the range -1 to 1.
FBTEST:      functional.intfunc.math.cos_01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select COS( 14) from rdb$database;
select COS( 0) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
COS
=======================
0.1367372182078336
COS
=======================
1.000000000000000

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
