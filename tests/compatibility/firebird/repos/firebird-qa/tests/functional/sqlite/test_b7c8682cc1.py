#coding:utf-8

"""
ID:          b7c8682cc1
ISSUE:       https://www.sqlite.org/src/tktview/b7c8682cc1
TITLE:       Incorrect result from LEFT JOIN with OR in the WHERE clause
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a integer primary key, b int, c int, d int);
    create table t2(x integer primary key, y int);
    create table t3(p integer primary key, q int);
    insert into t1 values(2,3,4,5);
    insert into t1 values(3,4,5,6);
    insert into t2 values(2,4);
    insert into t3 values(5,55);

    set count on;
    select *
    from t1 left join t2 on t2.y = t1.b cross join t3
    where t1.c = t3.p or t1.d = t3.p;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 2
    B 3
    C 4
    D 5
    X <null>
    Y <null>
    P 5
    Q 55
    
    A 3
    B 4
    C 5
    D 6
    X 2
    Y 4
    P 5
    Q 55
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
