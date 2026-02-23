#coding:utf-8

"""
ID:          issue-673
ISSUE:       673
TITLE:       DateTime math imprecision
DESCRIPTION:
JIRA:        CORE-336
FBTEST:      bugs.core_336
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select (cast('01.01.2004 10:01:00' as timestamp)-cast('01.01.2004 10:00:00' as timestamp))+cast('01.01.2004 10:00:00' as timestamp) from rdb$database ;
"""

act = isql_act('db', test_script)

expected_stdout = """ADD
=========================
2004-01-01 10:01:00.0000

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

