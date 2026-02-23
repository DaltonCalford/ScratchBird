#coding:utf-8

"""
ID:          f2369304e4
ISSUE:       https://www.sqlite.org/src/tktview/f2369304e4
TITLE:       Incorrect results when OR is used in the ON clause of a LEFT JOIN
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
    create table t2(y integer primary key, a int, b int);
    insert into t1 values(1);
    insert into t2 values(1,2,3);
    set count on;
    select * from t1 left join t2 on a=2 or b=3 where y is null;
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
