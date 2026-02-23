#coding:utf-8

"""
ID:          d9ed4ebef1
ISSUE:       https://www.sqlite.org/src/tktview/d9ed4ebef1
TITLE:       SELECT on window function FIRST_VALUE()OVER() causes a segfault
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
    set count on;
    create table t0(c0 int unique);
    select * from t0 where c0 in (select first_value(0)over() from t0);
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
