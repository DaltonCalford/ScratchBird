#coding:utf-8

"""
ID:          f00d096cae
ISSUE:       https://www.sqlite.org/src/tktview/f00d096cae
TITLE:       Assertion when use 'IN()' with dense_rank()over() and lag()over()
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
    create table t0(c0 int unique);
    insert into t0 values(0);

    set count on;
    select * from t0 where -- (0, t0.c0) in(select dense_rank() over(), lag(0) over() from t0); 
    exists
    (
        select 1
        from t0
        join (
            select dense_rank() over() as t0_rnk, lag(0) over() as t0_lag from t0
        ) tx on t0.c0 = coalesce(tx.t0_lag,0)
    );

"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 0
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
