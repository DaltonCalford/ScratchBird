#coding:utf-8

"""
ID:          c5ea805691
ISSUE:       https://www.sqlite.org/src/tktview/c5ea805691
TITLE:       Inverted sort order when using DISTINCT and a descending index
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
    insert into t1 values(3);
    insert into t1 values(1);
    insert into t1 values(5);
    insert into t1 values(2);
    insert into t1 values(6);
    insert into t1 values(4);
    insert into t1 values(5);
    insert into t1 values(1);
    insert into t1 values(3);
    commit;
    create descending index t1x on t1(x);
    set count on;
    select distinct x from t1 order by x asc;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    X 1
    X 2
    X 3
    X 4
    X 5
    X 6
    Records affected: 6
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
