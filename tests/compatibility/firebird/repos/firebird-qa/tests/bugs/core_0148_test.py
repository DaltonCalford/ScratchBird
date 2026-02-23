#coding:utf-8

"""
ID:          issue-477
ISSUE:       477
TITLE:       SELECT '1' UNION SELECT '12'
DESCRIPTION:
JIRA:        CORE-148
FBTEST:      bugs.core_0148
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Confirmed: runtime error on WI-V1.5.6.5026:
    -- -SQL error code = -104
    -- -Invalid command
    -- -Data type unknown

    recreate table test_a(x int);
    recreate table test_b(y int);

    insert into test_a values(9999999);
    insert into test_b values(888888);

    set list on;
    select '1' || a.x as result
    from test_a a
    union
    select '12' || b.y
    from test_b b
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RESULT                          12888888
    RESULT                          19999999
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

