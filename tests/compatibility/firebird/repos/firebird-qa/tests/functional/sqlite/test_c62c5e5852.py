#coding:utf-8

"""
ID:          c62c5e5852
ISSUE:       https://www.sqlite.org/src/tktview/c62c5e5852
TITLE:       Assertion 
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
    recreate table t0(c0 varchar(10));
    create index i0 on t0 computed by ('0' like coalesce(c0, 0));
    insert into t0(c0) values (null);
    insert into t0(c0) values (1);
    insert into t0(c0) values (-1);
    -- set plan on;
    set count on;
    select * from t0 where '0' like coalesce(c0, 0);
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
