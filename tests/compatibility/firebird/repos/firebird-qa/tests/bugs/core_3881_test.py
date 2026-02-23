#coding:utf-8

"""
ID:          issue-4218
ISSUE:       4218
TITLE:       Extend the error reported for index/constraint violations to include the problematic key value
DESCRIPTION:
JIRA:        CORE-3881
FBTEST:      bugs.core_3881
NOTES:
    [27.06.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table tmain(
        id int constraint tmain_pk primary key using index tmain_pk
        ,mka int
        ,mkb int
        ,constraint tmain_mka_mkb_constraint_unq unique(mka, mkb) using index tmain_mka_mkb_constraint_unq
        ,constraint tmain_fka foreign key (mka) references tmain(id) on update set null using index tmain_fka
        ,constraint tmain_fkb foreign key (mkb) references tmain(id) on update set null using index tmain_fkb
    );
    create unique descending index tmain_difference_unq_idx on tmain computed by( (coalesce(mka,0) - coalesce(mkb,0)) );
    commit;

    insert into tmain(id, mka, mkb) values( 100, 100,  null);
    insert into tmain(id, mka, mkb) values( 200, null, 200);
    insert into tmain(id, mka, mkb) values( 300, 200, null);
    commit;


    -- This statement should violate primary key constraint:
    insert into tmain(id, mka, mkb) select 600-id, 600-id, mkb from tmain where id<>200;

    -- This will violate unique constraint `tmain_mka_mkb_constraint_unq` defined on two nullable fields:(mka, mkb):
    insert into tmain(id, mka, mkb) values(500, null,200);

    -- This will violate unique computed-by index `tmain_difference_unq_idx` because NULLS are treated in its expression as ZEROES:
    update tmain set mka=null, mkb=null;

    -- This will violate unique constraint `tmain_mka_mkb_constraint_unq` defined on two nullable fields:(mka, mkb):
    update tmain set mka=coalesce(mka, 200), mkb=coalesce(mkb, 200);

    -- This will force CASCADE trigger to make updating values in fields `mka` and `mkb` to NULLS.
    -- But it means that these NULLS will be treated as ZEROEs in expression of computed-by index `tmain_difference_unq_idx`
    -- and thus again will violate its uniquness:
    update tmain set id=0 where id=200;  -- uniq idx tmain_difference_unq_idx, At trigger 'CHECK_nn'
"""

act = isql_act('db', test_script, substitutions=[('-At trigger.*', '')])


@pytest.mark.version('>=3')
def test_1(act: Action):
    
    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'

    expected_stderr = f"""
        Statement failed, SQLSTATE = 23000
        violation of PRIMARY or UNIQUE KEY constraint "TMAIN_PK" on table {SQL_SCHEMA_PREFIX}"TMAIN"
        -Problematic key value is ("ID" = 300)

        Statement failed, SQLSTATE = 23000
        violation of PRIMARY or UNIQUE KEY constraint "TMAIN_MKA_MKB_CONSTRAINT_UNQ" on table {SQL_SCHEMA_PREFIX}"TMAIN"
        -Problematic key value is ("MKA" = NULL, "MKB" = 200)

        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index {SQL_SCHEMA_PREFIX}"TMAIN_DIFFERENCE_UNQ_IDX"
        -Problematic key value is (<expression> = 0)

        Statement failed, SQLSTATE = 23000
        violation of PRIMARY or UNIQUE KEY constraint "TMAIN_MKA_MKB_CONSTRAINT_UNQ" on table {SQL_SCHEMA_PREFIX}"TMAIN"
        -Problematic key value is ("MKA" = 200, "MKB" = 200)

        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index {SQL_SCHEMA_PREFIX}"TMAIN_DIFFERENCE_UNQ_IDX"
        -Problematic key value is (<expression> = 0)
        -At trigger 'CHECK_2'
    """

    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

