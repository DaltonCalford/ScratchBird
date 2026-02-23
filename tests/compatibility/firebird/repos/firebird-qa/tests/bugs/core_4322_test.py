#coding:utf-8

"""
ID:          issue-4645
ISSUE:       4645
TITLE:       Engine crashes when use aggregate or window functions in recursive query
  (instead of producing compiling error)
DESCRIPTION:
JIRA:        CORE-4322
FBTEST:      bugs.core_4322
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
with recursive
r as(
  select 0 i,count(*)over() c
  from rdb$database
  union all
  select r.i+1,sum(i)over() from r where r.i<10
)
select * from r;

with recursive
r as(
  select 0 i,count(*) c from rdb$database
  union all
  select r.i + 1, 0 from r where sum(r.i) = 0
)
select * from r;
"""

act = isql_act('db', test_script)

expected_stderr = """
Statement failed, SQLSTATE = 42000
Dynamic SQL Error
-SQL error code = -104
-Recursive member of CTE cannot use aggregate or window function
Statement failed, SQLSTATE = 42000
Dynamic SQL Error
-SQL error code = -104
-Cannot use an aggregate or window function in a WHERE clause, use HAVING (for aggregate only) instead
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

