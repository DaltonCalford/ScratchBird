#coding:utf-8

"""
ID:          issue-6006
ISSUE:       6006
TITLE:       Incorrect error message in iSQL when trying to create database with wrong password
DESCRIPTION:
  We can just attempt to create current test database with wrong password.
  Result should contain text "SQLSTATE=28000"
  ("Your user name and password are not defined" or "install incomplete..." - no matter).
JIRA:        CORE-5742
FBTEST:      bugs.core_5742
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    commit;
    create database '$(DSN)' user sysdba password 'T0tAlly$Wr0ng';
"""

act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE).)*$', '')])

expected_stderr = """
    Statement failed, SQLSTATE = 28000
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
