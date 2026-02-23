#coding:utf-8

"""
ID:          fba33c8b1d
ISSUE:       https://www.sqlite.org/src/tktview/fba33c8b1d
TITLE:       Partial index causes row to not be fetched in BETWEEN expression
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0(c1 boolean);
    create index i0 on t0 computed by(1) where c1 is not null;
    insert into t0(c1) values (null);
    set count on;
    select * from t0 where t0.c1 is false between false and true;
    select * from t0 where t0.c1 = false between false and true;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C1 <null>
    Records affected: 1
    Records affected: 0
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
