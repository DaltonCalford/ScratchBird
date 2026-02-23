#coding:utf-8

"""
ID:          c2ad16f997
ISSUE:       https://www.sqlite.org/src/tktview/c2ad16f997
TITLE:       Segfault on query involving deeply nested aggregate views
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(x int, y int);
    create table ta(x int);
    create table tb(y int);

    set count on;
    --select max((select avg(x) from tb)) from ta;
    select max((select a from (select count(1) as a from t1))) as v1 from t1;
    select max((select a from (select avg(x) a from tb))) as v2 from ta;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    V1 <null>
    Records affected: 1

    V2 <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
