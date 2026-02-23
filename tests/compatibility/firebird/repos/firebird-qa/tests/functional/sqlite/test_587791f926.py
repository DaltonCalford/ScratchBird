#coding:utf-8

"""
ID:          587791f926
ISSUE:       https://www.sqlite.org/src/tktview/587791f926
TITLE:       Wrong result of COUNT when using WHERE clause with POSITION()
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
    create table t0(c0 char(8) character set octets primary key, c1 int);
    insert into t0(c0) values (x'bb');
    insert into t0(c0) values (0);
    select count(*) from (select * from t0 where position(x'aabb' in t0.c0) > 0 order by t0.c0, t0.c1); -- 1
    set count on;
    select * from t0 where position(x'aabb' in t0.c0) > 0 order by t0.c0, t0.c1; -- no row is fetched
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    COUNT 0
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
