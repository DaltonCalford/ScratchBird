#coding:utf-8

"""
ID:          issue-1547
ISSUE:       1547
TITLE:       Incorrect results when left join on subquery with constant column
DESCRIPTION:
JIRA:        CORE-1126
FBTEST:      bugs.core_1126
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """SELECT _UTF8 'Z' FROM RDB$DATABASE
UNION ALL
SELECT _UTF8 'A' FROM RDB$DATABASE;
"""

act = isql_act('db', test_script)

expected_stdout = """CONSTANT
========
Z
A

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

