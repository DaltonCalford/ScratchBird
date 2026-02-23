#coding:utf-8

"""
ID:          623eff57e7
ISSUE:       https://www.sqlite.org/src/tktview/623eff57e7
TITLE:       LEFT JOIN in view malfunctions with partial index on table
DESCRIPTION:
NOTES:
    [15.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0(c0 int);
    create table t1(c0 int);
    create index i0 on t0 computed by (0) where null in (c0);
    create view v0(c0) as select t0.c0 from t1 left join t0 using(c0);
    insert into t1(c0) values (0);

    set count on;
    select count(*) from v0 where null in (v0.c0);
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    COUNT 0
    Records affected: 1
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
