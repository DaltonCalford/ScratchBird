#coding:utf-8

"""
ID:          892fc34f17
ISSUE:       https://www.sqlite.org/src/tktview/892fc34f17
TITLE:       Incorrect query result when a LEFT JOIN subquery is flattened
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(id integer primary key);
    create table t2(id integer primary key, c2 integer);
    create table t3(id integer primary key, c3 integer);

    insert into t1(id) values(456);
    insert into t3(id) values(1);
    insert into t3(id) values(2);

    set count on;
    select t1.id, x2.id, x3.id
    from t1
    left join (select * from t2) as x2 on t1.id=x2.c2
    left join t3 as x3 on x2.id=x3.c3;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID 456
    ID <null>
    ID <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
