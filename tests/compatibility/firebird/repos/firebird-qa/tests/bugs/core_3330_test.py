#coding:utf-8

"""
ID:          issue-3696
ISSUE:       3696
TITLE:       Server crashes while recreating the table with a NULL -> NOT NULL change
DESCRIPTION:
JIRA:        CORE-3330
FBTEST:      bugs.core_3330
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table test (a int);
    commit;
    insert into test (a) values (null);
    commit;
    recreate table test (b int not null);
    commit; -- crash here
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
