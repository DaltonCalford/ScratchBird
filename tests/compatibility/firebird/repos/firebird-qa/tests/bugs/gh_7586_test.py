#coding:utf-8

"""
ID:          issue-7586
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7586
TITLE:       Named arguments for function call, EXECUTE PROCEDURE and procedure record source
NOTES:
    [12.01.2024] pzotov
    Checked on 6.0.0.207
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    create or alter procedure sp_test (
         a_1 int default 1
        ,a_2 int default 2
        ,a_3 int default 3
        ,a_4 int default 4
        ,a_5 int default 5
    ) returns( o_sum int ) as
    begin
        o_sum = a_1 + a_2 + a_3 + a_4+ a_5;
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
        return a_1 + a_2 + a_3 + a_4+ a_5;
    end
    ^

    create or alter package pg_test as
    begin
        procedure sp(a_1 int default 1, a_2 int default 2, a_3 int default 3, a_4 int default 4, a_5 int default 5) returns(o_sum int);
        function fn(a_1 int default 1, a_2 int default 2, a_3 int default 3, a_4 int default 4, a_5 int default 5) returns int;
    end
    ^

    recreate package body pg_test as
    begin
        procedure sp(a_1 int, a_2 int, a_3 int, a_4 int, a_5 int) returns(o_sum int) as
        begin
            o_sum = a_1 + a_2 + a_3 + a_4 + a_5;
            suspend;
        end

        function fn(a_1 int, a_2 int, a_3 int, a_4 int, a_5 int) returns int as
        begin
            return a_1 + a_2 + a_3 + a_4+ a_5;
        end
    end
    ^
    set term ;^
    commit;

    select o_sum as standalone_sp_1 from sp_test(a_5 => -9);
    select o_sum as standalone_sp_2 from sp_test(a_4 => -11, a_5 => 19);
    select o_sum as standalone_sp_3 from sp_test(default, default, a_5 => 100, a_4 => -50, a_3 => 30);
    select o_sum as standalone_sp_4 from sp_test(-123, a_4 => 123);
    select o_sum as standalone_sp_5 from sp_test(-12, -23, default, a_4 => 123);

    ----------------------------------

    select fn_test(a_5 => -9) as standalone_fn_1 from rdb$database;
    select fn_test(a_4 => -11, a_5 => 19) as standalone_fn_2 from rdb$database;
    select fn_test(default, default, a_5 => 100, a_4 => -50, a_3 => 30) as standalone_fn_3 from rdb$database;
    select fn_test(-123, a_4 => 123) as standalone_fn_4 from rdb$database;
    select fn_test(-12, -23, default, a_4 => 123) as standalone_fn_5 from rdb$database;

    ----------------------------------

    select o_sum as packaged_sp_1 from pg_test.sp(a_5 => -9);
    select o_sum as packaged_sp_2 from pg_test.sp(a_4 => -11, a_5 => 19);
    select o_sum as packaged_sp_3 from pg_test.sp(default, default, a_5 => 100, a_4 => -50, a_3 => 30);
    select o_sum as packaged_sp_4 from pg_test.sp(-123, a_4 => 123);
    select o_sum as packaged_sp_5 from pg_test.sp(-12, -23, default, a_4 => 123);

    select pg_test.fn(a_5 => -9) as packaged_fn_1 from rdb$database;
    select pg_test.fn(a_4 => -11, a_5 => 19) as packaged_fn_2 from rdb$database;
    select pg_test.fn(default, default, a_5 => 100, a_4 => -50, a_3 => 30) as packaged_fn_3 from rdb$database;
    select pg_test.fn(-123, a_4 => 123) as packaged_fn_4 from rdb$database;
    select pg_test.fn(-12, -23, default, a_4 => 123) as packaged_fn_5 from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    STANDALONE_SP_1                 1
    STANDALONE_SP_2                 14
    STANDALONE_SP_3                 83
    STANDALONE_SP_4                 10
    STANDALONE_SP_5                 96
    STANDALONE_FN_1                 1
    STANDALONE_FN_2                 14
    STANDALONE_FN_3                 83
    STANDALONE_FN_4                 10
    STANDALONE_FN_5                 96
    PACKAGED_SP_1                   1
    PACKAGED_SP_2                   14
    PACKAGED_SP_3                   83
    PACKAGED_SP_4                   10
    PACKAGED_SP_5                   96
    PACKAGED_FN_1                   1
    PACKAGED_FN_2                   14
    PACKAGED_FN_3                   83
    PACKAGED_FN_4                   10
    PACKAGED_FN_5                   96
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
