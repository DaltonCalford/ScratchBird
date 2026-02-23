#coding:utf-8

"""
ID:          issue-1241
ISSUE:       1241
TITLE:       substring(current_user from 4) fails
DESCRIPTION:
JIRA:        CORE-852
FBTEST:      bugs.core_0852
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select substring(current_user from 4) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    SUBSTRING                       DBA
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

