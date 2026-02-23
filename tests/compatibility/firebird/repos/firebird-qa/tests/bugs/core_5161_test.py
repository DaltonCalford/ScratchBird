#coding:utf-8

"""
ID:          issue-5444
ISSUE:       5444
TITLE:       Unique index could be created on non-unique data
DESCRIPTION:
    Confirmed on: WI-V3.0.0.32378, WI-V2.5.6.26980:
    one might to create unique index when number of inserted rows was >= 3276.
JIRA:        CORE-5161
FBTEST:      bugs.core_5161
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

    set list on;
    recreate table t (id int, x int);
    set term ^;
    execute block as
        declare i int = 0;
    begin
        --while (i < 100000) do
        while (i < 50000) do -- minimal number for reproduce: 3276
        begin
            insert into t values ( :i, iif(:i=1, -888888888, -:i) );
            i = i + 1;
        end
    end
    ^
    set term ;^
    select sign(count(*)) as cnt_non_zero from t;

    insert into t values(1, -999999999);
    commit;

    create unique index t_id_unique on t(id);

    set plan on;
    select id, x from t where id = 1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    CNT_NON_ZERO                    1
    Statement failed, SQLSTATE = 23000
    attempt to store duplicate value (visible to active transactions) in unique index "T_ID_UNIQUE"
    -Problematic key value is ("ID" = 1)
    PLAN (T NATURAL)
    ID                              1
    X                               -888888888
    ID                              1
    X                               -999999999
"""

expected_stdout_6x = """
    CNT_NON_ZERO                    1
    Statement failed, SQLSTATE = 23000
    attempt to store duplicate value (visible to active transactions) in unique index "PUBLIC"."T_ID_UNIQUE"
    -Problematic key value is ("ID" = 1)
    PLAN ("PUBLIC"."T" NATURAL)
    ID                              1
    X                               -888888888
    ID                              1
    X                               -999999999
"""

@pytest.mark.version('>=2.5.6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
