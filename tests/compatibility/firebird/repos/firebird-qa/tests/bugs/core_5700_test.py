#coding:utf-8

"""
ID:          issue-5966
ISSUE:       5966
TITLE:       DECFLOAT underflow should yield zero instead of an error
DESCRIPTION:
  Test case is based on letter from Alex, 05-feb-2018 20:23.
JIRA:        CORE-5700
FBTEST:      bugs.core_5700
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select 1e-5000 / 1e5000 as r from rdb$database; -- this should NOT raise exception since this ticket was fixed.
    set decfloat traps to underflow;
    select 1e-5000 / 1e5000 as r from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[\\s]+', ' ')])

expected_stdout = """
    R                                                                  0E-6176
"""

expected_stderr = """
    Statement failed, SQLSTATE = 22003
    Decimal float underflow.  The exponent of a result is less than the magnitude allowed.
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
