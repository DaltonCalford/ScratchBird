#coding:utf-8

"""
ID:          dfd66334cf
ISSUE:       https://www.sqlite.org/src/tktview/dfd66334cf
TITLE:       Assertion
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
    create table t0(c0 int);
    create table t1(c0 int);
    insert into t0 values(0);
    insert into t1 values(0);
    set count on;
    select * from t0 left join t1 using(c0) where (t1.c0 between 0 and 0) > ('false' = (t0.c0 = 0));
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
