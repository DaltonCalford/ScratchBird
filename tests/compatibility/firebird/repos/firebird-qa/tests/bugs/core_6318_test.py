#coding:utf-8

"""
ID:          issue-6559
ISSUE:       6559
TITLE:       CAST('NOW' as TIME) raises exception
DESCRIPTION:
JIRA:        CORE-6318
FBTEST:      bugs.core_6318
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    select cast('now' as time) is not null from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    <true>
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
