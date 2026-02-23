#coding:utf-8

"""
ID:          issue-319
ISSUE:       319
JIRA:        CORE-1
TITLE:       Server shutdown
DESCRIPTION: Server shuts down when user password is attempted to be modified to a empty string
FBTEST:      bugs.core_0001
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    alter user tmp$c0001 password '';
    commit;
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER USER TMP$C0001 failed
    -Password should not be empty string
"""

user = user_factory('db', name='tmp$c0001', password='123')

@pytest.mark.version('>=3.0')
def test_1(act: Action, user: User):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

