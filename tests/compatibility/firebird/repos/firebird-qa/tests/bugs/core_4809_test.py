#coding:utf-8

"""
ID:          issue-5107
ISSUE:       5107
TITLE:       HASH/MERGE JOIN is not used for more than two streams if they are joined via USING/NATURAL clauses and join is based on DBKEY concatenations
DESCRIPTION:
JIRA:        CORE-4809
FBTEST:      bugs.core_4809
NOTES:
    [07.04.2022] pzotov
    FB 5.0.0.455 and later: data sources with equal cardinality now present in the HASH plan in order they are specified in the query.
    Reversed order was used before this build. Because of this, two cases of expected stdout must be taken in account, see variables
    'fb3x_checked_stdout' and 'fb5x_checked_stdout'.
    [31.07.2023] pzotov
    Adjusted expected execution plans for FB 5.x after commit: 8ef5b9838129ff1ae90a3672ddc6f0e924b42166
    ("Fixed cardinality for hash joins"). Explained by dimitr, letter 31-JUL-2023 11:00:
    result of HASH(A, B) is considered now as having greater cardinality than HASH(C).
    This causes optimizer to put HASH(A,B) as first source.
    Checked on 5.0.0.1149.

    [30.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table tk(x int primary key using index tk_x);
    commit;
    insert into tk(x)
    with recursive r as (select 0 i from rdb$database union all select r.i+1 from r where r.i<99)
    select r1.i*100+r0.i from r r1, r r0;
    commit;
    set statistics index tk_x;
    commit;
    
    recreate table tf(id_key int primary key using index tf_id_key);
    commit;
    insert into tf(id_key)
    with recursive r as (select 0 i from rdb$database union all select r.i+1 from r where r.i<99)
    select r1.i*100+r0.i from r r1, r r0;
    commit;
    set statistics index tf_id_key;
    commit;
    
    set planonly;
    
    -- 1. Join using RDB$DB_KEY based expressions:
    
    ----------- test `traditional` join form -----------------
    
    select count(*)
    from (select rdb$db_key||'' a from tk) datasource_a
    join (select rdb$db_key||'' a from tk) datasource_b on datasource_a.a = datasource_b.a;
    
    
    select count(*)
    from (select rdb$db_key||'' a from tk) datasource_a
    join (select rdb$db_key||'' a from tk) datasource_b on datasource_a.a = datasource_b.a
    join (select rdb$db_key||'' a from tk) datasource_c on datasource_b.a = datasource_c.a;
    
    
    select count(*)
    from (select rdb$db_key||'' a from tk) datasource_a
    join (select rdb$db_key||'' a from tk) datasource_b on datasource_a.a = datasource_b.a
    join (select rdb$db_key||'' a from tk) datasource_c on datasource_b.a = datasource_c.a
    join (select rdb$db_key||'' a from tk) datasource_d on datasource_c.a = datasource_d.a;
    
    ----------- test join on named columns form -----------------
    
    select count(*)
    from (select rdb$db_key||'' a from tk) datasource_a
    join (select rdb$db_key||'' a from tk) datasource_b using(a);
    
    select count(*)
    from (select rdb$db_key||'' a from tk) datasource_a
    join (select rdb$db_key||'' a from tk) datasource_b using(a)
    join (select rdb$db_key||'' a from tk) datasource_c using(a);
    
    select count(*)
    from (select rdb$db_key||'' a from tk) datasource_a
    join (select rdb$db_key||'' a from tk) datasource_b using(a)
    join (select rdb$db_key||'' a from tk) datasource_c using(a)
    join (select rdb$db_key||'' a from tk) datasource_d using(a);
    
    ----------- test natural join form -----------------
    
    select count(*)
    from (select rdb$db_key||'' a from tk) datasource_a
    natural join (select rdb$db_key||'' a from tk) datasource_b;
    
    select count(*)
    from (select rdb$db_key||'' a from tk) datasource_a
    natural join (select rdb$db_key||'' a from tk) datasource_b
    natural join (select rdb$db_key||'' a from tk) datasource_c;
    
    select count(*)
    from (select rdb$db_key||'' a from tk) datasource_a
    natural join (select rdb$db_key||'' a from tk) datasource_b
    natural join (select rdb$db_key||'' a from tk) datasource_c
    natural join (select rdb$db_key||'' a from tk) datasource_d;
    
    -------------------------------------------------
    
    -- 2. Join using COMMON FIELD based expressions:
    -- (all following statements produced 'PLAN HASH' before fix also; 
    --  here we verify them only for sure that evething remains OK).
    
    ----------- test `traditional` join form -----------------
    
    select count(*)
    from (select id_key||'' a from tf) datasource_a
    join (select id_key||'' a from tf) datasource_b on datasource_a.a = datasource_b.a;
    
    
    select count(*)
    from (select id_key||'' a from tf) datasource_a
    join (select id_key||'' a from tf) datasource_b on datasource_a.a = datasource_b.a
    join (select id_key||'' a from tf) datasource_c on datasource_b.a = datasource_c.a;
    
    
    select count(*)
    from (select id_key||'' a from tf) datasource_a
    join (select id_key||'' a from tf) datasource_b on datasource_a.a = datasource_b.a
    join (select id_key||'' a from tf) datasource_c on datasource_b.a = datasource_c.a
    join (select id_key||'' a from tf) datasource_d on datasource_c.a = datasource_d.a;
    
    ----------- test join on named columns form -----------------
    
    select count(*)
    from (select id_key||'' a from tf) datasource_a
    join (select id_key||'' a from tf) datasource_b using(a);
    
    select count(*)
    from (select id_key||'' a from tf) datasource_a
    join (select id_key||'' a from tf) datasource_b using(a)
    join (select id_key||'' a from tf) datasource_c using(a);
    
    select count(*)
    from (select id_key||'' a from tf) datasource_a
    join (select id_key||'' a from tf) datasource_b using(a)
    join (select id_key||'' a from tf) datasource_c using(a)
    join (select id_key||'' a from tf) datasource_d using(a);
    
    ----------- test natural join form -----------------
    
    select count(*)
    from (select id_key||'' a from tf) datasource_a
    natural join (select id_key||'' a from tf) datasource_b;
    
    select count(*)
    from (select id_key||'' a from tf) datasource_a
    natural join (select id_key||'' a from tf) datasource_b
    natural join (select id_key||'' a from tf) datasource_c;
    
    select count(*)
    from (select id_key||'' a from tf) datasource_a
    natural join (select id_key||'' a from tf) datasource_b
    natural join (select id_key||'' a from tf) datasource_c
    natural join (select id_key||'' a from tf) datasource_d;
"""

