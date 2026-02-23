#coding:utf-8

"""
ID:          issue-7456
ISSUE:       7456
TITLE:       Impossible drop function in package with name of PSQL-function
NOTES:
    [15.02.2023] pzotov
    Confirmed bug on 5.0.0.917
    Checked on 5.0.0.920, 4.0.3.2900, 3.0.11.33664 -- all fine.

    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow to specify THE SAME terminator twice,
    i.e.
    set term @; select 1 from rdb$database @ set term @; - will not compile ("Unexpected end of command" raises).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    set term ^;
    create or alter function func1 returns int as
    begin
        return 1;
    end
    ^

    create or alter package test_pkg as
    begin
        -- same name of real function, but in package:
        function func1() returns int;
        function func2() returns int;
    end
    ^

    recreate package body test_pkg as
    begin
        function func1() returns int as
        begin
            return 11;
        end

        function func2() returns int as
        begin
            return 12;
        end
    end
    ^

    create or alter procedure test_proc as
        declare new_int int;
    begin
        new_int = func1();
        rdb$set_context('USER_SESSION','STANDALONE_PROC_DONE',1);
    end
    ^

    -- used psql-function, function in package no need, drop it:
    create or alter package test_pkg as
    begin
        function func2() returns int;
    end
    ^
    set term ;^

    execute procedure test_proc;
    select rdb$get_context('USER_SESSION','STANDALONE_PROC_DONE') as STANDALONE_PROC_DONE from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    STANDALONE_PROC_DONE            1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
