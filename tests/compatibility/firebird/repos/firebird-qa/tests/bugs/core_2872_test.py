#coding:utf-8

"""
ID:          issue-3256
ISSUE:       3256
TITLE:       EVL_expr: invalid operation (232)
DESCRIPTION:
JIRA:        CORE-2872
FBTEST:      bugs.core_2872
"""

import pytest
from firebird.qa import *

db_1 = db_factory()

test_script = """
    set list on;
    set count on;
    select 1 as i
    from rdb$database
    where count(*) >= all (select count(*) from rdb$database)
    ;
    select 1 as k
    from rdb$database
    where count(*) = (select count(*) from rdb$database)
    ;
"""

act = isql_act('db_1', test_script)

expected_stdout = """
    Records affected: 0
"""

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Cannot use an aggregate or window function in a WHERE clause, use HAVING (for aggregate only) instead
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

