#coding:utf-8

"""
ID:          issue-2628
ISSUE:       2628
TITLE:       Extremely slow executing a cross join of 3 tables in Firebird 2.X
DESCRIPTION:
JIRA:        CORE-2200
FBTEST:      bugs.core_2200
NOTES:
    [26.06.2025] pzotov
    Re-implemented to be more relevant with:
    https://github.com/FirebirdSQL/firebird/issues/2628#issuecomment-826197753
    (Improve the cross join algorithm to stop as soon as any of the involved streams is detected as empty).
    No matter how many tables are involved in the cross join, its plan must always start from EMPTY table.
    We create here six tables and fill five of them. Table 't6' remains empty.
    We check that in all queries where this table present in different position execution plans starts
    with name of this table.

    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Confirmed: wrong plan on 2.0.0.12724 and 2.1.0.17798: 'T2' was choosen as drive source,
    -- absence of rows in T3 was ignored, used: PLAN JOIN (T2 NATURAL, T1 NATURAL, T3 NATURAL)

    set list on;
    recreate table t1(id int);
    recreate table t2(id int);
    recreate table t3(id int);
    recreate table t4(id int);
    recreate table t5(id int);
    recreate table t6(id int);
    set term ^;
    create procedure sp_gen_rows( a_cnt int ) returns ( i int ) as
    begin
        i = 0;
        while (i < a_cnt ) do
        begin
            suspend;
            i = i + 1;
        end
    end
    ^
    set term ;^
    commit;

    insert into t1 select p.i from sp_gen_rows( 1000 + rand() * 9000 ) as p;
    insert into t2 select p.i from sp_gen_rows( 1000 + rand() * 9000 ) as p;
    insert into t3 select p.i from sp_gen_rows( 1000 + rand() * 9000 ) as p;
    insert into t4 select p.i from sp_gen_rows( 1000 + rand() * 9000 ) as p;
    insert into t5 select p.i from sp_gen_rows( 1000 + rand() * 9000 ) as p;
    -- ::: NB ::: we do NOT add any row to the table `t6`, it remains empty
    commit;

    select
         sign( (select count(*) from t1) - 1000 ) t1_big_enough
        ,sign( (select count(*) from t2) - 1000 ) t2_big_enough
        ,sign( (select count(*) from t3) - 1000 ) t3_big_enough
        ,sign( (select count(*) from t4) - 1000 ) t4_big_enough
        ,sign( (select count(*) from t5) - 1000 ) t5_big_enough
        ,(select count(*) from t6) t6_rows_count
    from rdb$database;

    set planonly;
    select count(*) from t1, t2, t3, t4, t5, t6;
    select count(*) from t1, t2, t3, t4, t6, t5;
    select count(*) from t1, t2, t3, t6, t5, t4;
    select count(*) from t1, t2, t6, t4, t5, t3;
    select count(*) from t1, t6, t3, t4, t5, t2;
    select count(*) from t6, t2, t3, t4, t5, t1;
"""


substitutions = [
    ('[ \t]+', ' '),
    (r'PLAN JOIN \(T6 NATURAL.*', 'PLAN JOIN (T6 NATURAL'),
    (r'PLAN JOIN \("PUBLIC"."T6" NATURAL.*', 'PLAN JOIN ("PUBLIC"."T6" NATURAL')
]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    T1_BIG_ENOUGH 1
    T2_BIG_ENOUGH 1
    T3_BIG_ENOUGH 1
    T4_BIG_ENOUGH 1
    T5_BIG_ENOUGH 1
    T6_ROWS_COUNT 0
    PLAN JOIN (T6 NATURAL, T2 NATURAL, T4 NATURAL, T5 NATURAL, T1 NATURAL, T3 NATURAL)
    PLAN JOIN (T6 NATURAL, T2 NATURAL, T4 NATURAL, T5 NATURAL, T1 NATURAL, T3 NATURAL)
    PLAN JOIN (T6 NATURAL, T2 NATURAL, T4 NATURAL, T5 NATURAL, T1 NATURAL, T3 NATURAL)
    PLAN JOIN (T6 NATURAL, T2 NATURAL, T4 NATURAL, T5 NATURAL, T1 NATURAL, T3 NATURAL)
    PLAN JOIN (T6 NATURAL, T2 NATURAL, T4 NATURAL, T5 NATURAL, T1 NATURAL, T3 NATURAL)
    PLAN JOIN (T6 NATURAL, T2 NATURAL, T4 NATURAL, T5 NATURAL, T3 NATURAL, T1 NATURAL)
"""

expected_stdout_6x = """
    T1_BIG_ENOUGH 1
    T2_BIG_ENOUGH 1
    T3_BIG_ENOUGH 1
    T4_BIG_ENOUGH 1
    T5_BIG_ENOUGH 1
    T6_ROWS_COUNT 0
    PLAN JOIN ("PUBLIC"."T6" NATURAL, "PUBLIC"."T5" NATURAL, "PUBLIC"."T4" NATURAL, "PUBLIC"."T1" NATURAL, "PUBLIC"."T3" NATURAL, "PUBLIC"."T2" NATURAL)
    PLAN JOIN ("PUBLIC"."T6" NATURAL, "PUBLIC"."T5" NATURAL, "PUBLIC"."T4" NATURAL, "PUBLIC"."T1" NATURAL, "PUBLIC"."T3" NATURAL, "PUBLIC"."T2" NATURAL)
    PLAN JOIN ("PUBLIC"."T6" NATURAL, "PUBLIC"."T5" NATURAL, "PUBLIC"."T4" NATURAL, "PUBLIC"."T1" NATURAL, "PUBLIC"."T3" NATURAL, "PUBLIC"."T2" NATURAL)
    PLAN JOIN ("PUBLIC"."T6" NATURAL, "PUBLIC"."T5" NATURAL, "PUBLIC"."T4" NATURAL, "PUBLIC"."T1" NATURAL, "PUBLIC"."T3" NATURAL, "PUBLIC"."T2" NATURAL)
    PLAN JOIN ("PUBLIC"."T6" NATURAL, "PUBLIC"."T5" NATURAL, "PUBLIC"."T4" NATURAL, "PUBLIC"."T1" NATURAL, "PUBLIC"."T3" NATURAL, "PUBLIC"."T2" NATURAL)
    PLAN JOIN ("PUBLIC"."T6" NATURAL, "PUBLIC"."T5" NATURAL, "PUBLIC"."T4" NATURAL, "PUBLIC"."T3" NATURAL, "PUBLIC"."T1" NATURAL, "PUBLIC"."T2" NATURAL)
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
