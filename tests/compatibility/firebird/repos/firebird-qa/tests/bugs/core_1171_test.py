#coding:utf-8

"""
ID:          issue-1594
ISSUE:       1594
TITLE:       ISQL exponential format of numbers has zero pad on windows
DESCRIPTION:
JIRA:        CORE-1171
FBTEST:      bugs.core_1171
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select cast ('-2.488355210669293e+39' as double precision) from rdb$database;"""

act = isql_act('db', test_script)

expected_stdout = """
                   CAST
=======================
 -2.488355210669293e+39

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

