#coding:utf-8

"""
ID:          issue-5238
ISSUE:       5238
TITLE:       Compound ALTER TABLE statement with ADD and DROP the same check constraint fails
DESCRIPTION:
JIRA:        CORE-4947
FBTEST:      bugs.core_4947
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t(x int not null);
    alter table t
        add constraint cx check(x > 0),
        drop constraint cx;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
