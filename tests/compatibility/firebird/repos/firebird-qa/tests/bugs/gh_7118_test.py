#coding:utf-8

"""
ID:          issue-7118
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7118
TITLE:       Chained JOIN .. USING across the same column names may be optimized badly
NOTES:
    [01.03.2023] pzotov
    Commit related to this test:
    https://github.com/FirebirdSQL/firebird/commit/1b192404d43a15d403b5ff92760bc5df9d3c89c3
    (13.09.2022 19:17, "More complete solution for #3357 and #7118")
    One more test that attempts to verify this commit: bugs/gh_7398_test.py
    Checked on 3.0.11.33665, 4.0.3.2904, 5.0.0.964

    [04.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t1 (id int primary key, col int);
    create index t1_col on t1 (col);
    recreate table t2 (id int primary key, col int);
    create index t2_col on t2 (col);
    recreate table t3 (id int primary key, col int);
    create index t3_col on t3 (col);

    set plan;
    select *
    from t1 t1
      inner join t2 t2 on t1.id = t2.id
      inner join t3 t3 on t1.id = t3.id
    where
      t1.col = 0
      and t2.col = 0
      and t3.col = 0;
    -- PLAN JOIN (T1 INDEX (T1_COL), T2 INDEX (RDB$PRIMARY11), T3 INDEX (RDB$PRIMARY12))

    select *
    from t1 t1
      inner join t2 t2 on t1.id = t2.id
      inner join t3 t3 on t2.id = t3.id
    where
      t1.col = 0
      and t2.col = 0
      and t3.col = 0;
    -- PLAN JOIN (T1 INDEX (T1_COL), T2 INDEX (RDB$PRIMARY11), T3 INDEX (RDB$PRIMARY12))

    select *
    from t1 t1
      inner join t2 t2 on t1.id = t2.id
      inner join t3 t3 on t1.id = t3.id and t2.id = t3.id
    where
      t1.col = 0
      and t2.col = 0
      and t3.col = 0;
    -- PLAN JOIN (T1 INDEX (T1_COL), T2 INDEX (RDB$PRIMARY11), T3 INDEX (RDB$PRIMARY12))

    select *
    from t1 t1
      inner join t2 t2 using (id)
      inner join t3 t3 using (id)
    where
      t1.col = 0
      and t2.col = 0
      and t3.col = 0;
    -- The last query with USING syntax gets a different (usually worse) plan.
    -- This issue is closely related to #3357, because using (id) is internally
    -- transformed into coalesce(t1.id, t2.id) = t3.id.
    -- PLAN HASH (JOIN (T1 INDEX (T1_COL), T2 INDEX (RDB$PRIMARY11)), T3 INDEX (T3_COL))
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0.9')
def test_1(act: Action):

    expected_stdout_5x = """
        PLAN JOIN (T1 INDEX (T1_COL), T2 INDEX (RDB$PRIMARY2), T3 INDEX (RDB$PRIMARY3))
        PLAN JOIN (T1 INDEX (T1_COL), T2 INDEX (RDB$PRIMARY2), T3 INDEX (RDB$PRIMARY3))
        PLAN JOIN (T1 INDEX (T1_COL), T2 INDEX (RDB$PRIMARY2), T3 INDEX (RDB$PRIMARY3))
        PLAN JOIN (T1 INDEX (T1_COL), T2 INDEX (RDB$PRIMARY2), T3 INDEX (RDB$PRIMARY3))
    """

    expected_stdout_6x = """
        PLAN JOIN ("T1" INDEX ("PUBLIC"."T1_COL"), "T2" INDEX ("PUBLIC"."RDB$PRIMARY2"), "T3" INDEX ("PUBLIC"."RDB$PRIMARY3"))
        PLAN JOIN ("T1" INDEX ("PUBLIC"."T1_COL"), "T2" INDEX ("PUBLIC"."RDB$PRIMARY2"), "T3" INDEX ("PUBLIC"."RDB$PRIMARY3"))
        PLAN JOIN ("T1" INDEX ("PUBLIC"."T1_COL"), "T2" INDEX ("PUBLIC"."RDB$PRIMARY2"), "T3" INDEX ("PUBLIC"."RDB$PRIMARY3"))
        PLAN JOIN ("T1" INDEX ("PUBLIC"."T1_COL"), "T2" INDEX ("PUBLIC"."RDB$PRIMARY2"), "T3" INDEX ("PUBLIC"."RDB$PRIMARY3"))
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
