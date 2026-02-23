#coding:utf-8

"""
ID:          issue-4582
ISSUE:       4582
TITLE:       Regression: Wrong boundary for minimum value for BIGINT/DECIMAL(18)
DESCRIPTION:
JIRA:        CORE-4258
FBTEST:      bugs.core_4258
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test(x decimal(18), y bigint);
    commit;
    insert into test values( -9223372036854775808, -9223372036854775808);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select * from test;
"""

act = isql_act('db', test_script)

expected_stdout = """
    X                               -9223372036854775808
    Y                               -9223372036854775808
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

