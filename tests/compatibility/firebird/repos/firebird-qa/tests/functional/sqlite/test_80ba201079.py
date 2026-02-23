#coding:utf-8

"""
ID:          80ba201079
ISSUE:       https://www.sqlite.org/src/tktview/80ba201079
TITLE:       Bug involving subqueries and the OR optimization
DESCRIPTION:
    SELECT statements can return incorrect results in certain cases where the following are true:
        * The query is a join,
        * The query takes advantage of the OR optimization,
        * At least one branch of the optimized OR expression in the WHERE clause involves a subquery.
        * At least one branch of the optimized OR expression in the WHERE clause refers to a column
          of a table other than the table to which the OR optimization applies.
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a char);
    create index i1 on t1(a);
    create table t2(b char);
    create table t3(c char);

    insert into t1 values('a');
    insert into t2 values('b');
    insert into t3 values('c');

    set count on;
    select *
    from t1 cross join t2
    where
        (a = 'a' and b = 'x')
        or
        (  a = 'a'
           and
           exists (select * from t3 where c = 'c')
        );
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A a
    B b
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
