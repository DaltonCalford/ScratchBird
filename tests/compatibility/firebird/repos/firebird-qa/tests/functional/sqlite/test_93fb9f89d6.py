#coding:utf-8

"""
ID:          93fb9f89d6
ISSUE:       https://www.sqlite.org/src/tktview/93fb9f89d6
TITLE:       Index causes incorrect WHERE clause evaluation
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(x int,y int);
    create table t2(a int, b char);
    create index t2b on t2(b);

    insert into t1 values(1,2);
    insert into t1 values(2,7);
    insert into t1 values(3,4);
    insert into t2 values(1,2);
    insert into t2 values(5,6);
    set count on;
    select t1.*, t2.*, y = b as chk
    from t1, t2 where y=b;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    X 1
    Y 2
    A 1
    B 2
    CHK <true>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
