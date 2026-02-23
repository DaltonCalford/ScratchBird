#coding:utf-8

"""
ID:          issue-6594
ISSUE:       6594
TITLE:       INT128 has problems with some PSQL objects
DESCRIPTION:
JIRA:        CORE-6353
FBTEST:      bugs.core_6353
NOTES:
    [03.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    create exception ex_zero_div_not_allowed 'Can not delete @1 by zero';
    commit;

    create sequence g1 start with 9223372036854775807;
    create sequence g2 start with -9223372036854775808;

    /*
    THIS WILL *NOT* WORK IN ANY FUTURE VERSION OF FB 4.X,
    SEE LETTER FROM ALEX, 13.07.2020 10:50 (SUBJECT: "i128"):
    create sequence g3 start with 170141183460469231731687303715884105727;
    create sequence g4 start with -170141183460469231731687303715884105728;
    */

    -------------------------------------------------
    recreate table test0( id int128 generated always as identity, id2 computed by (170141183460469231731687303715884105727 - id) );
    insert into test0 default values;
    select * from test0;
    commit;

    -- FAILS, CORE-6365: recreate table test0( id int128 generated always as identity (start with -9223372036854775808 increment by 9223372036854775807), id2 computed by (id+9223372036854775807) );

    recreate table test0( id int128 generated always as identity (start with -9223372036854775808 increment by 2147483647), id2 computed by (id+9223372036854775807) );
    insert into test0 default values;
    select * from test0;
    commit;

    -- FAILS, CORE-6365: recreate table test0( id int128 generated always as identity (start with 9223372036854775807 increment by -2147483648), id2 computed by (id-9223372036854775808) );

    recreate table test0( id int128 generated always as identity (start with 9223372036854775807 increment by -2147483647), id2 computed by (id-9223372036854775808) );
    insert into test0 default values;
    select * from test0;
    commit;

    -------------------------------------------------

    recreate table test0( i_min int128, i_max int128);
    create index test0_i_min_asc on test0(i_min);
    create descending index test0_i_min_dec on test0(i_min);
    create index test0_i_max_asc on test0(i_max);
    create descending index test0_i_max_dec on test0(i_max);

    recreate view v_test0 as select * from test0;
    commit;

    insert into test0(i_min, i_max) values(-170141183460469231731687303715884105728, 170141183460469231731687303715884105727);
    insert into v_test0(i_min, i_max) values(-170141183460469231731687303715884105728, 170141183460469231731687303715884105727);

    select * from v_test0;
    set plan on;
    select max(i_min) from v_test0;
    select min(i_min) from v_test0;
    select max(i_max) from v_test0;
    select min(i_max) from v_test0;
    set plan off;
    commit;

    -------------------------------------------------

    create domain dm_int128_great1 as int128 default 170141183460469231731687303715884105727;
    create domain dm_int128_least1 as int128 default -170141183460469231731687303715884105728;

    recreate table test1( i_min dm_int128_least1, i_max dm_int128_great1);
    alter table test1
        add constraint test1_chk
        check(
                i_min in(170141183460469231731687303715884105727, -170141183460469231731687303715884105728)
                and
                i_max in(170141183460469231731687303715884105727, -170141183460469231731687303715884105728)
             );

    recreate view v_test1 as select * from test1;
    commit;

    insert into test1 default values;
    insert into v_test1 default values;
    commit;

    select * from v_test1;
    commit;

    ---------------------------------------------------------

    create domain dm_int128_great2 as int128 default  170141183460469231731687303715884105727 check(value in(170141183460469231731687303715884105727, -170141183460469231731687303715884105728));
    create domain dm_int128_least2 as int128 default -170141183460469231731687303715884105728 check(value in(170141183460469231731687303715884105727, -170141183460469231731687303715884105728));
    commit;

    recreate table test2( i_min dm_int128_least2, i_max dm_int128_great2);
    recreate view v_test2 as select * from test2;
    commit;

    insert into test2 default values;

    insert into v_test2 default values;
    commit;

    select * from v_test2;
    commit;

    -- these two must fail because of check violation:
    insert into test2(i_min, i_max) values(-2,2);
    insert into v_test2(i_min, i_max) values(2,-2);

    ---------------------------------------------------------

    recreate view v_test3 as
    select -170141183460469231731687303715884105728 as v_min from rdb$database
    union all
    select 170141183460469231731687303715884105727 as v_min from rdb$database;
    commit;

    select * from v_test3;
    commit;

    -----------------------------------------------------------

    recreate table test4( i_min int128, i_max int128);
    commit;
    insert into test4(i_min, i_max) values(-170141183460469231731687303715884105728, 170141183460469231731687303715884105727);
    commit;

    set term ^;
    create or alter procedure sp_min(
        a_min type of column test4.i_min default -170141183460469231731687303715884105728
    ) returns(
        p_min type of column test4.i_min
    ) as
    begin
       select i_max from test4 where i_min >= :a_min rows 1 into p_min;
       suspend;
    end
    ^

    create or alter procedure sp_max(
        a_max type of column test4.i_max default 170141183460469231731687303715884105727
    ) returns(
        p_max type of column test4.i_max
    ) as
    begin
       select i_min from test4 where i_min <= :a_max rows 1 into p_max;
       suspend;
    end
    ^

    create or alter procedure sp_zero_div(a_delimiter int128 ) returns(p_min decfloat) as -- type of column test4.i_min) as
        declare v_min type of column test4.i_min;
    begin
       select min(i_min) from test4 into v_min;
       begin
           p_min = v_min / a_delimiter;
       when sqlstate '22012' do --  335544778
           exception ex_zero_div_not_allowed using( v_min );
       when any do
           exception;
       end

       suspend;
    end
    ^
    set term ;^
    commit;

    select * from sp_min;

    select * from sp_max;

    select * from sp_zero_div(0);

    select * from sp_zero_div( '-170141183460469231731687303715884105728' );

    select * from sp_zero_div( '170141183460469231731687303715884105727' );
    rollback;
"""