act = isql_act('db', test_script)

fb3x_checked_stdout = """
    PLAN HASH (DATASOURCE_B TK NATURAL, DATASOURCE_A TK NATURAL)
    PLAN HASH (HASH (DATASOURCE_C TK NATURAL, DATASOURCE_B TK NATURAL), DATASOURCE_A TK NATURAL)
    PLAN HASH (HASH (HASH (DATASOURCE_D TK NATURAL, DATASOURCE_C TK NATURAL), DATASOURCE_B TK NATURAL), DATASOURCE_A TK NATURAL)
    PLAN HASH (DATASOURCE_B TK NATURAL, DATASOURCE_A TK NATURAL)
    PLAN HASH (DATASOURCE_C TK NATURAL, HASH (DATASOURCE_B TK NATURAL, DATASOURCE_A TK NATURAL))
    PLAN HASH (DATASOURCE_D TK NATURAL, HASH (DATASOURCE_C TK NATURAL, HASH (DATASOURCE_B TK NATURAL, DATASOURCE_A TK NATURAL)))
    PLAN HASH (DATASOURCE_B TK NATURAL, DATASOURCE_A TK NATURAL)
    PLAN HASH (DATASOURCE_C TK NATURAL, HASH (DATASOURCE_B TK NATURAL, DATASOURCE_A TK NATURAL))
    PLAN HASH (DATASOURCE_D TK NATURAL, HASH (DATASOURCE_C TK NATURAL, HASH (DATASOURCE_B TK NATURAL, DATASOURCE_A TK NATURAL)))
    PLAN HASH (DATASOURCE_B TF NATURAL, DATASOURCE_A TF NATURAL)
    PLAN HASH (HASH (DATASOURCE_C TF NATURAL, DATASOURCE_B TF NATURAL), DATASOURCE_A TF NATURAL)
    PLAN HASH (HASH (HASH (DATASOURCE_D TF NATURAL, DATASOURCE_C TF NATURAL), DATASOURCE_B TF NATURAL), DATASOURCE_A TF NATURAL)
    PLAN HASH (DATASOURCE_B TF NATURAL, DATASOURCE_A TF NATURAL)
    PLAN HASH (DATASOURCE_C TF NATURAL, HASH (DATASOURCE_B TF NATURAL, DATASOURCE_A TF NATURAL))
    PLAN HASH (DATASOURCE_D TF NATURAL, HASH (DATASOURCE_C TF NATURAL, HASH (DATASOURCE_B TF NATURAL, DATASOURCE_A TF NATURAL)))
    PLAN HASH (DATASOURCE_B TF NATURAL, DATASOURCE_A TF NATURAL)
    PLAN HASH (DATASOURCE_C TF NATURAL, HASH (DATASOURCE_B TF NATURAL, DATASOURCE_A TF NATURAL))
    PLAN HASH (DATASOURCE_D TF NATURAL, HASH (DATASOURCE_C TF NATURAL, HASH (DATASOURCE_B TF NATURAL, DATASOURCE_A TF NATURAL)))
"""


