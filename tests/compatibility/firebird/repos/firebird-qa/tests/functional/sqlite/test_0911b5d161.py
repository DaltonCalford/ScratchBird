#coding:utf-8

"""
ID:          0911b5d161
ISSUE:       https://www.sqlite.org/src/tktview/0911b5d161
TITLE:       Assertion
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    NB: 3.x raises "SQLSTATE = 22011 / Invalid offset parameter ... to SUBSTRING. Only positive integers are allowed."
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0 (c0 int);
    insert into t0(c0) values (0x00);
    set count on;
    select * from t0 where cast(substring(c0 from 0) as varchar(10)) >= 0; 
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 0
    Records affected: 1
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
