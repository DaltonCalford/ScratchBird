#coding:utf-8

"""
ID:          issue-1493
ISSUE:       1493
TITLE:       Server crashes instead of reporting the error "key size exceeds implementation restriction"
DESCRIPTION:
JIRA:        CORE-4127
FBTEST:      bugs.core_4127
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table tab1 (col1 int, col2 char(10));
    create index itab1 on tab1 (col1, col2);
    commit;
    insert into tab1 values(1, 'a');
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select * from tab1
    where col1 = 1 and col2 = rpad('a', 32765)
    union all
    -- This part of query will NOT raise
    -- Statement failed, SQLSTATE = 54000
    -- arithmetic exception, numeric overflow, or string truncation
    -- -Implementation limit exceeded
    -- since WI-V3.0.0.31981
    select * from tab1
    where col1 = 1 and col2 = rpad('a', 32766);
"""

act = isql_act('db', test_script)

expected_stdout = """
    COL1                            1
    COL2                            a
    COL1                            1
    COL2                            a
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

