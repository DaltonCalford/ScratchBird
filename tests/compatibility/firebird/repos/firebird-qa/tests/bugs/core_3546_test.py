#coding:utf-8

"""
ID:          issue-3902
ISSUE:       3902
TITLE:       Aliases for the RETURNING clause
DESCRIPTION:
JIRA:        CORE-3546
FBTEST:      bugs.core_3546
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t(id int, x int default 123, y int default 456);
    commit;
    set list on;
    insert into t(id) values(1) returning x+y as i234567890123456789012345678901;
    insert into t(id) values(2) returning x-y    "/** That's result of (x-y) **/";
"""

act = isql_act('db', test_script)

expected_stdout = """
    I234567890123456789012345678901 579
    /** That's result of (x-y) **/  -333
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

