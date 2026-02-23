#coding:utf-8

"""
ID:          issue-6337
ISSUE:       6337
TITLE:       Problem with casting within UNION
DESCRIPTION:
JIRA:        CORE-6087
FBTEST:      bugs.core_6087
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
   set list on;
   select cast(0.1234 as int) as result from rdb$database
   union all
   select cast(0.1234 as numeric(18,4)) from rdb$database
   ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RESULT                          0.0000
    RESULT                          0.1234
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
