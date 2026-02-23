#coding:utf-8

"""
ID:          d0866b26f8
ISSUE:       https://www.sqlite.org/src/tktview/d0866b26f8
TITLE:       Window function in correlated subquery causes assertion fault
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    CREATE TABLE t1(x varchar(1));
    insert into t1 values('1');
    insert into t1 values('2');
    insert into t1 values('3');

    create table t2(a varchar(1), b int);
    insert into t2 values('x', 1);
    insert into t2 values('x', 2);
    insert into t2 values('y', 2);
    insert into t2 values('y', 3);

    set count on;
    select x, (
      select sum(b)
        over (partition by a rows between unbounded preceding and unbounded following)
      from t2 where  b < x
      rows 1
    ) from t1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    X 1
    SUM <null>

    X 2
    SUM 1

    X 3
    SUM 3

    Records affected: 3
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
