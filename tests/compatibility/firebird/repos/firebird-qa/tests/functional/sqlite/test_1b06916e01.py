#coding:utf-8

"""
ID:          1b06916e01
ISSUE:       https://www.sqlite.org/src/tktview/1b06916e01
TITLE:       Assertion
DESCRIPTION:
NOTES:
    [17.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0(c0 int, c1 int check(c1 in(c1)), c2 int check (c2 not in(c0,c1)));
    insert into t0(c1) values('0');
    insert into t0(c0,c1) values('-1','-2');
    -- insert into t0(c0,c1,c2) values('-3','-4', '-3');
    set count on;
    select c0,c1,c2,c2 not in(c0,c1) as chk from t0;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 <null>
    C1 0
    C2 <null>
    CHK <null>

    C0 -1
    C1 -2
    C2 <null>
    CHK <null>

    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
