#coding:utf-8

"""
ID:          issue-3386
ISSUE:       3386
TITLE:       recreate table T with PK or UK is impossible after duplicate typing w/o commit
  when ISQL is launched in AUTODDL=OFF mode
DESCRIPTION:
JIRA:        CORE-3529
FBTEST:      bugs.gh_3886
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set autoddl off;
    recreate table t(id int primary key);
    recreate table t(id int primary key);
    commit;
    commit; -- yes, 2nd time.
    exit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.execute()
