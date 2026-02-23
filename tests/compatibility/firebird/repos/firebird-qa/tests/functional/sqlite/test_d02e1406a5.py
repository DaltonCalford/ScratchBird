#coding:utf-8

"""
ID:          d02e1406a5
ISSUE:       https://www.sqlite.org/src/tktview/d02e1406a5
TITLE:       LEFT JOIN with an OR in the ON clause causes segfault
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
    create table t1(a int, b int, c int);
    create table t2(d int, e int, f int);

    insert into t1 values(1,2,3);
    insert into t1 values (4,5,6);

    insert into t2 values(3,6,9);
    insert into t2 values(4,8,12);

    set count on;
    select * from t1 as x left join t2 as y on (y.d=x.c) or (y.e=x.b);
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 1
    B 2
    C 3
    D 3
    E 6
    F 9
    
    A 4
    B 5
    C 6
    D <null>
    E <null>
    F <null>
    
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
