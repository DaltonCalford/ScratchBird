#coding:utf-8

"""
ID:          1dcb4d4496
ISSUE:       https://www.sqlite.org/src/tktview/
TITLE:       Incorrect query result when redundant terms appears in WHERE clause
DESCRIPTION:
NOTES:
    [15.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(x varchar(5));
    create table t2(y varchar(5));
    create index t1_x on t1(x);
    create index t2_y on t2(y);

    insert into t1 values('good');
    insert into t1 values('bad');
    insert into t2 values('good');
    insert into t2 values('bad');
    set count on;
    -- set plan on;
    select *
    from t1
    join t2 on x = y
    where x='good' and y='good';
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    X good
    Y good
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
