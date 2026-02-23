#coding:utf-8

"""
ID:          issue-6583
ISSUE:       6583
TITLE:       Make explicit basic type for high precision numerics - INT128
DESCRIPTION:
    Initial discuss with Alex: letter 24.06.2020 18:29.
    This test most probably will be added by another checks, currently it has initial state.
    We verify that:
    1) one may to write:  create table test( x int128 ); -- i.e. explicitly specify type = 'int128'
    2) table column can refer to domain which was declared as int128
    3) one may to write SET BIND OF INT128 TO <any_other_numeric_datatype> ans vice versa.
JIRA:        CORE-6342
FBTEST:      bugs.core_6342
NOTES:
    [13.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test1(x int);

    set term ^;
    execute block as
    begin
        begin
            execute statement 'drop domain dm_128_a';
            when any do begin end
        end
        begin
            execute statement 'drop domain dm_128_b';
            when any do begin end
        end
        begin
            execute statement 'drop domain dm_128_c';
            when any do begin end
        end
    end
    ^
    set term ;^

    create domain dm_128_a int128;
    create domain dm_128_b as numeric(20,2) default -9999999999999999991;  -- CORE-6294
    --create domain dm_128_c as bigint default -9223372036854775808;  -- CORE-6291
    create domain dm_128_c as int128 default -9223372036854775807;
    recreate table test1(x int128 default 9999999999999999991, a dm_128_a default 9223372036854775807, b dm_128_b, c dm_128_c);

    set sqlda_display on;
    select * from test1;
    commit;
    set sqlda_display off;
    -------------------------


    recreate table test2(x numeric(19), y numeric(38,38));

    set bind of numeric to decimal;
    set sqlda_display on;
    select * from test2;
    set sqlda_display off;

    ----------------------------

    set bind of int128 to smallint;

    recreate table test3(x int128);
    set sqlda_display on;
    select * from test3;
    commit;
    set sqlda_display off;

    ----------------------------

    set bind of int128 to decimal;

    recreate table test4(x int128);
    set sqlda_display on;
    select * from test4;
    commit;
    set sqlda_display off;


    ----------------------------

    set bind of int128 to numeric;

    recreate table test5(x int128);
    set sqlda_display on;
    select * from test5;
    commit;
    set sqlda_display off;

    ----------------------------

    set bind of int128 to int;

    recreate table test6(x int128);
    set sqlda_display on;
    select * from test6;
    commit;
    set sqlda_display off;

    ----------------------------

    set bind of int128 to bigint;

    recreate table test7(x int128);
    set sqlda_display on;
    select * from test7;
    commit;
    set sqlda_display off;

    ----------------------------

    set bind of int128 to double precision;

    recreate table test8(x int128);
    set sqlda_display on;
    select * from test8;
    commit;
    set sqlda_display off;


    ----------------------------

    set bind of int128 to float;

    recreate table test9(x int128);
    set sqlda_display on;
    select * from test9;
    commit;
    set sqlda_display off;

    --#############################

    set bind of smallint to int128;

    recreate table test10(x smallint);
    set sqlda_display on;
    select * from test10;
    commit;
    set sqlda_display off;

    ---------------------------------

    set bind of int to int128;

    recreate table test11(x int);
    set sqlda_display on;
    select * from test11;
    commit;
    set sqlda_display off;

    ---------------------------------

    set bind of bigint to int128;

    recreate table test12(x bigint);
    set sqlda_display on;
    select * from test12;
    commit;
    set sqlda_display off;

    ---------------------------------

    set bind of numeric to int128;

    recreate table test13(x numeric(9,2));
    set sqlda_display on;
    select * from test13;
    commit;
    set sqlda_display off;

    ---------------------------------

    set bind of decimal to int128;

    recreate table test14(x decimal(19,19));
    set sqlda_display on;
    select * from test14;
    commit;
    set sqlda_display off;

    ---------------------------------

    set bind of double precision to int128;

    recreate table test15(x double precision);
    set sqlda_display on;
    select * from test15;
    commit;
    set sqlda_display off;

    ---------------------------------

    set bind of float to int128;

    recreate table test16(x float);
    set sqlda_display on;
    select * from test16;
    commit;
    set sqlda_display off;
"""

act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE|sqltype).)*$', ''), ('[ \t]+', ' ')])

expected_stdout = """
    01: sqltype: 32752 INT128 Nullable scale: 0 subtype: 0 len: 16
    02: sqltype: 32752 INT128 Nullable scale: 0 subtype: 0 len: 16
    03: sqltype: 32752 INT128 Nullable scale: -2 subtype: 1 len: 16
    04: sqltype: 32752 INT128 Nullable scale: 0 subtype: 0 len: 16
    01: sqltype: 32752 INT128 Nullable scale: 0 subtype: 2 len: 16
    02: sqltype: 32752 INT128 Nullable scale: -38 subtype: 2 len: 16
    01: sqltype: 500 SHORT Nullable scale: 0 subtype: 0 len: 2
    01: sqltype: 32752 INT128 Nullable scale: 0 subtype: 2 len: 16
    01: sqltype: 32752 INT128 Nullable scale: 0 subtype: 1 len: 16
    01: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
    01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    01: sqltype: 480 DOUBLE Nullable scale: 0 subtype: 0 len: 8
    01: sqltype: 482 FLOAT Nullable scale: 0 subtype: 0 len: 4
    01: sqltype: 32752 INT128 Nullable scale: 0 subtype: 0 len: 16
    01: sqltype: 32752 INT128 Nullable scale: 0 subtype: 0 len: 16
    01: sqltype: 32752 INT128 Nullable scale: 0 subtype: 0 len: 16
    01: sqltype: 32752 INT128 Nullable scale: -2 subtype: 1 len: 16
    01: sqltype: 32752 INT128 Nullable scale: -19 subtype: 2 len: 16
    01: sqltype: 32752 INT128 Nullable scale: 0 subtype: 0 len: 16
    01: sqltype: 32752 INT128 Nullable scale: 0 subtype: 0 len: 16
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
