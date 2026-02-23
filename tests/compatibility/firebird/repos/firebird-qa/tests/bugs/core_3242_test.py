#coding:utf-8

"""
ID:          issue-3612
ISSUE:       3612
TITLE:       Recursive stored procedure shouldnt require execute right to call itself
DESCRIPTION:
JIRA:        CORE-3242
FBTEST:      bugs.core_3242
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set wng off;
    set bail on;

    -- Drop old account if it remains from prevoius run:
    set term ^;
    execute block as
    begin
        begin
            execute statement 'drop user tmp$c3242' with autonomous transaction;
            when any do begin end
        end
    end
    ^
    set term ;^
    commit;

    create user tmp$c3242 password '123';
    commit;

    set term ^;
    create or alter procedure sp_recur(i smallint) returns(o bigint) as
    begin
        if ( i > 1 ) then
            o = i * (select o from sp_recur( :i - 1 ));
        else
            o = i;

        suspend;

    end
    ^

    create or alter function fn_recur(i smallint) returns bigint as
    begin
        if ( i > 1 ) then
            return i * fn_recur( i-1 );
        else
            return i;
    end
    ^

    create or alter procedure sp_factorial(i smallint) returns (o bigint) as
    begin
        for select o from sp_recur( :i ) into o do suspend;
    end
    ^
    create or alter function fn_factorial(i smallint) returns bigint as
    begin
        return fn_recur( i );
    end
    ^

    recreate package pg_factorial as
    begin
        procedure pg_sp_factorial(i smallint) returns (o bigint);
        function pg_fn_factorial(i smallint) returns bigint;
    end
    ^

    create package body pg_factorial as
    begin
        procedure pg_sp_factorial(i smallint) returns(o bigint) as
        begin
            if ( i > 1 ) then
                o = i * (select o from sp_recur( :i - 1 ));
            else
                o = i;

            suspend;

        end

        function pg_fn_factorial(i smallint) returns bigint as
        begin
            if ( i > 1 ) then
                return i * fn_recur( i-1 );
            else
                return i;
        end
    end
    ^
    set term ;^
    commit;


    ---------------------------------------------------------------------------

    revoke all on all from tmp$c3242;
    commit;

    grant execute on function fn_factorial to tmp$c3242;
    grant execute on procedure sp_factorial to tmp$c3242;
    grant execute on package pg_factorial to tmp$c3242;

    grant execute on function fn_recur to function fn_factorial;
    grant execute on procedure sp_recur to procedure sp_factorial;
    grant execute on function fn_recur to package pg_factorial;
    grant execute on procedure sp_recur to package pg_factorial;
    commit;

    connect '$(DSN)' user tmp$c3242 password '123';

    set list on;
    select p.o as standalone_sp_result from sp_factorial(5) p;
    select fn_factorial(7) as standalone_fn_result from rdb$database;

    select p.o as packaged_sp_result from pg_factorial.pg_sp_factorial(9) p;
    select pg_factorial.pg_fn_factorial(11) as packaged_fn_result from rdb$database;
    commit;

    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$c3242;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    STANDALONE_SP_RESULT            120
    STANDALONE_FN_RESULT            5040
    PACKAGED_SP_RESULT              362880
    PACKAGED_FN_RESULT              39916800
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

