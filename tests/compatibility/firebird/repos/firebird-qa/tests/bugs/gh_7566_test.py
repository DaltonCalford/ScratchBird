#coding:utf-8

"""
ID:          issue-7566
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7566
TITLE:       Allow DEFAULT keyword in argument list
NOTES:
    [12.01.2024] pzotov
    Checked on 6.0.0.207

    [21.01.2024] pzotov
    Added code for test "When parameter has no default, use domain's default for TYPE OF or NULL."
    https://github.com/FirebirdSQL/firebird/commit/8224df02787a4a07c4a5ba69ee24240fdf7d40b0
    See 'sp_domain_based_defaults', 'fn_domain_based_defaults' (standalone and packaged).
    Checked on 6.0.0.219
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set bail on;
    set list on;
    create domain dm_int int default current_connection;
    create domain dm_dts date default current_date;
    create domain dm_txt varchar(30) character set utf8 default 'liberté, égalité, fraternité' collate unicode_ci_ai;
    create domain dm_boo boolean;

    set term ^;
    create or alter procedure sp_test (
         a_1 int default 1
        ,a_2 int default 2
        ,a_3 int default 3
        ,a_4 int default 4
        ,a_5 int default 5
    ) returns( o_sum int ) as
    begin
        o_sum = a_1 + a_2 + a_3 + a_4 + a_5;
        suspend;
    end
    ^
    create or alter function fn_test (
         a_1 int default 1
        ,a_2 int default 2
        ,a_3 int default 3
        ,a_4 int default 4
        ,a_5 int default 5
    ) returns int as
    begin
        return a_1 + a_2 + a_3 + a_4 + a_5;
    end
    ^

    create or alter procedure sp_domain_based_defaults (
         a_1 dm_int
        ,a_2 dm_dts
        ,a_3 dm_txt
        ,a_4 dm_boo
    ) returns( o_result boolean ) as
    begin
        o_result = a_1 = current_connection and a_2 >= current_date and a_3 similar to '%éGALITé%' and a_4 is null;
        suspend;
    end
    ^

    create or alter function fn_domain_based_defaults (
         a_1 dm_int
        ,a_2 dm_dts
        ,a_3 dm_txt
        ,a_4 dm_boo
    ) returns boolean as
    begin
        return a_1 = current_connection and a_2 >= current_date and a_3 similar to '%éGALITé%' and a_4 is null;
    end
    ^

    create or alter package pg_test as
    begin
        procedure sp(a_1 int default 1, a_2 int default 2, a_3 int default 3, a_4 int default 4, a_5 int default 5) returns(o_sum int);
        function fn(a_1 int default 1, a_2 int default 2, a_3 int default 3, a_4 int default 4, a_5 int default 5) returns int;

        procedure sp_domain_based_defaults(a_1 dm_int, a_2 dm_dts, a_3 dm_txt, a_4 dm_boo) returns(o_result boolean);
        function fn_domain_based_defaults (a_1 dm_int, a_2 dm_dts, a_3 dm_txt, a_4 dm_boo) returns boolean;
    end
    ^

    recreate package body pg_test as
    begin
        -- NB: we must SKIP specifying 'default' clause for input params in the package body, otherwise:
        -- Statement failed, SQLSTATE = 42000
        -- unsuccessful metadata update
        -- -RECREATE PACKAGE BODY PG_TEST failed
        -- procedure sp(a_1 int default 1, a_2 int default 2, a_3 int default 3, a_4 int default 4, a_5 int default 5) returns(o_sum int) as
        procedure sp(a_1 int, a_2 int, a_3 int, a_4 int, a_5 int) returns(o_sum int) as
        begin
            o_sum = a_1 + a_2 + a_3 + a_4 + a_5;
            suspend;
        end

        function fn(a_1 int, a_2 int, a_3 int, a_4 int, a_5 int) returns int as
        begin
            return a_1 + a_2 + a_3 + a_4+ a_5;
        end

        procedure sp_domain_based_defaults(a_1 dm_int, a_2 dm_dts, a_3 dm_txt, a_4 dm_boo) returns(o_result boolean) as
        begin
            o_result = a_1 = current_connection and a_2 >= current_date and a_3 similar to '%éGALITé%' and a_4 is null;
            suspend;
        end

        function fn_domain_based_defaults (a_1 dm_int, a_2 dm_dts, a_3 dm_txt, a_4 dm_boo) returns boolean as
        begin
            return a_1 = current_connection and a_2 >= current_date and a_3 similar to '%éGALITé%' and a_4 is null;
        end
    end
    ^

    set term ;^
    commit;

    select o_sum as standalone_sp_1 from sp_test;
    select o_sum as standalone_sp_2 from sp_test(default, default, default, default, default);
    select o_sum as standalone_sp_3 from sp_test(default,      -1, default,      -2, default);
    select o_sum as standalone_sp_4 from sp_test(     -1, default, default, default,      -5);

    ----------------------------------

    select fn_test() as standalone_fn_1 from rdb$database;
    select fn_test(default, default, default, default, default) as standalone_fn_2 from rdb$database;
    select fn_test(default,      -1, default,      -2, default) as standalone_fn_3 from rdb$database;
    select fn_test(     -1, default, default, default,      -5) as standalone_fn_4 from rdb$database;

    ----------------------------------

    select o_sum as packaged_sp_1 from pg_test.sp;
    select o_sum as packaged_sp_2 from pg_test.sp(default, default, default, default, default);
    select o_sum as packaged_sp_3 from pg_test.sp(default,      -1, default,      -2, default);
    select o_sum as packaged_sp_4 from pg_test.sp(     -1, default, default, default,      -5);

    select pg_test.fn() as packaged_fn_1 from rdb$database;
    select pg_test.fn(default, default, default, default, default) as packaged_fn_2 from rdb$database;
    select pg_test.fn(default,      -1, default,      -2, default) as packaged_fn_3 from rdb$database;
    select pg_test.fn(     -1, default, default, default,      -5) as packaged_fn_4 from rdb$database;

    ----------------------------------

    select o_result as standalone_sp_domain_defaults from sp_domain_based_defaults(default, default, default, default);
    select fn_domain_based_defaults(default, default, default, default) as standalone_fn_domain_defaults from rdb$database;

    select o_result as packaged_sp_domain_defaults from pg_test.sp_domain_based_defaults(default, default, default, default);
    select pg_test.fn_domain_based_defaults(default, default, default, default) as packaged_fn_domain_defaults from rdb$database;

"""

act = isql_act('db', test_script)

expected_stdout = """
    STANDALONE_SP_1                 15
    STANDALONE_SP_2                 15
    STANDALONE_SP_3                 6
    STANDALONE_SP_4                 3
    STANDALONE_FN_1                 15
    STANDALONE_FN_2                 15
    STANDALONE_FN_3                 6
    STANDALONE_FN_4                 3
    PACKAGED_SP_1                   15
    PACKAGED_SP_2                   15
    PACKAGED_SP_3                   6
    PACKAGED_SP_4                   3
    PACKAGED_FN_1                   15
    PACKAGED_FN_2                   15
    PACKAGED_FN_3                   6
    PACKAGED_FN_4                   3
    STANDALONE_SP_DOMAIN_DEFAULTS   <true>
    STANDALONE_FN_DOMAIN_DEFAULTS   <true>
    PACKAGED_SP_DOMAIN_DEFAULTS     <true>
    PACKAGED_FN_DOMAIN_DEFAULTS     <true>
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
