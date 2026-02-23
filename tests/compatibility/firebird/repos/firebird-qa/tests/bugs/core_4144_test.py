#coding:utf-8

"""
ID:          issue-4471
ISSUE:       4471
TITLE:       Error "context already in use (BLR error)" when preparing a query with UNION
DESCRIPTION:
JIRA:        CORE-4144
FBTEST:      bugs.core_4144
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """set heading off;
select n
  from
  (
     select d.rdb$relation_id as n from rdb$database d
     union all
     select d.rdb$relation_id as n from rdb$database d
  )
union all
select cast(1 as bigint) from rdb$database d;
"""

act = isql_act('db', test_script)

expected_stdout = """128
128
1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

