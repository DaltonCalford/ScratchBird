#coding:utf-8

"""
ID:          issue-2516
ISSUE:       2516
TITLE:       RDB$DB_KEY in subselect expression incorrectly returns NULL
DESCRIPTION:
JIRA:        CORE-2081
FBTEST:      bugs.core_2081
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select a.rdb$db_key, (select b.rdb$db_key from rdb$database b)
  from rdb$database a;
"""

act = isql_act('db', test_script)

expected_stdout = """
DB_KEY           DB_KEY
================ ================
0100000001000000 0100000001000000

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

