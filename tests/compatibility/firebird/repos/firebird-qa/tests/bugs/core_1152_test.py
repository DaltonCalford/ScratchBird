#coding:utf-8

"""
ID:          issue-1573
ISSUE:       1573
TITLE:       Cannot erase a table with check constraints referencing more than a single columns
DESCRIPTION:
JIRA:        CORE-1152
FBTEST:      bugs.core_1152
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE test(
int1 INTEGER,
int2 INTEGER,
CHECK(int1 IS NULL OR int2 IS NULL)
);
"""

db = db_factory(init=init_script)

test_script = """DROP TABLE test;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
