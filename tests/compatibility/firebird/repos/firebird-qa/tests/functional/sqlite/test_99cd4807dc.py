#coding:utf-8

"""
ID:          99cd4807dc
ISSUE:       https://www.sqlite.org/src/tktview/99cd4807dc
TITLE:       Scalar subquery with "LIMIT 0" should return NULL
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
    create table t1(x int);
    insert into t1(x) values(1);
    set count on;
    select (select x from t1 rows 0) from rdb$database;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    X <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
