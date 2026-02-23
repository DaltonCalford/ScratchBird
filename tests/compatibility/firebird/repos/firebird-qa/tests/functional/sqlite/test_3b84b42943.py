#coding:utf-8

"""
ID:          3b84b42943
ISSUE:       https://www.sqlite.org/src/tktview/3b84b42943
TITLE:       LEFT JOIN malfunctions with generated column
DESCRIPTION:
NOTES:
    [15.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0(c0 int);
    create table t1(c0 int, c1 boolean default true);
    insert into t0(c0) values(0);

    set count on;
    select t1.c1 is true as chk
    from t0 left join t1 using(c0);
    select t1.c1 is true as chk
    from t0 natural left join t1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    CHK <false>
    Records affected: 1

    CHK <false>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
