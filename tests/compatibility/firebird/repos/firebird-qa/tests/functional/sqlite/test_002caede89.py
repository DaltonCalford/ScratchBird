#coding:utf-8

"""
ID:          002caede89
ISSUE:       https://www.sqlite.org/src/tktview/002caede89
TITLE:       LEFT JOIN with OR terms in WHERE clause causes assertion fault
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
    create table t1(a int, b int, c int, d int);
    create table t2(e int, f int);
    create index t1a on t1(a);
    create unique index t1b on t1(b);

    create table t3(g int);
    create table t4(h int);

    insert into t1 values(1,2,3,4);
    insert into t2 values(10,-8);

    insert into t3 values(4);
    insert into t4 values(5);

    set count on;
    select * from t3
    left join t1 on d=g
    left join t4 on c=h
    where (a=1 and h=3)
         or b in (
               select x+1 
               from (
                   select e+f as x, e
                   from t2
                   order by 1 rows 2
               )
               group by x
            );
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
