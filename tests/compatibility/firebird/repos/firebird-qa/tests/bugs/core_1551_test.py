#coding:utf-8

"""
ID:          issue-1968
ISSUE:       1968
TITLE:       AV when all statements are cancelled
DESCRIPTION:
JIRA:        CORE-1551
FBTEST:      bugs.core_1551
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """delete from MON$STATEMENTS;
delete from MON$ATTACHMENTS;
COMMIT;
SELECT 1 FROM RDB$DATABASE;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CONSTANT
============
           1

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

