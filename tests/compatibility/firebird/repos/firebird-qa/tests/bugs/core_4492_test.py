#coding:utf-8

"""
ID:          issue-1566
ISSUE:       1566
TITLE:       OR/IN predicates for RDB$DBKEY lead to NATURAL plan
DESCRIPTION:

JIRA:        CORE-4492
FBTEST:      bugs.core_4492
NOTES:
    [25.11.2017]
    Following query will not compile:
    select 1 from rdb$relations a join rdb$relations b using ( rdb$db_key );
    Statement failed, SQLSTATE = 42000 / -Token unknown /  -rdb$db_key ==> Why ?
    Sent letter to dimitr, 25.11.2017 22:42. Waiting for reply.

    [27.12.2017]
    Seems that this note will remain unresolved for undef. time.

    [07.04.2022] pzotov
    FB 5.0.0.455 and later: data sources with equal cardinality now present in the HASH plan in order they are specified in the query.
    Reversed order was used before this build. Because of this, two cases of expected stdout must be taken in account, see variables
    'fb3x_checked_stdout' and 'fb5x_checked_stdout'.

    [29.06.2025] pzotov
    Added branch in defintion of expected output for FB-6x.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.

"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;

    recreate view v_unioned as
    select rdb$relation_id as rel_id, rdb$db_key as db_key from rdb$relations
    union all
    select rdb$relation_id,           rdb$db_key           from rdb$relations
    ;

    set planonly;
    --set echo on;

    -- 27.12.2017: this works fine (fixed by dimitr, see letter 01.12.2017 09:57):
    select 0 from rdb$relations rr0 where rr0.rdb$db_key is not distinct from ?;

    select 1 from rdb$relations rr1 where rr1.rdb$db_key in (?, ?);
    select 2 from v_unioned vu where vu.db_key in (?, ?);

    select 3 from rdb$relations rr_a join rdb$relations rr_b on rr_a.rdb$db_key = rr_b.rdb$db_key;

    -- NOTE. Following query shows difference in FB 5.x vs previous releases:
    -- 3.x, 4.x: PLAN HASH (VU_B ..., VU_B ..., VU_A ..., VU_A ...)
    -- 5.x:      PLAN HASH (VU_A ..., VU_A ..., VU_B ..., VU_B ...) // since 5.0.0.455 03-apr-2022
    -- (data sources with equal cardinality now present in the HASH plan in order they are specified in the query; before *reverse* order was used)
    -- 
    select 4 from v_unioned vu_a join v_unioned vu_b on vu_a.db_key = vu_b.db_key;
"""

act = isql_act('db', test_script)

fb3x_checked_stdout = """
    PLAN (RR0 INDEX ())
    PLAN (RR1 INDEX ())
    PLAN (VU RDB$RELATIONS INDEX (), VU RDB$RELATIONS INDEX ())
    PLAN JOIN (RR_A NATURAL, RR_B INDEX ())
    PLAN HASH (VU_B RDB$RELATIONS NATURAL, VU_B RDB$RELATIONS NATURAL, VU_A RDB$RELATIONS NATURAL, VU_A RDB$RELATIONS NATURAL)
"""


fb5x_checked_stdout = """
    PLAN (RR0 INDEX ())
    PLAN (RR1 INDEX ())
    PLAN (VU RDB$RELATIONS INDEX (), VU RDB$RELATIONS INDEX ())
    PLAN JOIN (RR_A NATURAL, RR_B INDEX ())
    PLAN HASH (VU_A RDB$RELATIONS NATURAL, VU_A RDB$RELATIONS NATURAL, VU_B RDB$RELATIONS NATURAL, VU_B RDB$RELATIONS NATURAL)
"""

fb6x_checked_stdout = """
    PLAN ("RR0" INDEX ())
    PLAN ("RR1" INDEX ())
    PLAN ("VU" "SYSTEM"."RDB$RELATIONS" INDEX (), "VU" "SYSTEM"."RDB$RELATIONS" INDEX ())
    PLAN JOIN ("RR_A" NATURAL, "RR_B" INDEX ())
    PLAN HASH ("VU_A" "SYSTEM"."RDB$RELATIONS" NATURAL, "VU_A" "SYSTEM"."RDB$RELATIONS" NATURAL, "VU_B" "SYSTEM"."RDB$RELATIONS" NATURAL, "VU_B" "SYSTEM"."RDB$RELATIONS" NATURAL)
"""

@pytest.mark.version('>=3.0.3')
def test_1(act: Action):
    act.expected_stdout = fb3x_checked_stdout if act.is_version('<5') else fb5x_checked_stdout if act.is_version('<6') else fb6x_checked_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

