#coding:utf-8

"""
ID:          issue-2382
ISSUE:       2382
TITLE:       Incorrect results when left join on subquery with constant column
DESCRIPTION:
JIRA:        CORE-1943
FBTEST:      bugs.core_1943
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select first 10 1 from rdb$database a group by rand();
"""

act = isql_act('db', test_script)

expected_stdout = """
    CONSTANT
============
           1

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

