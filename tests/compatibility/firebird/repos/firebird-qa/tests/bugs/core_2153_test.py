#coding:utf-8

"""
ID:          issue-2584
ISSUE:       2584
TITLE:       SIMILAR TO predicate hangs with "|" option
DESCRIPTION:
JIRA:        CORE-2153
FBTEST:      bugs.core_2153
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select iif( 'avieieavav' similar to '%(av|ie){2,}%', 1, 0) r from rdb$database;
    select iif( 'avieieieav' similar to '%((av)|(ie)){2,}%', 1, 0) r from rdb$database;
    select iif( 'eiavieieav' similar to '%(av)|{2,}%', 1, 0) r from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    R                               1
    R                               1
"""

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Invalid SIMILAR TO pattern
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

