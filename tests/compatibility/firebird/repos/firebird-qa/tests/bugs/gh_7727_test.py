#coding:utf-8

"""
ID:          issue-7727
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7727
TITLE:       Index for integer column cannot be used when INT128/DECFLOAT value is being searched
DESCRIPTION:
NOTES:
    [31.08.2023] pzotov
        Confirmed problem on 5.0.0.1177, 4.0.4.2979
        Checked on 5.0.0.1183, 4.0.4.2983 (intermediate snapshots).
    [05.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.909; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = f"""
    recreate table test_a (
        id bigint not null,
        constraint pk_test primary key(id)
    );

    recreate table test_b (
        id bigint not null,
        constraint pk_test_b primary key(id)
    );


    set plan on;

    select ta.id, tb.id
    from test_a ta
    left join test_b tb on tb.id = ta.id / cast(1000 as int128);

    select ta.id, tb.id
    from test_a ta
    left join test_b tb on tb.id = ta.id / cast(1000 as decfloat);
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=4.0.4')
def test_1(act: Action):

    expected_stdout_5x = f"""
        PLAN JOIN (TA NATURAL, TB INDEX (PK_TEST_B))
        PLAN JOIN (TA NATURAL, TB INDEX (PK_TEST_B))
    """
    expected_stdout_6x = f"""
        PLAN JOIN ("TA" NATURAL, "TB" INDEX ("PUBLIC"."PK_TEST_B"))
        PLAN JOIN ("TA" NATURAL, "TB" INDEX ("PUBLIC"."PK_TEST_B"))
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
