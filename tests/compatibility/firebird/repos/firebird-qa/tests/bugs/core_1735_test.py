#coding:utf-8

"""
ID:          issue-2159
ISSUE:       2159
TITLE:       Bug in SET DEFAULT statement
DESCRIPTION:
JIRA:        CORE-1735
FBTEST:      bugs.core_1735
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TEST (
   A INTEGER
);
"""

db = db_factory(init=init_script)

test_script = """ALTER TABLE TEST ADD CHECK (A > 0);

ALTER TABLE TEST ALTER A SET DEFAULT '1000';
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
