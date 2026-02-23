#coding:utf-8

"""
ID:          be84e357c0
ISSUE:       https://www.sqlite.org/src/tktview/be84e357c0
TITLE:       Segfault during query involving LEFT JOIN column in the ORDER BY clause
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
    create table t1(a int);
    create table t2(b int, c int);
    insert into t1 values(1);
    set count on;
    select distinct a from t1 left join t2 on a=b order by c is null;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 1
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
