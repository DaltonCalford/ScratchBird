#coding:utf-8

"""
ID:          issue-2148
ISSUE:       2148
TITLE:        Common table expressions cannot be used in computed columns and quantified predicates (IN / ANY / ALL)
DESCRIPTION:
JIRA:        CORE-1724
FBTEST:      bugs.core_1724
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select
        sign( count(*) ) +
        (
          with recursive
          n as(
            select 0 i from rdb$database
            union all
            select n.i+1 from n
            where n.i < 10
          )
          select avg(i)
          from n
        ) s
    from rdb$pages p
    where p.rdb$relation_id
    > all (
        with recursive
        n as(
          select 0 i from rdb$database
          union all
          select n.i+1 from n
          where n.i < 10
        )
        select i
        from n
    )
    and p.rdb$page_number
    not in (
        with recursive
        n as(
          select 0 i from rdb$database
          union all
          select n.i+1 from n
          where n.i < 10
        )
        select i
        from n
    );
"""

act = isql_act('db', test_script)

expected_stdout = """
    S                               6
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

