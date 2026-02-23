#coding:utf-8

"""
ID:          82b588d342
ISSUE:       https://www.sqlite.org/src/tktview/
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
    recreate table test_a(v1 int);
    recreate table test_b(v5 int, v4 int);
    create index test_b_v4_v5 on test_b(v4, v5);
    insert into test_a values(0);

    set count on;
    select test_b.v5 from test_a left join test_b on test_b.v4 is null and test_b.v5 in( 0 );
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    V5 <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
