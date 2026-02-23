#coding:utf-8

"""
ID:          issue-2850
ISSUE:       2850
TITLE:       CREATE USER command: Invalid error message
DESCRIPTION:
  Attempt to create user with empty password should raise error with message related to
  this problem.
JIRA:        CORE-2434
FBTEST:      bugs.core_2434
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create user tmp$c2434 password '';
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE USER TMP$C2434 failed
    -Password should not be empty string
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

