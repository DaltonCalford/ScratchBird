#coding:utf-8

"""
ID:          issue-4891
ISSUE:       4891
TITLE:       Regression: Incorrect result in subquery with aggregate
DESCRIPTION:
JIRA:        CORE-4574
FBTEST:      bugs.core_4574
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    with
    a as (
        select 1 id from rdb$database
        union all
        select 2 from rdb$database
        union all
        select 3 from rdb$database
    ),
    b as (
        select 1 id1, null id2 from rdb$database
        union all
        select 2, null from rdb$database
        union all
        select 3, null from rdb$database
    )
    select
        sum((select count(*) from B where B.ID1 = A.ID)) s1
        ,sum((select count(*) from B where B.ID2 = A.ID)) s2
        -- must be (3,0) (FB2.5) , but not (3,3) (FB3.0)
    from a;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    S1                              3
    S2                              0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

