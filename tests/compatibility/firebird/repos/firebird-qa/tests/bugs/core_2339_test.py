#coding:utf-8

"""
ID:          issue-2763
ISSUE:       2763
TITLE:       Incorrect result for the derived expression based on aggregate and computation
DESCRIPTION:
JIRA:        CORE-2339
FBTEST:      bugs.core_2339
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select * from ( select sum(1)*1 as "sum_1_multiplied_for_1" from rdb$database );
    -- result is NULL instead of 1
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    sum_1_multiplied_for_1 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
