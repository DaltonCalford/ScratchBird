#coding:utf-8

"""
ID:          intfunc.math.sign
TITLE:       SIGN( <number> )
DESCRIPTION:
  Returns 1, 0, or -1 depending on whether the input value is positive, zero or negative, respectively.
FBTEST:      functional.intfunc.math.sign_01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
select SIGN(-9) from rdb$database;
select SIGN(8) from rdb$database;
select SIGN(0) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
SIGN
=======
     -1

SIGN
=======
      1

SIGN
=======
0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
