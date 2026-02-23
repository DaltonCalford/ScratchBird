#coding:utf-8

"""
ID:          issue-3875
ISSUE:       3875
TITLE:       Server crash with built in function LPAD with string as second parameter
DESCRIPTION:
JIRA:        CORE-3517
FBTEST:      bugs.core_3517
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select LPAD('abc', 0, 3) FROM RDB$DATABASE;")

expected_stdout = """
LPAD
======

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

