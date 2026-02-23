#coding:utf-8

"""
ID:          issue-5041
ISSUE:       5041
TITLE:       Expression 'where bool_field IS true | false' should also use index as 'where bool_field = true | false' (if such index exists)
DESCRIPTION:
JIRA:        CORE-4735
FBTEST:      bugs.core_4735
NOTES:
    [28.01.2019] pzotov
    Changed expected PLAN of execution after dimitr's letter 28.01.2019 17:28:
    'is NOT <bool>' and 'is distinct from <bool>' should use  PLAN NATURAL.

    [30.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test(x boolean, unique(x) using index test_x);
    commit;

    set plan on;
    select 1 from test where x = true ;
    select 1 from test where x is true ;
    select 0 from test where x = false ;
    select 0 from test where x is false ;
    select 1 from test where x is not true ; -- this must have plan NATURAL, 26.01.2019
    select 1 from test where x is distinct from true ; -- this must have plan NATURAL, 26.01.2019
    select 1 from test where x is not false ; -- this must have plan NATURAL, 26.01.2019
    select 1 from test where x is distinct from false ; -- this must have plan NATURAL, 26.01.2019
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN (TEST INDEX (TEST_X))
    PLAN (TEST INDEX (TEST_X))
    PLAN (TEST INDEX (TEST_X))
    PLAN (TEST INDEX (TEST_X))
    PLAN (TEST NATURAL)
    PLAN (TEST NATURAL)
    PLAN (TEST NATURAL)
    PLAN (TEST NATURAL)
"""

expected_stdout_6x = """
    PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_X"))
    PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_X"))
    PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_X"))
    PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_X"))
    PLAN ("PUBLIC"."TEST" NATURAL)
    PLAN ("PUBLIC"."TEST" NATURAL)
    PLAN ("PUBLIC"."TEST" NATURAL)
    PLAN ("PUBLIC"."TEST" NATURAL)
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
