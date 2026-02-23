#coding:utf-8

"""
ID:          issue-4676
ISSUE:       4676
TITLE:       Parsing of recursive query returns error "Column does not belong to referenced table"
  for source that HAS such column
DESCRIPTION:
JIRA:        CORE-4354
FBTEST:      bugs.core_4354
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
with recursive
b as (
    select 0 rc
    from rdb$database qa

    union all

    select b.rc+1
    from b
        join rdb$database q1 on q1.rdb$relation_id*0=b.rc*0
        join rdb$database q2 on q2.rdb$relation_id*0=b.rc*0
    where b.rc=0
)
select * from b;
"""

act = isql_act('db', test_script)

expected_stdout = """
          RC
============
           0
           1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