fb4x_checked_stdout = """
    PLAN HASH (DATASOURCE_B TK NATURAL, DATASOURCE_A TK NATURAL)
    PLAN HASH (HASH (DATASOURCE_C TK NATURAL, DATASOURCE_B TK NATURAL), DATASOURCE_A TK NATURAL)
    PLAN HASH (HASH (HASH (DATASOURCE_D TK NATURAL, DATASOURCE_C TK NATURAL), DATASOURCE_B TK NATURAL), DATASOURCE_A TK NATURAL)
    PLAN HASH (DATASOURCE_B TK NATURAL, DATASOURCE_A TK NATURAL)
    PLAN HASH (DATASOURCE_C TK NATURAL, HASH (DATASOURCE_B TK NATURAL, DATASOURCE_A TK NATURAL))
    PLAN HASH (DATASOURCE_D TK NATURAL, HASH (DATASOURCE_C TK NATURAL, HASH (DATASOURCE_B TK NATURAL, DATASOURCE_A TK NATURAL)))
    PLAN HASH (DATASOURCE_B TK NATURAL, DATASOURCE_A TK NATURAL)
    PLAN HASH (DATASOURCE_C TK NATURAL, HASH (DATASOURCE_B TK NATURAL, DATASOURCE_A TK NATURAL))
    PLAN HASH (DATASOURCE_D TK NATURAL, HASH (DATASOURCE_C TK NATURAL, HASH (DATASOURCE_B TK NATURAL, DATASOURCE_A TK NATURAL)))
    PLAN HASH (DATASOURCE_B TF NATURAL, DATASOURCE_A TF NATURAL)
    PLAN HASH (HASH (DATASOURCE_C TF NATURAL, DATASOURCE_B TF NATURAL), DATASOURCE_A TF NATURAL)
    PLAN HASH (HASH (HASH (DATASOURCE_D TF NATURAL, DATASOURCE_C TF NATURAL), DATASOURCE_B TF NATURAL), DATASOURCE_A TF NATURAL)
    PLAN HASH (DATASOURCE_B TF NATURAL, DATASOURCE_A TF NATURAL)
    PLAN HASH (DATASOURCE_C TF NATURAL, HASH (DATASOURCE_B TF NATURAL, DATASOURCE_A TF NATURAL))
    PLAN HASH (DATASOURCE_D TF NATURAL, HASH (DATASOURCE_C TF NATURAL, HASH (DATASOURCE_B TF NATURAL, DATASOURCE_A TF NATURAL)))
    PLAN HASH (DATASOURCE_B TF NATURAL, DATASOURCE_A TF NATURAL)
    PLAN HASH (DATASOURCE_C TF NATURAL, HASH (DATASOURCE_B TF NATURAL, DATASOURCE_A TF NATURAL))
    PLAN HASH (DATASOURCE_D TF NATURAL, HASH (DATASOURCE_C TF NATURAL, HASH (DATASOURCE_B TF NATURAL, DATASOURCE_A TF NATURAL)))
"""

