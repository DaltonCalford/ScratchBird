#coding:utf-8

"""
ID:          issue-2917
ISSUE:       2917
TITLE:       Built-in trigonometric functions can produce NaN and Infinity
DESCRIPTION:
JIRA:        CORE-1000
FBTEST:      bugs.core_2505
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """select asin(2), cot(0) from rdb$database;
select acos(2) - acos(2) from rdb$database;
select LOG10(-1) from rdb$database;"""

act = isql_act('db', test_script)

expected_stdout = """
                   ASIN                     COT
======================= =======================

               SUBTRACT
=======================

                  LOG10
=======================
"""

expected_stderr = """Statement failed, SQLSTATE = 42000

expression evaluation not supported

-Argument for COT must be different than zero

Statement failed, SQLSTATE = 42000

expression evaluation not supported

-Argument for ACOS must be in the range [-1, 1]

Statement failed, SQLSTATE = 42000

expression evaluation not supported

-Argument for LOG10 must be positive

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