act = isql_act('db', test_script, substitutions=[('line: [\\d]+, col: [\\d]+', ''),
                                                 ('[ \t]+', ' ')])

expected_stdout_5x = """
    ID 1
    ID2 170141183460469231731687303715884105726
    ID -9223372036854775808
    ID2 -1
    ID 9223372036854775807
    ID2 -1
    I_MIN -170141183460469231731687303715884105728
    I_MAX 170141183460469231731687303715884105727
    I_MIN -170141183460469231731687303715884105728
    I_MAX 170141183460469231731687303715884105727
    PLAN (V_TEST0 TEST0 ORDER TEST0_I_MIN_DEC)
    MAX -170141183460469231731687303715884105728
    PLAN (V_TEST0 TEST0 ORDER TEST0_I_MIN_ASC)
    MIN -170141183460469231731687303715884105728
    PLAN (V_TEST0 TEST0 ORDER TEST0_I_MAX_DEC)
    MAX 170141183460469231731687303715884105727
    PLAN (V_TEST0 TEST0 ORDER TEST0_I_MAX_ASC)
    MIN 170141183460469231731687303715884105727
    I_MIN -170141183460469231731687303715884105728
    I_MAX 170141183460469231731687303715884105727
    I_MIN -170141183460469231731687303715884105728
    I_MAX 170141183460469231731687303715884105727
    I_MIN -170141183460469231731687303715884105728
    I_MAX 170141183460469231731687303715884105727
    I_MIN -170141183460469231731687303715884105728
    I_MAX 170141183460469231731687303715884105727
    Statement failed, SQLSTATE = 23000
    validation error for column "TEST2"."I_MIN", value "-2"
    Statement failed, SQLSTATE = 23000
    validation error for column "TEST2"."I_MIN", value "2"
    V_MIN -170141183460469231731687303715884105728
    V_MIN 170141183460469231731687303715884105727
    P_MIN 170141183460469231731687303715884105727
    P_MAX -170141183460469231731687303715884105728
    Statement failed, SQLSTATE = HY000
    exception 1
    -EX_ZERO_DIV_NOT_ALLOWED
    -Can not delete -170141183460469231731687303715884105728 by zero
    -At procedure 'SP_ZERO_DIV'
    P_MIN 1
    P_MIN -1
"""

expected_stdout_6x = """
    ID 1
    ID2 170141183460469231731687303715884105726
    ID -9223372036854775808
    ID2 -1
    ID 9223372036854775807
    ID2 -1
    I_MIN -170141183460469231731687303715884105728
    I_MAX 170141183460469231731687303715884105727
    I_MIN -170141183460469231731687303715884105728
    I_MAX 170141183460469231731687303715884105727
    PLAN ("PUBLIC"."V_TEST0" "PUBLIC"."TEST0" ORDER "PUBLIC"."TEST0_I_MIN_DEC")
    MAX -170141183460469231731687303715884105728
    PLAN ("PUBLIC"."V_TEST0" "PUBLIC"."TEST0" ORDER "PUBLIC"."TEST0_I_MIN_ASC")
    MIN -170141183460469231731687303715884105728
    PLAN ("PUBLIC"."V_TEST0" "PUBLIC"."TEST0" ORDER "PUBLIC"."TEST0_I_MAX_DEC")
    MAX 170141183460469231731687303715884105727
    PLAN ("PUBLIC"."V_TEST0" "PUBLIC"."TEST0" ORDER "PUBLIC"."TEST0_I_MAX_ASC")
    MIN 170141183460469231731687303715884105727
    I_MIN -170141183460469231731687303715884105728
    I_MAX 170141183460469231731687303715884105727
    I_MIN -170141183460469231731687303715884105728
    I_MAX 170141183460469231731687303715884105727
    I_MIN -170141183460469231731687303715884105728
    I_MAX 170141183460469231731687303715884105727
    I_MIN -170141183460469231731687303715884105728
    I_MAX 170141183460469231731687303715884105727
    Statement failed, SQLSTATE = 23000
    validation error for column "PUBLIC"."TEST2"."I_MIN", value "-2"
    Statement failed, SQLSTATE = 23000
    validation error for column "PUBLIC"."TEST2"."I_MIN", value "2"
    V_MIN -170141183460469231731687303715884105728
    V_MIN 170141183460469231731687303715884105727
    P_MIN 170141183460469231731687303715884105727
    P_MAX -170141183460469231731687303715884105728
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EX_ZERO_DIV_NOT_ALLOWED"
    -Can not delete -170141183460469231731687303715884105728 by zero
    -At procedure "PUBLIC"."SP_ZERO_DIV"
    P_MIN 1
    P_MIN -1
"""


@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
