#coding:utf-8

"""
ID:          c31034044b
ISSUE:       https://www.sqlite.org/src/tktview/c31034044b
TITLE:       LEFT JOIN in view malfunctions with NOTNULL
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
    create table t1(c1 int);
    create view v0(c0) as select t1.c1 from t0 left join t1 on t0.c0 = t1.c1;

    insert into t0(c0) values(0);
    set count on;
    select * from v0 where v0.c0 is not null is not null;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
