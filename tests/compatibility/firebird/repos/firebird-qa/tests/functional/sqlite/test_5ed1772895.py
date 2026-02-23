#coding:utf-8

"""
ID:          5ed1772895
ISSUE:       https://www.sqlite.org/src/tktview/5ed1772895
TITLE:       Incorrect ORDER BY on an indexed JOIN
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
    create table t1(a int unique not null, b int not null);
    create index t1ba on t1(b,a);
    create table t2(x int not null references t1(a), y int not null);
    create unique index t2xy on t2(x,y);
    insert into t1 values(1,1);
    insert into t1 values(3,1);
    insert into t2 values(1,13);
    insert into t2 values(1,15);
    insert into t2 values(3,14);
    insert into t2 values(3,16);

    set count on;
    select b, y from t1 cross join t2 where x=a order by b, y;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    B 1
    Y 13
    B 1
    Y 14
    B 1
    Y 15
    B 1
    Y 16
    Records affected: 4
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
