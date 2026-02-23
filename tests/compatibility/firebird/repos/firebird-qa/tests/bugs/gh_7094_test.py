#coding:utf-8

"""
ID:          issue-7094
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7094
TITLE:       Incorrect indexed lookup of strings when the last keys characters are part of collated contractions and there is multi-segment insensitive descending index
DESCRIPTION:
NOTES:
    [15.08.2023] pzotov
    Confirmed problem on 5.0.0.425.
    Checked on 5.0.0.1163, 4.0.4.2978.
    Test fails on 3.0.12 with 'invalid collation attribute', thus min_version was set to 4.0.2.

    [04.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.876; 5.0.3.1668.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create collation unicode_ci_cz for utf8 from unicode case insensitive 'LOCALE=cs_CZ';

    create table t1 (
        c1 varchar(10) character set utf8 collate unicode_ci_cz,
        c2 varchar(10) character set utf8 collate unicode_ci_cz
    );

    insert into t1 (c1) values ('a');
    insert into t1 (c1) values ('b');
    insert into t1 (c1) values ('c');
    insert into t1 (c1) values ('ch');
    insert into t1 (c1) values ('d');
    insert into t1 (c1) values ('e');
    insert into t1 (c1) values ('f');
    update t1 set c2 = c1;
    commit;

    create desc index t1_c1_c2_desc on t1 (c1, c2);

    set list on;
    set plan on;

    -- no data was displayed here before fix:
    select t1.* from t1 where c1 > 'c' order by c1, c2;

    select t1.* from t1 where c1 > 'c' plan (t1 natural) order by c1, c2;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.intl
@pytest.mark.version('>=4.0.2')
def test_1(act: Action):

    expected_stdout_5x = """
        PLAN SORT (T1 INDEX (T1_C1_C2_DESC))
        C1                              d
        C2                              d
        C1                              e
        C2                              e
        C1                              f
        C2                              f
        C1                              ch
        C2                              ch

        PLAN SORT (T1 NATURAL)
        C1                              d
        C2                              d
        C1                              e
        C2                              e
        C1                              f
        C2                              f
        C1                              ch
        C2                              ch
    """

    expected_stdout_6x = """
        PLAN SORT ("PUBLIC"."T1" INDEX ("PUBLIC"."T1_C1_C2_DESC"))
        C1                              d
        C2                              d
        C1                              e
        C2                              e
        C1                              f
        C2                              f
        C1                              ch
        C2                              ch
        PLAN SORT ("PUBLIC"."T1" NATURAL)
        C1                              d
        C2                              d
        C1                              e
        C2                              e
        C1                              f
        C2                              f
        C1                              ch
        C2                              ch
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
