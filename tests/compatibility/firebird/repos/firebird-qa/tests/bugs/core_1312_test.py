#coding:utf-8

"""
ID:          issue-1731
ISSUE:       1731
TITLE:       A remote attacker can check, if a file is present in the system, running firebird server
DESCRIPTION: Check if password validation is done as soon as possible
JIRA:        CORE-1312
FBTEST:      bugs.core_1312
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    commit;
    connect 'localhost:bla' user 'qqq' password 'zzz';
"""

act = isql_act('db', test_script)

expected_stderr = """Statement failed, SQLSTATE = 28000
Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

