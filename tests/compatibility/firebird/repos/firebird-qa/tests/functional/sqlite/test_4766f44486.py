#coding:utf-8

"""
ID:          4766f44486
ISSUE:       https://www.sqlite.org/src/tktview/4766f44486
TITLE:       ORDER BY handling with indexes on expressions
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(x int, y int);
    insert into t1 values(1, 1);
    insert into t1 values(1, 2);
    insert into t1 values(2, 2);
    insert into t1 values(2, 1);

    set count on;
    select * from t1 order by x+0, y;
    commit;
    create index i1 on t1 computed by(x+0);
    select * from t1 order by x+0, y;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    X 1
    Y 1
    X 1
    Y 2
    X 2
    Y 1
    X 2
    Y 2
    Records affected: 4

    X 1
    Y 1
    X 1
    Y 2
    X 2
    Y 1
    X 2
    Y 2
    Records affected: 4
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
