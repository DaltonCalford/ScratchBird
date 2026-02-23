#coding:utf-8

"""
ID:          issue-5474
ISSUE:       5474
TITLE:       Precedence problem with operator IS
DESCRIPTION:
JIRA:        CORE-5193
FBTEST:      bugs.core_5193
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select not false is true is unknown as boo1 from rdb$database;
    select not false = true is not unknown as boo2  from rdb$database;
    select not unknown and not unknown is not unknown as boo3  from rdb$database;
    select not not unknown is not unknown  as boo4 from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    BOO1                            <true>
    BOO2                            <true>
    BOO3                            <null>
    BOO4                            <false>
"""

@pytest.mark.version('>=3.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

