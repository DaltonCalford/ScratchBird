#coding:utf-8

"""
ID:          a976c487d1
ISSUE:       https://www.sqlite.org/src/tktview/a976c487d1
TITLE:       LEFT JOIN in view malfunctions
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
    create table t0(c1 boolean);
    create table t1(c0 boolean);
    create view v0 as select t0.c1 from t1 left join t0 on t0.c1 = t1.c0;
    insert into t1 values (true);
    set count on;
    select * from v0 where not(v0.c1 is false);
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C1 <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
