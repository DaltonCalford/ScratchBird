#coding:utf-8

"""
ID:          issue-4653
ISSUE:       4653
TITLE:       not correct result function LAG, if OFFSET value are assigned from a table
DESCRIPTION:
JIRA:        CORE-4330
FBTEST:      bugs.core_4330
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    with t(a, b) as
    (
      select 1, null from rdb$database
      union all
      select 2, 1 from rdb$database
      union all
      select 3, 2 from rdb$database
      union all
      select 4, 3 from rdb$database
      union all
      select 5, 2 from rdb$database
    )
    select
      a,
      b,
      lag(a, b)over(order by a) as la
    from t ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    A                               1
    B                               <null>
    LA                              <null>

    A                               2
    B                               1
    LA                              1

    A                               3
    B                               2
    LA                              1

    A                               4
    B                               3
    LA                              1

    A                               5
    B                               2
    LA                              3
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
