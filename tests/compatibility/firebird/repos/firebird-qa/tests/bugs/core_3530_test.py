#coding:utf-8

"""
ID:          issue-3887
ISSUE:       3887
TITLE:       BETWEEN operand/clause not supported for COMPUTED columns -- "feature is not supported"
DESCRIPTION:
JIRA:        CORE-3530
FBTEST:      bugs.core_3530
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test2(id int);
    commit;

    recreate table test(
        x int,
        y int
    );

    recreate table test2(
        id int,
        z computed by
        (
           coalesce( (  select sum(
                                     case
                                         when (x = -1) then
                                            999
                                          else
                                            (coalesce(x, 0) - coalesce(y, 0))
                                     end
                                  )
                        from test
                        where x = test2.id
                      ),
                      0
                    )
        )
    );
    commit;

    set plan on;
    set count on;
    --set echo on;
    -- Before 3.0.2 following statement failed with:
    -- Statement failed, SQLSTATE = 0A000
    -- feature is not supported
    select * from test2 where z between 1 and 2;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN (TEST NATURAL)
    PLAN (TEST NATURAL)
    PLAN (TEST NATURAL)
    PLAN (TEST2 NATURAL)
    Records affected: 0
"""

expected_stdout_6x = """
    PLAN ("PUBLIC"."TEST" NATURAL)
    PLAN ("PUBLIC"."TEST" NATURAL)
    PLAN ("PUBLIC"."TEST" NATURAL)
    PLAN ("PUBLIC"."TEST2" NATURAL)
    Records affected: 0
"""

@pytest.mark.version('>=3.0.2')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
