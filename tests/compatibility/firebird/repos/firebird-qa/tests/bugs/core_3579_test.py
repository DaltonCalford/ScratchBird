#coding:utf-8

"""
ID:          issue-3933
ISSUE:       3933
TITLE:       Can't drop table when computed field depends on later created another field
DESCRIPTION:
JIRA:        CORE-3579
FBTEST:      bugs.core_3579
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core3579.fbk')

test_script = """
    drop table Test;
    show table;
"""

act = isql_act('db', test_script)

expected_stderr = """
   There are no tables in this database
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

