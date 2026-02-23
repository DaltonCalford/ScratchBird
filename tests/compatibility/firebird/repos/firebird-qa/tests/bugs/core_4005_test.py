#coding:utf-8

"""
ID:          issue-4337
ISSUE:       4337
TITLE:       wrong error message with recursive CTE
DESCRIPTION:
JIRA:        CORE-4005
FBTEST:      bugs.core_4005
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    with recursive
    c1 as (
        select 0 as i from rdb$database
        union all
        select i + 1 from c1 where i < 1
    )
    ,c2 as (
        select i, 0 as j from c1
        union all
        select j * 10 + c1.i, c2.j + 1
        from c1 c1
        join c2 c2 on c2.j < 1
    )
    select count(i) as cnt from c2
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CNT                             6
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

