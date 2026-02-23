#coding:utf-8

"""
ID:          f74beaabde
ISSUE:       https://www.sqlite.org/src/tktview/f74beaabde
TITLE:       Problem with 3-way joins and the USING clause
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

    create table t1(a char(3));
    create table t2(a char(3));
    create table t3(a char(3), b char(3));
    insert into t1 values('abc');
    insert into t3 values('abc', 'def');

    select 'point-1' as msg from rdb$database;
    set count on;
    select * from t1 left join t2 using(a) left join t3 using(a); -- abc||
    select * from t1 left join t2 using(a) left join t3 on t3.a=t2.a; -- abc|||
    select * from t1 left join t2 using(a) left join t3 on t3.a=t1.a; -- abc||abc|def
    set count off;
    commit;
    -------------
    recreate table t1(w int, x int);
    recreate table t2(x int, y int);
    recreate table t3(w int, z int);

    select 'point-2' as msg from rdb$database;
    set count on;
    select * from t1 join t2 using(x) join t3 using(w);
    set count off;
    commit;
    -------------
    recreate table t1(a int,x int,y int);
    recreate table t2(b int,y int,z int);
    recreate table t3(c int,x int,z int);

    insert into t1 values(1,91,92);
    insert into t1 values(2,93,94);
    insert into t2 values(3,92,93);
    insert into t2 values(4,94,95);
    insert into t3 values(5,91,93);
    insert into t3 values(6,99,95);

    select 'point-3' as msg from rdb$database;
    set count on;
    select * from t1 natural join t2 natural join t3;
    set count off;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MSG point-1
    A abc
    B def
    Records affected: 1
    A abc
    A <null>
    B <null>
    Records affected: 1
    A abc
    A abc
    B def
    Records affected: 1
    
    MSG point-2
    Records affected: 0
    
    MSG point-3
    A 1
    X 91
    Y 92
    B 3
    Z 93
    C 5
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
