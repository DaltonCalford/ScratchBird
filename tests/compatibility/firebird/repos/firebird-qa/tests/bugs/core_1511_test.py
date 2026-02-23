#coding:utf-8

"""
ID:          issue-1926
ISSUE:       1926
TITLE:       POSITION(string_exp1, string_exp2 [, start])
DESCRIPTION:
JIRA:        CORE-1511
FBTEST:      bugs.core_1511
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """SELECT position ('be', 'To be or not to be')
,position ('be', 'To be or not to be', 4)
,position ('be', 'To be or not to be', 8)
,position ('be', 'To be or not to be', 18)
FROM RDB$DATABASE;"""

act = isql_act('db', test_script)

expected_stdout = """
    POSITION     POSITION     POSITION     POSITION
============ ============ ============ ============
           4            4           17            0

"""

@pytest.mark.version('>=2.1.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

