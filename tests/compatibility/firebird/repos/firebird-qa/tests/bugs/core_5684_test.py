#coding:utf-8

"""
ID:          issue-5950
ISSUE:       5950
TITLE:       Error "no current record for fetch operation" is raised while deleting record
  from MON$ATTACHMENTS using ORDER BY clause
DESCRIPTION:
JIRA:        CORE-5684
FBTEST:      bugs.core_5684
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    commit;
    set count on;
    delete from mon$attachments order by mon$attachment_id rows 1;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

