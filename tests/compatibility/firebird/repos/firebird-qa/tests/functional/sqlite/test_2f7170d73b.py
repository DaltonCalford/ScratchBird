#coding:utf-8

"""
ID:          2f7170d73b
ISSUE:       https://www.sqlite.org/src/tktview/2f7170d73b
TITLE:       Error "misuse of aggregate" raising if aggregate column in FROM subquery presents in the correlated subquery
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
    create table t1(x int);
    create table t2(y int, z int);
    set count on;
    select (select y from t2 where z = cnt) as v1
    from (
        select count(*) as cnt from t1
    );

"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    V1 <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
