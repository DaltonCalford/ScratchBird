#coding:utf-8

"""
ID:          57c47526c3
ISSUE:       https://www.sqlite.org/src/tktview/57c47526c3
TITLE:       Incorrect answer when flattening a UNION ALL compound
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
    create table t0(c0 int);
    create view v0(c0) as select cast(t0.c0 as integer) from t0;
    create table t1_a(a integer primary key, b char(10));
    create table t1_b(c integer primary key, d char(10));
    create view t1 as 
      select a, b from t1_a
      union all
      select c, c from t1_b
    ;
    commit;

    insert into t0 values(0);
    insert into t1_a values(1,'one');
    insert into t1_a values(4,'four');
    insert into t1_b values(2,'two');
    insert into t1_b values(5,'five');

    set count on;
    select * from (
      select t1.a as a, t1.b as b, t0.c0 as c, v0.c0 as d
        from t0 left join v0 on v0.c0>'0',t1
    ) as t2 where b='2';

    select * from (
      select t1.a, t1.b, t0.c0 as c, v0.c0 as d
        from t0 left join v0 on v0.c0>'0',t1
    ) as t2 where b='2';

"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 2
    B 2
    C 0
    D <null>
    Records affected: 1

    A 2
    B 2
    C 0
    D <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
