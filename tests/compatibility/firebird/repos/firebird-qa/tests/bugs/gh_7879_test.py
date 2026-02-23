#coding:utf-8

"""
ID:          issue-7879
ISSUE:       7879
TITLE:       Unexpected Results when Using Natural Right Join
DESCRIPTION:
NOTES:
    [27.11.2023] pzotov
    Checked 6.0.0.154, 5.0.0.1280
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t0(c0 int, c1 int);
    recreate table t1(c0 int);

    insert into t0(c0, c1) values (1, 2);
    insert into t1( c0) values (3);

    set list on;
    set count on;

    select * from t0 natural right join t1 where ((c0 in (c0, c1))); 
"""

act = isql_act('db', test_script)

expected_stdout = """
    C0                              3
    C1                              <null>
    Records affected: 1
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
