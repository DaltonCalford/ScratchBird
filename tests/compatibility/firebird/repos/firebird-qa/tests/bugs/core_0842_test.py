#coding:utf-8

"""
ID:          issue-1231
ISSUE:       1231
TITLE:       Specific query crashing server
DESCRIPTION:
  Run the query below twice and the server will crash:

    select
       cast('' as varchar(32765)),
       cast('' as varchar(32748))
    from
       rdb$database;
JIRA:        CORE-842
FBTEST:      bugs.core_0842
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
  set list on;
  -- [pcisar] 20.10.2021
  -- This script reports error:
  -- Statement failed, SQLSTATE = HY004
  -- Dynamic SQL Error
  -- -SQL error code = -204
  -- -Data type unknown
  -- -Implementation limit exceeded
  -- -COLUMN
  -- Statement failed, SQLSTATE = HY004
  -- Dynamic SQL Error
  -- -SQL error code = -204
  -- -Data type unknown
  -- -Implementation limit exceeded
  -- -COLUMN

  select cast('' as varchar(32765)), cast('' as varchar(32748)) from rdb$database;
  select cast('' as varchar(32765)), cast('' as varchar(32748)) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CAST
    CAST
    CAST
    CAST
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

