#coding:utf-8

"""
ID:          a179fe7465
ISSUE:       https://www.sqlite.org/src/tktview/a179fe7465
TITLE:       Incorrect output order on a join with an ORDER BY
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a int, b int, primary key(a,b));
    create table t2(x int, y int, primary key(x,y));

    insert into t1 values(1,1);
    insert into t1 values(1,2);
    insert into t2 values(3,3);
    insert into t2 values(4,4);

    set count on;
    select 'query-1' msg, a, x from t1, t2 order by 1, 2;
    set count off;
    commit;

    ------------------------------------------------
    
    create table t3(a int);
    create index t3a on t3(a);
    create table t4(x int);
    create index t4x on t4(x);

    insert into t3 values(1);
    insert into t3 values(1);
    insert into t4 values(3);
    insert into t4 values(4);

    set count on;
    select 'query-2' msg, a, x from t3, t4 order by 1, 2;
    set count off;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MSG query-1
    A 1
    X 3
    MSG query-1
    A 1
    X 3
    MSG query-1
    A 1
    X 4
    MSG query-1
    A 1
    X 4
    Records affected: 4
    
    MSG query-2
    A 1
    X 3
    MSG query-2
    A 1
    X 3
    MSG query-2
    A 1
    X 4
    MSG query-2
    A 1
    X 4
    Records affected: 4
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
