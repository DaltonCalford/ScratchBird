#coding:utf-8

"""
ID:          issue-3357
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/3357
TITLE:       Bad execution plan if some stream depends on multiple streams via a function [CORE2975]
NOTES:
    [04.03.2023] pzotov
         1. Discussed with dimitr, letters 01-mar-2023 18:37 and 04-mar-2023 10:38.
            Test must verify that execution plan uses NESTED LOOPS rather than HASH JOIN.
            Because of this, tables must be filled with approximately equal volume of data.
            Confirmed bug on 3.0.9.33548 (28-dec-2021), plan was:
                PLAN HASH (JOIN (T1 INDEX (T1_COL), T2 INDEX (T2_ID)), T3 NATURAL)
         2. Commit related to this test:
            https://github.com/FirebirdSQL/firebird/commit/1b192404d43a15d403b5ff92760bc5df9d3c89c3
            (13.09.2022 19:17, "More complete solution for #3357 and #7118")
            One more test that attempts to verify this commit: bugs/gh_7398_test.py
         Checked on 5.0.0.970, 4.0.3.2904, 3.0.11.33665.
    [26.06.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    create table T1 (ID int, COL int);
    create table T2 (ID int);
    create table T3 (ID int);
    create view v_data as
    select iif(mod(i,2) = 0, null, i) as id, iif(mod(i,2) = 1, null, i) as col
    from (
        select row_number()over() as i
        from rdb$types a, rdb$types b
        rows 5000
    );
    commit;

    insert into t1 select id, col from v_data;
    insert into t2 select id from v_data;
    insert into t3 select id from v_data;
    commit;

    create index T1_ID on T1 (ID);
    create unique index T1_COL on T1 (COL);
    create index T2_ID on T2 (ID);
    create index T3_ID on T3 (ID);

    set planonly;
    select *
    from T1
    join T2 on T2.ID = T1.ID
    join T3 on T3.ID = coalesce(T1.ID, T2.ID)
    where T1.COL = 1;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN JOIN (T1 INDEX (T1_COL), T2 INDEX (T2_ID), T3 INDEX (T3_ID))
"""

expected_stdout_6x = """
    PLAN JOIN ("PUBLIC"."T1" INDEX ("PUBLIC"."T1_COL"), "PUBLIC"."T2" INDEX ("PUBLIC"."T2_ID"), "PUBLIC"."T3" INDEX ("PUBLIC"."T3_ID"))
"""

@pytest.mark.version('>=3.0.9')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
