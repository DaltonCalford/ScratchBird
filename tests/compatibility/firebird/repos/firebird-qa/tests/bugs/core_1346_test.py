#coding:utf-8

"""
ID:          issue-1765
ISSUE:       1765
TITLE:       lpad and rpad with two columns not working
DESCRIPTION:
JIRA:        CORE-1346
FBTEST:      bugs.core_1346
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select lpad('xxx', 8, '0') one, lpad('yyy', 8, '0') two from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
ONE      TWO
======== ========
00000xxx 00000yyy

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