fb5x_checked_stdout = """
    PLAN HASH (DATASOURCE_A TK NATURAL, DATASOURCE_B TK NATURAL)
    PLAN HASH (DATASOURCE_A TK NATURAL, DATASOURCE_B TK NATURAL, DATASOURCE_C TK NATURAL)
    PLAN HASH (DATASOURCE_A TK NATURAL, DATASOURCE_B TK NATURAL, DATASOURCE_C TK NATURAL, DATASOURCE_D TK NATURAL)
    PLAN HASH (DATASOURCE_A TK NATURAL, DATASOURCE_B TK NATURAL)
    PLAN HASH (HASH (DATASOURCE_A TK NATURAL, DATASOURCE_B TK NATURAL), DATASOURCE_C TK NATURAL)
    PLAN HASH (HASH (HASH (DATASOURCE_A TK NATURAL, DATASOURCE_B TK NATURAL), DATASOURCE_C TK NATURAL), DATASOURCE_D TK NATURAL)
    PLAN HASH (DATASOURCE_A TK NATURAL, DATASOURCE_B TK NATURAL)
    PLAN HASH (HASH (DATASOURCE_A TK NATURAL, DATASOURCE_B TK NATURAL), DATASOURCE_C TK NATURAL)
    PLAN HASH (HASH (HASH (DATASOURCE_A TK NATURAL, DATASOURCE_B TK NATURAL), DATASOURCE_C TK NATURAL), DATASOURCE_D TK NATURAL)
    PLAN HASH (DATASOURCE_A TF NATURAL, DATASOURCE_B TF NATURAL)
    PLAN HASH (DATASOURCE_A TF NATURAL, DATASOURCE_B TF NATURAL, DATASOURCE_C TF NATURAL)
    PLAN HASH (DATASOURCE_A TF NATURAL, DATASOURCE_B TF NATURAL, DATASOURCE_C TF NATURAL, DATASOURCE_D TF NATURAL)
    PLAN HASH (DATASOURCE_A TF NATURAL, DATASOURCE_B TF NATURAL)
    PLAN HASH (HASH (DATASOURCE_A TF NATURAL, DATASOURCE_B TF NATURAL), DATASOURCE_C TF NATURAL)
    PLAN HASH (HASH (HASH (DATASOURCE_A TF NATURAL, DATASOURCE_B TF NATURAL), DATASOURCE_C TF NATURAL), DATASOURCE_D TF NATURAL)
    PLAN HASH (DATASOURCE_A TF NATURAL, DATASOURCE_B TF NATURAL)
    PLAN HASH (HASH (DATASOURCE_A TF NATURAL, DATASOURCE_B TF NATURAL), DATASOURCE_C TF NATURAL)
    PLAN HASH (HASH (HASH (DATASOURCE_A TF NATURAL, DATASOURCE_B TF NATURAL), DATASOURCE_C TF NATURAL), DATASOURCE_D TF NATURAL)
"""

