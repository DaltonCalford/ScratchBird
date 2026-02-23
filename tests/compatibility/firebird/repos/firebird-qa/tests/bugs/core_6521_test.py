#coding:utf-8

"""
ID:          issue-6750
ISSUE:       6750
TITLE:       CAST of Infinity values to FLOAT doesn't work
DESCRIPTION:
JIRA:        CORE-6521
FBTEST:      bugs.core_6521
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    select cast(log(1, 1.5) as float) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Infinity
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
