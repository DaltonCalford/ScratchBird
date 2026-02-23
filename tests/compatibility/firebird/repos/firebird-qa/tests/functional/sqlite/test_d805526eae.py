#coding:utf-8

"""
ID:          d805526eae
ISSUE:       https://www.sqlite.org/src/tktview/d805526eae
TITLE:       Incorrect join result or assertion fault due to transitive constraints
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
    create table t1(w integer primary key, x int);
    create table t2(y integer, z int);
    insert into t1 values(1,2);
    insert into t2 values(1,3);

    set count on;
    select *
    from t1 cross join t2
    where w=y and y is not null;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    W 1
    X 2
    Y 1
    Z 3
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
