#coding:utf-8

"""
ID:          issue-5933
ISSUE:       5933
TITLE:       Regression in 3.0+: message CTE 'X' has cyclic dependencies appear when 'X'
  is alias for resultset and there is previous CTE part with the same name 'X' in the query
DESCRIPTION:
JIRA:        CORE-5667
FBTEST:      bugs.core_5667
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    with
    x as(
      select 1 i from rdb$database
    )
    ,y as(
      select i from x
    )
    select * from y as x ------------------ NOTE: "as X", i.e. alias for final CTE part ( "y" ) matches to 1st CTE ( "x" )
    ;

    -- Additional sample:
    with recursive
    a as(
      select 0 a from rdb$database
      union all
      select a+1 from a where a.a < 1
    )
    ,b as(
      select a b from a b
    )
    ,c as(
      select b c from b c
    )
    select c a from c a
    ;

    -- Two samples from CORE-5655:
    with cte as (select sign(t.rdb$relation_id) ct from rdb$database t) select ct.ct from cte ct;
    select e.ct from (select d.ct from (select sign(t.rdb$relation_id) ct from rdb$database t) d) e;


"""

act = isql_act('db', test_script)

expected_stdout = """
    I                               1

    A                               0
    A                               1

    CT                              1
    CT                              1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