fb6x_checked_stdout = """
    PLAN HASH ("DATASOURCE_A" "PUBLIC"."TK" NATURAL, "DATASOURCE_B" "PUBLIC"."TK" NATURAL)
    PLAN HASH ("DATASOURCE_A" "PUBLIC"."TK" NATURAL, "DATASOURCE_B" "PUBLIC"."TK" NATURAL, "DATASOURCE_C" "PUBLIC"."TK" NATURAL)
    PLAN HASH ("DATASOURCE_A" "PUBLIC"."TK" NATURAL, "DATASOURCE_B" "PUBLIC"."TK" NATURAL, "DATASOURCE_C" "PUBLIC"."TK" NATURAL, "DATASOURCE_D" "PUBLIC"."TK" NATURAL)
    PLAN HASH ("DATASOURCE_A" "PUBLIC"."TK" NATURAL, "DATASOURCE_B" "PUBLIC"."TK" NATURAL)
    PLAN HASH (HASH ("DATASOURCE_A" "PUBLIC"."TK" NATURAL, "DATASOURCE_B" "PUBLIC"."TK" NATURAL), "DATASOURCE_C" "PUBLIC"."TK" NATURAL)
    PLAN HASH (HASH (HASH ("DATASOURCE_A" "PUBLIC"."TK" NATURAL, "DATASOURCE_B" "PUBLIC"."TK" NATURAL), "DATASOURCE_C" "PUBLIC"."TK" NATURAL), "DATASOURCE_D" "PUBLIC"."TK" NATURAL)
    PLAN HASH ("DATASOURCE_A" "PUBLIC"."TK" NATURAL, "DATASOURCE_B" "PUBLIC"."TK" NATURAL)
    PLAN HASH (HASH ("DATASOURCE_A" "PUBLIC"."TK" NATURAL, "DATASOURCE_B" "PUBLIC"."TK" NATURAL), "DATASOURCE_C" "PUBLIC"."TK" NATURAL)
    PLAN HASH (HASH (HASH ("DATASOURCE_A" "PUBLIC"."TK" NATURAL, "DATASOURCE_B" "PUBLIC"."TK" NATURAL), "DATASOURCE_C" "PUBLIC"."TK" NATURAL), "DATASOURCE_D" "PUBLIC"."TK" NATURAL)
    PLAN HASH ("DATASOURCE_A" "PUBLIC"."TF" NATURAL, "DATASOURCE_B" "PUBLIC"."TF" NATURAL)
    PLAN HASH ("DATASOURCE_A" "PUBLIC"."TF" NATURAL, "DATASOURCE_B" "PUBLIC"."TF" NATURAL, "DATASOURCE_C" "PUBLIC"."TF" NATURAL)
    PLAN HASH ("DATASOURCE_A" "PUBLIC"."TF" NATURAL, "DATASOURCE_B" "PUBLIC"."TF" NATURAL, "DATASOURCE_C" "PUBLIC"."TF" NATURAL, "DATASOURCE_D" "PUBLIC"."TF" NATURAL)
    PLAN HASH ("DATASOURCE_A" "PUBLIC"."TF" NATURAL, "DATASOURCE_B" "PUBLIC"."TF" NATURAL)
    PLAN HASH (HASH ("DATASOURCE_A" "PUBLIC"."TF" NATURAL, "DATASOURCE_B" "PUBLIC"."TF" NATURAL), "DATASOURCE_C" "PUBLIC"."TF" NATURAL)
    PLAN HASH (HASH (HASH ("DATASOURCE_A" "PUBLIC"."TF" NATURAL, "DATASOURCE_B" "PUBLIC"."TF" NATURAL), "DATASOURCE_C" "PUBLIC"."TF" NATURAL), "DATASOURCE_D" "PUBLIC"."TF" NATURAL)
    PLAN HASH ("DATASOURCE_A" "PUBLIC"."TF" NATURAL, "DATASOURCE_B" "PUBLIC"."TF" NATURAL)
    PLAN HASH (HASH ("DATASOURCE_A" "PUBLIC"."TF" NATURAL, "DATASOURCE_B" "PUBLIC"."TF" NATURAL), "DATASOURCE_C" "PUBLIC"."TF" NATURAL)
    PLAN HASH (HASH (HASH ("DATASOURCE_A" "PUBLIC"."TF" NATURAL, "DATASOURCE_B" "PUBLIC"."TF" NATURAL), "DATASOURCE_C" "PUBLIC"."TF" NATURAL), "DATASOURCE_D" "PUBLIC"."TF" NATURAL)
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = fb3x_checked_stdout if act.is_version('<4') else fb4x_checked_stdout if act.is_version('<5') else fb5x_checked_stdout if act.is_version('<6') else fb6x_checked_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

