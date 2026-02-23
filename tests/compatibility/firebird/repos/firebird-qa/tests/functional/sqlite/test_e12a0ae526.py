#coding:utf-8

"""
ID:          e12a0ae526
ISSUE:       https://www.sqlite.org/src/tktview/e12a0ae526
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
    create table test_0(f01 int, f02 int);
    create table test_1(f04 int, f05 int);
    create index test_2 on test_1(f05, f04);
    insert into test_0 values(0, 8);

    set count on;
    select (select min(f04) from test_0 left join test_1 on test_1.f05 is null) from test_0;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MIN <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
