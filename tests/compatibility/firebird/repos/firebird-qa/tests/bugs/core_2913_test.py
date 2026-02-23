#coding:utf-8

"""
ID:          issue-3297
ISSUE:       3297
TITLE:       COLLATE expressions are applied incorrectly
DESCRIPTION:
JIRA:        CORE-2913
FBTEST:      bugs.core_2913
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select cast(_dos850 'X' as char(1) character set iso8859_1) collate iso8859_1 as x from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    X                               X
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

