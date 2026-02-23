#coding:utf-8

"""
ID:          issue-3382
ISSUE:       3382
TITLE:       Error on delete user "ADMIN"
DESCRIPTION:
  Also added sample from #3488
JIRA:        CORE-3000
FBTEST:      bugs.core_3000
"""

import pytest
from firebird.qa import *

db_ = db_factory()

test_script = """
    -- Following users should NOT be created:
    create user 'ADMIN' password '123';
    create user 'CHECK' password '123';
"""

act = isql_act('db_', test_script, substitutions=[('-Token unknown - line.*', '-Token unknown')])

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown
    -'ADMIN'

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown
    -'CHECK'
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

