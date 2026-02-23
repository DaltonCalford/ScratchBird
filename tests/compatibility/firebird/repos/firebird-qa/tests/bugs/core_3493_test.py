#coding:utf-8

"""
ID:          issue-3852
ISSUE:       3852
TITLE:       Adding a value to a timestamp below '16.11.1858 00:00:01' throws 'value exceeds the range for valid timestamp'
DESCRIPTION:
JIRA:        CORE-3493
FBTEST:      bugs.core_3493
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "SELECT CAST('01.01.0200 12:00:00' as timestamp) + 1 from rdb$database;")

expected_stdout = """
                      ADD
=========================
0200-01-02 12:00:00.0000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

