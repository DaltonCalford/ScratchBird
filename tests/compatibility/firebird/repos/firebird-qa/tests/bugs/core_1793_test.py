#coding:utf-8

"""
ID:          issue-2219
ISSUE:       2219
TITLE:       AV at prepare of query with unused parametrized CTE
DESCRIPTION:
JIRA:        CORE-1793
FBTEST:      bugs.core_1793
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Updated 06.01.2018:
    --  25SC, build 2.5.8.27089: OK, 0.297s.
    --  30SS, build 3.0.3.32861: OK, 1.578s.
    --  40SS, build 4.0.0.840: OK, 1.390s.
    recreate table test(x int);
    commit;
    set planonly;
    with
        x as (select x.x from test x),
        y as (select y.x from test y),
        z as (select x from test)
    select * from y;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    SQL warning code = -104
    -CTE "X" is not used in query
    -CTE "Z" is not used in query
    PLAN (Y Y NATURAL)
"""

expected_stdout_6x = """
    SQL warning code = -104
    -CTE "X" is not used in query
    -CTE "Z" is not used in query
    PLAN ("Y" "Y" NATURAL)
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
