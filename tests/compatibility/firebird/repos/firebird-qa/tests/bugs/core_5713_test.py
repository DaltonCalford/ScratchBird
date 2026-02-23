#coding:utf-8

"""
ID:          issue-5979
ISSUE:       5979
TITLE:       Field alias disapears in complex query
DESCRIPTION:
JIRA:        CORE-5713
FBTEST:      bugs.core_5713
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select a1, a2
    from (
      select 1 a1, 2 a2
      from rdb$database
    )
    group by 1, 2

    union all

    select 1 a1, coalesce(cast(null as varchar(64)), 0) a2
    from rdb$database;

"""

act = isql_act('db', test_script)

expected_stdout = """
    A1                              1
    A2                              2

    A1                              1
    A2                              0
"""

@pytest.mark.version('>=3.0.3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

