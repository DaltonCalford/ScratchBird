#coding:utf-8

"""
ID:          issue-3575
ISSUE:       3575
TITLE:       ATAN2 returns incorrect value for (0, 0)
DESCRIPTION:
JIRA:        CORE-3201
FBTEST:      bugs.core_3201
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select ATAN2(0, 0) undef_atan from rdb$database;
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    expression evaluation not supported
    -Arguments for ATAN2 cannot both be zero
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

