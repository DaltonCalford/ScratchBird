#coding:utf-8

"""
ID:          c997b11c4d
ISSUE:       https://www.sqlite.org/src/tktview/c997b11c4d
TITLE:       ORDER BY clause ignored in 3-way join query
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
    create table t1(a integer primary key);
    create table t2(b integer primary key, c integer);
    create table t3(d integer);

    insert into t1 values(1);
    insert into t1 values(2);
    insert into t1 values(3);

    insert into t2 values(3, 1);
    insert into t2 values(4, 2);
    insert into t2 values(5, 3);

    insert into t3 values(4);
    insert into t3 values(3);
    insert into t3 values(5);

    set count on;
    select t1.a from t1, t2, t3 where t1.a=t2.c and t2.b=t3.d order by t1.a;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 1
    A 2
    A 3
    Records affected: 3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
