#coding:utf-8

"""
ID:          dc6ebeda93
ISSUE:       https://www.sqlite.org/src/tktview/dc6ebeda93
TITLE:       Incorrect DELETE due to the one-pass optimization
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(x int primary key using index t1_x);
    insert into t1 values(1);
    insert into t1 values(2);
    insert into t1 values(3);
    commit;
    set count on;
    -- set plan on;
    delete from t1 where exists(select 1 from t1 as v where v.x=t1.x-1);
    select * from t1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 2
    X 1
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
