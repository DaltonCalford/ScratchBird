#coding:utf-8

"""
ID:          issue-4523
ISSUE:       4523
TITLE:       Incorrect "token unknown" error when the SQL string ends with a hex number literal
DESCRIPTION:
JIRA:        CORE-4198
FBTEST:      bugs.core_4198
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select 1 v_passed from rdb$database where 1 = 0x1 ;
    select 2 v_failed from rdb$database where 1 = 0x1; -- confirmed fail on 3.0 Alpha1 (passes OK on Alpha2)
"""

act = isql_act('db', test_script)

expected_stdout = """
    V_PASSED                        1
    V_FAILED                        2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

