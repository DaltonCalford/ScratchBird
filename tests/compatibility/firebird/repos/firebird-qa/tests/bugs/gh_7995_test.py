#coding:utf-8

"""
ID:          issue-7995
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7995
TITLE:       Unexpected results after creating partial index
NOTES:
    [07.02.2024] pzotov
    Confirmed bug on 6.0.0.249, 5.0.1.1331.
    Checked on 5.0.1.1332 (commit ffb54229).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t0(c0 boolean);
    recreate table t1(c0 int, c1 int);
    create unique index t1i0 on t1(c0) where ( t1.c0 is not null );

    insert into t0 (c0) values (true);
    insert into t0 (c0) values (false);
    insert into t1 (c0, c1) values (0, 1);
    insert into t1 (c0) values (1);
    insert into t1 (c0) values (2);
    insert into t1 (c0) values (3);
    insert into t1 (c0) values (4);
    insert into t1 (c0) values (5);
    insert into t1 (c0) values (6);
    insert into t1 (c0) values (7);
    insert into t1 (c0) values (8);
    insert into t1 (c0) values (9);
    insert into t1 (c0) values (10); -- at least 11 rows data

    set heading off;
    set count on;

    select ((true or t1.c1 > 0)and(t0.c0)) from t1 cross join t0; -- 11 rows of true

    select t1.c0 as t1_c0, t1.c1 as t1_c1, t0.c0 as t0_c0 from t1 cross join t0 where ( (true or t1.c1 > 0) and t0.c0);
"""

act = isql_act('db', test_script, substitutions = [('[ \t]+', ' ')])

expected_stdout = """
    <true>
    <true>
    <true>
    <true>
    <true>
    <true>
    <true>
    <true>
    <true>
    <true>
    <true>
    <false>
    <false>
    <false>
    <false>
    <false>
    <false>
    <false>
    <false>
    <false>
    <false>
    <false>

    Records affected: 22

     0            1 <true>
     1       <null> <true>
     2       <null> <true>
     3       <null> <true>
     4       <null> <true>
     5       <null> <true>
     6       <null> <true>
     7       <null> <true>
     8       <null> <true>
     9       <null> <true>
    10       <null> <true>

    Records affected: 11
"""

@pytest.mark.version('>=5.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
