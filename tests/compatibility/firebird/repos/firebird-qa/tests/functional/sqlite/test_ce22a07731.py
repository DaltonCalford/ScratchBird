#coding:utf-8

"""
ID:          ce22a07731
ISSUE:       https://www.sqlite.org/src/tktview/ce22a07731
TITLE:       NULL WHERE condition unexpectedly results in row being fetched
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
    create table t0 (c0 int default 1, c1 int unique, c2 int unique);
    insert into t0(c1) values (1);
    set count on;
    select * from t0 where 0 = t0.c2 or t0.c1 between t0.c2 and 1; 
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
