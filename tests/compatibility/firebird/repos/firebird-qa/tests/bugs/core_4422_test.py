#coding:utf-8

"""
ID:          issue-4744
ISSUE:       4744
TITLE:       FB crashes when using row_number()over( PARTITION BY x) in ORDER by clause
DESCRIPTION:
JIRA:        CORE-4422
FBTEST:      bugs.core_4422
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
  select 1 as n
  from rdb$database
  order by row_number()over( PARTITION BY 1);
"""

act = isql_act('db', test_script)

expected_stdout = """
           N
============
           1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

