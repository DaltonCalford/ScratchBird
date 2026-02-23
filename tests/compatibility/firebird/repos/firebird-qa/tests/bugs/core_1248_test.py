#coding:utf-8

"""
ID:          issue-1672
ISSUE:       1672
TITLE:       Incorrect timestamp arithmetic when one of operands is negative number
DESCRIPTION:
JIRA:        CORE-1248
FBTEST:      bugs.core_1248
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
  set heading off;
  select cast('04.05.2007' as timestamp) - (-7) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    2007-05-11 00:00:00.0000
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

