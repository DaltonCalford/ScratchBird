#coding:utf-8

"""
ID:          7fde638e94
ISSUE:       https://www.sqlite.org/src/tktview/7fde638e94
TITLE:       Assertion fault on a LEFT JOIN
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
    create table t1(a int);
    create table t3(x int);

    insert into t1 values(1);
    insert into t1 values(2);
    insert into t1 values(3);
    commit;

    create view v2 as select a, 1 as b from t1;

    insert into t3 values(2);
    insert into t3 values(4);

    set count on;
    select * from t3 left join v2 on a=x where b=1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    X 2
    A 2
    B 1
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
