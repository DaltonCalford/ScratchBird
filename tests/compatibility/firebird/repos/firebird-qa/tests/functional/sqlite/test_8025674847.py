#coding:utf-8

"""
ID:          8025674847
ISSUE:       https://www.sqlite.org/src/tktview/8025674847
TITLE:       Incorrect use of "WHERE x NOT NULL" partial index for query with a "WHERE x IS NOT ?" term
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0 (c0 boolean, c1 int);
    create index i0 on t0(c0,c1) where c0 is null;
    insert into t0(c0) values(null);
    set count on;
    select * from t0 where t0.c0 is not true;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 <null>
    C1 <null>
    Records affected: 1
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
