#coding:utf-8

"""
ID:          7f39060a24
ISSUE:       https://www.sqlite.org/src/tktview/7f39060a24
TITLE:       LEFT JOIN malfunctions with partial index (unexpected fetch 1 row).
DESCRIPTION:
NOTES:
    [15.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0(c0 int);
    create table t1(c0 int);
    create index i0 on t0 computed by(1) where c0 is null;
    insert into t0(c0) values (1);
    insert into t1(c0) values (1);

    set count on;
    select t1.c0 as t1_c0, t0.c0 as t0_co
    from t1 left join t0 using(c0)
    where t0.c0 is null;

"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
