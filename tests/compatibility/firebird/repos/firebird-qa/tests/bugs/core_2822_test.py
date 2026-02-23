#coding:utf-8

"""
ID:          issue-3209
ISSUE:       3209
TITLE:       Error "no current row for fetch operation" when subquery includes a non-trivial derived table
DESCRIPTION:
JIRA:        CORE-2822
FBTEST:      bugs.core_2822
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select *
from rdb$relations r natural join rdb$relation_fields rf
where 1 = (
  select 1
  from (
    select 1 from rdb$database
    union
    select 1
    from rdb$fields f
    where f.rdb$field_name = rf.rdb$field_source
  ) as f (id) ) ;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
