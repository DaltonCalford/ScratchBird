#coding:utf-8

"""
ID:          issue-7048
ISSUE:       7048
TITLE:       Incorrect releasing of user savepoint (older savepoints become inaccessible)
DESCRIPTION:
FBTEST:      bugs.gh_7048
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    savepoint a;
    savepoint b;
    release savepoint b;
    release savepoint a;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=4.0.1')
def test_1(act: Action):
    act.execute()
