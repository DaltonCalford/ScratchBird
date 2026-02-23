#coding:utf-8

"""
ID:          issue-5973
ISSUE:       5973
TITLE:       Begin and end of physical backup in the same transaction could crash engine
DESCRIPTION:
JIRA:        CORE-5707
FBTEST:      bugs.core_5707
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    alter database begin backup end backup;
    commit;
    set autoddl off;
    alter database begin backup;
    alter database end backup;
    commit;
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER DATABASE failed
    -Incompatible ALTER DATABASE clauses: 'BEGIN BACKUP' and 'END BACKUP'
"""

@pytest.mark.version('>=3.0.3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

