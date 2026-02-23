#coding:utf-8

"""
ID:          issue-3953
ISSUE:       3953
TITLE:       Possible drop role RDB$ADMIN
DESCRIPTION:
JIRA:        CORE-3599
FBTEST:      bugs.core_3599
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    DROP ROLE RDB$ADMIN;
    COMMIT;
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -DROP ROLE RDB$ADMIN failed
    -Cannot delete system SQL role RDB$ADMIN
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

