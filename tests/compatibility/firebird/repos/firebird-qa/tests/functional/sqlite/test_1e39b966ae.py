#coding:utf-8

"""
ID:          1e39b966ae
ISSUE:       https://www.sqlite.org/src/tktview/1e39b966ae
TITLE:       LEFT JOIN strength reduction optimization causes an error.
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
    create table t1(a integer primary key, b varchar(10));
    create table t2(x integer primary key, y varchar(10));
    insert into t1(a,b) values(1,null);

    set count on;
    select t1.*, b is not null and y='xyz' as chk
    from t1
    left join t2 on b = x
    ;
    select a
    from t1
    left join t2 on (b=x)
    where not ( b is not null and y='xyz' );
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 1
    B <null>
    CHK <false>
    Records affected: 1

    A 1
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
