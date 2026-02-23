#coding:utf-8

"""
ID:          issue-5695
ISSUE:       5695
TITLE:       Regression: "Invalid usage of boolean expression" when use "BETWEEN" and "IS" operators
DESCRIPTION:
JIRA:        CORE-5423
FBTEST:      bugs.core_5423
NOTES:
    [01.07.2025] pzotov
    Refactored: suppress output of column name ('foo') that is unknown - it has no matter for this test.
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    select 1 k from rdb$database where 1 between 0 and 2 and null is null;
    select 2 k from rdb$database where 1 between 0 and 2 and foo is not null;
"""

substitutions = [ ('[ \t]+', ' '), ('(-)?(")?FOO(")?', ''), (r'(-)?At line(:)?\s+\d+.*', '') ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    K 1
    Records affected: 1
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

