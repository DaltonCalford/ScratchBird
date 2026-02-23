#coding:utf-8

"""
ID:          issue-7568
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7568
TITLE:       Equivalence of boolean condition in partial index
NOTES:
    [03.02.2024] pzotov
        Test is based on https://github.com/FirebirdSQL/firebird/pull/7987
        Confirmed problem on 6.0.0.244.
        Checked on 6.0.0.247.
    [04.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.863.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test (
       id bigint generated always as identity primary key
      ,int_fld1 bigint not null
      ,int_fld2 bigint not null
      ,bool_fld1 boolean default false not null
      ,bool_fld2 boolean default false not null
    );

    create index test_idx_offer_asc
      on test (int_fld1)
      where not bool_fld1;

    create descending index test_idx_offer_dec
      on test (int_fld2)
      where not bool_fld2;

    -- all the following sql queries must use appropriate index:

    set planonly;

    select * from test where not bool_fld1;

    select * from test where bool_fld1 = false;

    select * from test where false = bool_fld1;

    select * from test where bool_fld1 <> true;

    select * from test where true <> bool_fld1;

    select * from test where not bool_fld1 = true;

    select * from test where not true = bool_fld1;


    select * from test where not bool_fld2;

    select * from test where bool_fld2 = false;

    select * from test where false = bool_fld2;

    select * from test where bool_fld2 <> true;

    select * from test where true <> bool_fld2;

    select * from test where not bool_fld2 = true;

    select * from test where not true = bool_fld2;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout_5x = """
        PLAN (TEST INDEX (TEST_IDX_OFFER_ASC))
        PLAN (TEST INDEX (TEST_IDX_OFFER_ASC))
        PLAN (TEST INDEX (TEST_IDX_OFFER_ASC))
        PLAN (TEST INDEX (TEST_IDX_OFFER_ASC))
        PLAN (TEST INDEX (TEST_IDX_OFFER_ASC))
        PLAN (TEST INDEX (TEST_IDX_OFFER_ASC))
        PLAN (TEST INDEX (TEST_IDX_OFFER_ASC))
        PLAN (TEST INDEX (TEST_IDX_OFFER_DEC))
        PLAN (TEST INDEX (TEST_IDX_OFFER_DEC))
        PLAN (TEST INDEX (TEST_IDX_OFFER_DEC))
        PLAN (TEST INDEX (TEST_IDX_OFFER_DEC))
        PLAN (TEST INDEX (TEST_IDX_OFFER_DEC))
        PLAN (TEST INDEX (TEST_IDX_OFFER_DEC))
        PLAN (TEST INDEX (TEST_IDX_OFFER_DEC))
    """
    expected_stdout_6x = """
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_IDX_OFFER_ASC"))
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_IDX_OFFER_ASC"))
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_IDX_OFFER_ASC"))
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_IDX_OFFER_ASC"))
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_IDX_OFFER_ASC"))
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_IDX_OFFER_ASC"))
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_IDX_OFFER_ASC"))
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_IDX_OFFER_DEC"))
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_IDX_OFFER_DEC"))
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_IDX_OFFER_DEC"))
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_IDX_OFFER_DEC"))
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_IDX_OFFER_DEC"))
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_IDX_OFFER_DEC"))
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_IDX_OFFER_DEC"))
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
