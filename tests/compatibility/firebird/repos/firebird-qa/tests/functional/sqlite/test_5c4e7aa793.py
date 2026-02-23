#coding:utf-8

"""
ID:          5c4e7aa793
ISSUE:       https://www.sqlite.org/src/tktview/5c4e7aa793
TITLE:       Incorrect result for comparison with NULL
DESCRIPTION:
NOTES:
    [14.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0(c0 int primary key using descending index t0_pk_desc);
    insert into t0(c0) values (0);
    set count on;
    select * from t0 where t0.c0 > null; 
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
