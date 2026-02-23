#coding:utf-8

"""
ID:          issue-6832
ISSUE:       6832
TITLE:       Segfault using "commit retaining" with GTT
DESCRIPTION:
FBTEST:      bugs.gh_6832
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set autoddl off;
    recreate global temporary table gtt(x int);
    commit retain;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.execute()
