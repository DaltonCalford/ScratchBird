#coding:utf-8

"""
ID:          issue-5449
ISSUE:       5449
TITLE:       Wrong error message with UNIQUE BOOLEAN field
DESCRIPTION:
JIRA:        CORE-5166
FBTEST:      bugs.core_5166
NOTES:
    [01.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    
    Checked on 6.0.0.884; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Confirmed: result is OK, builds: 3.0.0.32418, WI-T4.0.0.98
    set list on;
    recreate table test1 (
        x boolean,
        constraint test1_x_unq unique (x)
        using index test1_x_unq
    );
    commit;
    insert into test1(x) values (true);
    select * from test1;

    set count on;
    insert into test1(x) values (true);
    commit;
    set count off;

    recreate table test2 (
        u boolean,
        v boolean,
        w boolean,
        constraint test2_uvw_unq unique (u,v,w)
        using index test2_uvw_unq
    );
    commit;

    set count on;
    insert into test2 values( null, null, null);
    insert into test2 values( true, true, true);
    insert into test2 values( true, null, true);
    insert into test2 values( true, null, null);
    insert into test2 values( null, true, null);
    insert into test2 values( null, null, null);
    insert into test2 values( true, true, true);
    update test2 set u=true, v=null, w=true where coalesce(u,v,w) is null rows 1;
"""

substitutions = [] # [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    expected_stdout_5x = """
        X                               <true>
        Statement failed, SQLSTATE = 23000
        violation of PRIMARY or UNIQUE KEY constraint "TEST1_X_UNQ" on table "TEST1"
        -Problematic key value is ("X" = TRUE)
        Records affected: 0
        Records affected: 1
        Records affected: 1
        Records affected: 1
        Records affected: 1
        Records affected: 1
        Records affected: 1
        Statement failed, SQLSTATE = 23000
        violation of PRIMARY or UNIQUE KEY constraint "TEST2_UVW_UNQ" on table "TEST2"
        -Problematic key value is ("U" = TRUE, "V" = TRUE, "W" = TRUE)
        Records affected: 0
        Statement failed, SQLSTATE = 23000
        violation of PRIMARY or UNIQUE KEY constraint "TEST2_UVW_UNQ" on table "TEST2"
        -Problematic key value is ("U" = TRUE, "V" = NULL, "W" = TRUE)
        Records affected: 0
    """

    expected_stdout_6x = """
        X                               <true>
        Statement failed, SQLSTATE = 23000
        violation of PRIMARY or UNIQUE KEY constraint "TEST1_X_UNQ" on table "PUBLIC"."TEST1"
        -Problematic key value is ("X" = TRUE)
        Records affected: 0
        Records affected: 1
        Records affected: 1
        Records affected: 1
        Records affected: 1
        Records affected: 1
        Records affected: 1
        Statement failed, SQLSTATE = 23000
        violation of PRIMARY or UNIQUE KEY constraint "TEST2_UVW_UNQ" on table "PUBLIC"."TEST2"
        -Problematic key value is ("U" = TRUE, "V" = TRUE, "W" = TRUE)
        Records affected: 0
        Statement failed, SQLSTATE = 23000
        violation of PRIMARY or UNIQUE KEY constraint "TEST2_UVW_UNQ" on table "PUBLIC"."TEST2"
        -Problematic key value is ("U" = TRUE, "V" = NULL, "W" = TRUE)
        Records affected: 0
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
