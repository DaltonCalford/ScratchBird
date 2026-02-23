#coding:utf-8

"""
ID:          issue-5144
ISSUE:       5144
TITLE:       MERGE ... WHEN NOT MATCHED ... RETURNING returns wrong (non-null) values when no insert is performed
DESCRIPTION:
JIRA:        CORE-4848
FBTEST:      bugs.core_4848
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    recreate table t1 (n1 integer, n2 integer);

    -- Case 1:
    merge into t1
    using (
        select 1 x
        from rdb$database
        where 1 = 0
    ) on 1 = 1
    when not matched then
        insert values (1, 11)
        returning n1, n2;

    -- Case 2:
    merge into t1
    using (
        select 1 x
        from rdb$database
        where 1 = 1
    ) on 1 = 0
    when not matched and 1 = 0 then
        insert values (1, 11)
        returning n1, n2;
"""

act = isql_act('db', test_script)

# version: 3.0

expected_stdout_1 = """
    N1                              <null>
    N2                              <null>

    N1                              <null>
    N2                              <null>
"""

@pytest.mark.version('>=3.0,<5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

# version: 5.0

expected_stdout_2 = """
    Records affected: 0
    Records affected: 0
"""

@pytest.mark.version('>=5.0')
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

