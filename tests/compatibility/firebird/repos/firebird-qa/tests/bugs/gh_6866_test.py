#coding:utf-8

"""
ID:          issue-6866
ISSUE:       6866
TITLE:       Some orphan records left at RDB$SECURITY_CLASSES and RDB$USER_PRIVILEGES after DROP PROCEDURE\\FUNCTION
DESCRIPTION:
  Code for 3.0.8 was separated from 4.x+: there is no 'sql security definer|invoker' clause before FB 4.x.
  Only procedures, functions and packages are checked here.
  More checks (for all other kinds of DB objects: tables, views etc) will be done in the test for GH-6868.
FBTEST:      bugs.gh_6866
"""

import pytest
from firebird.qa import *

substitutions = [('[ ]+', ' ')]

db = db_factory()

expected_stdout = """
    MSG                             Number of rows in rdb$security_classes and rdb$user_privileges was not changed.
"""

# version: 3.0.8

test_script_1 = """
    set bail on;
    set list on;
    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION', 'INIT_SEC_CLS', (SELECT COUNT(*) AS INIT_SEC_CLS FROM RDB$SECURITY_CLASSES));
        rdb$set_context('USER_SESSION', 'INIT_USR_PRV', (SELECT COUNT(*) AS INIT_SEC_CLS FROM RDB$USER_PRIVILEGES));
    end
    ^
    set term ;^


    create role tmp$gh_6866_boss;
    create or alter user tmp$gh_6866_john password '123' using plugin Srp;
    create or alter user tmp$gh_6866_mike password '456' using plugin Srp;
    grant tmp$gh_6866_boss to tmp$gh_6866_mike;

    set term ^;
    create function fn_bool_ssi returns boolean as begin return true; end
    ^
    create function fn_bool_ssd returns boolean as
    begin
        return fn_bool_ssi();
    end
    ^
    alter function fn_bool_ssi returns boolean as
    begin
        return fn_bool_ssd();
    end
    ^
    create procedure sp_test_ssi as begin end
    ^
    create procedure sp_test_ssd as
    begin
        execute procedure sp_test_ssi;
    end
    ^
    alter procedure sp_test_ssi as
    begin
        execute procedure sp_test_ssd;
    end
    ^
    create or alter package pg_test_ssd as
    begin
       procedure pg_sp1(a_id int);
       function pg_fn1 returns int;
    end
    ^
    create package body pg_test_ssd as
    begin
       procedure pg_sp1(a_id int) as
       begin
       end

       function pg_fn1 returns int as
       begin
           return 1;
       end
    end
    ^
    create or alter package pg_test_ssi as
    begin
       procedure pg_sp1(a_id int);
       function pg_fn1 returns int;
    end
    ^
    create package body pg_test_ssi as
    begin
       procedure pg_sp1(a_id int) as
       begin
       end

       function pg_fn1 returns int as
       begin
           return 1;
       end
    end
    ^
    set term ;^
    commit;

    grant execute on function fn_bool_ssi to role tmp$gh_6866_boss;
    grant execute on function fn_bool_ssd to tmp$gh_6866_john;

    grant execute on procedure sp_test_ssi to role tmp$gh_6866_boss;
    grant execute on procedure sp_test_ssd to tmp$gh_6866_john;

    grant execute on package pg_test_ssd to role tmp$gh_6866_boss;
    grant execute on package pg_test_ssd to tmp$gh_6866_john;
    grant execute on package pg_test_ssi to package pg_test_ssd;
    grant execute on package pg_test_ssd to package pg_test_ssi;

    grant execute on procedure sp_test_ssi to function fn_bool_ssi;
    grant execute on procedure sp_test_ssd to function fn_bool_ssi;
    grant execute on function fn_bool_ssi to procedure sp_test_ssi;
    grant execute on function fn_bool_ssd to procedure sp_test_ssi;
    grant execute on procedure sp_test_ssi to function fn_bool_ssd;
    grant execute on procedure sp_test_ssd to function fn_bool_ssd;
    grant execute on function fn_bool_ssi to procedure sp_test_ssd;
    grant execute on function fn_bool_ssd to procedure sp_test_ssd;

    grant execute on function fn_bool_ssi to package pg_test_ssd;
    grant execute on function fn_bool_ssi to package pg_test_ssi;
    grant execute on procedure sp_test_ssi to package pg_test_ssd;
    grant execute on procedure sp_test_ssi to package pg_test_ssi;
    grant execute on function fn_bool_ssd to package pg_test_ssd;
    grant execute on function fn_bool_ssd to package pg_test_ssi;
    grant execute on procedure sp_test_ssd to package pg_test_ssd;
    grant execute on procedure sp_test_ssd to package pg_test_ssi;

    grant execute on package pg_test_ssd to function fn_bool_ssi;
    grant execute on package pg_test_ssd to function fn_bool_ssd;
    grant execute on package pg_test_ssd to procedure sp_test_ssi;
    grant execute on package pg_test_ssd to procedure sp_test_ssd;
    grant execute on package pg_test_ssi to function fn_bool_ssi;
    grant execute on package pg_test_ssi to function fn_bool_ssd;
    grant execute on package pg_test_ssi to procedure sp_test_ssi;
    grant execute on package pg_test_ssi to procedure sp_test_ssd;

    commit;

    set term ^;
    alter function fn_bool_ssi returns boolean as begin end
    ^
    alter function fn_bool_ssd returns boolean as begin end
    ^
    alter procedure sp_test_ssi as begin end
    ^
    alter procedure sp_test_ssd as begin end
    ^
    set term ;^
    commit;

    drop function fn_bool_ssi;
    drop procedure sp_test_ssi;
    drop function fn_bool_ssd;
    drop procedure sp_test_ssd;
    drop package pg_test_ssd;
    drop package pg_test_ssi;
    drop user tmp$gh_6866_john using plugin Srp;
    drop user tmp$gh_6866_mike using plugin Srp;
    drop role tmp$gh_6866_boss;
    commit;

    set term ^;
    execute block returns(msg varchar(255)) as
    begin
        rdb$set_context('USER_SESSION', 'LAST_SEC_CLS', (SELECT COUNT(*) AS INIT_SEC_CLS FROM RDB$SECURITY_CLASSES));
        rdb$set_context('USER_SESSION', 'LAST_USR_PRV', (SELECT COUNT(*) AS INIT_SEC_CLS FROM RDB$USER_PRIVILEGES));
        msg = '';
        if ( rdb$get_context('USER_SESSION', 'INIT_SEC_CLS') <> rdb$get_context('USER_SESSION', 'LAST_SEC_CLS') ) then
        begin
            msg = 'rdb$security_classes count mismatch: init=' || rdb$get_context('USER_SESSION', 'INIT_SEC_CLS') || ' vs last=' || rdb$get_context('USER_SESSION', 'LAST_SEC_CLS');
            suspend;
        end

        if ( rdb$get_context('USER_SESSION', 'INIT_USR_PRV') <> rdb$get_context('USER_SESSION', 'LAST_USR_PRV') ) then
        begin
            msg = 'rdb$user_privileges count mismatch: init=' || rdb$get_context('USER_SESSION', 'INIT_USR_PRV') || ' vs last=' || rdb$get_context('USER_SESSION', 'LAST_USR_PRV');
            suspend;
        end
        if (msg = '' ) then
        begin
            msg = 'Number of rows in rdb$security_classes and rdb$user_privileges was not changed.';
            suspend;
        end
    end
    ^
    set term ;^
    commit;
"""

act_1 = isql_act('db', test_script_1, substitutions=substitutions)

@pytest.mark.version('>=3.0.8,<4.0')
def test_1(act_1: Action):
    act_1.expected_stdout = expected_stdout
    act_1.execute()
    assert act_1.clean_stdout == act_1.clean_expected_stdout

# version: 4.0

test_script_2 = """
    set bail on;
    set list on;
    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION', 'INIT_SEC_CLS', (SELECT COUNT(*) AS INIT_SEC_CLS FROM RDB$SECURITY_CLASSES));
        rdb$set_context('USER_SESSION', 'INIT_USR_PRV', (SELECT COUNT(*) AS INIT_SEC_CLS FROM RDB$USER_PRIVILEGES));
    end
    ^
    set term ;^


    create role tmp$gh_6866_boss;
    create or alter user tmp$gh_6866_john password '123' using plugin Srp;
    create or alter user tmp$gh_6866_mike password '456' using plugin Srp;
    grant tmp$gh_6866_boss to tmp$gh_6866_mike;

    set term ^;
    create function fn_bool_ssi returns boolean as begin return true; end
    ^
    create function fn_bool_ssd returns boolean sql security definer as
    begin
        return fn_bool_ssi();
    end
    ^
    alter function fn_bool_ssi returns boolean as
    begin
        return fn_bool_ssd();
    end
    ^
    create procedure sp_test_ssi as begin end
    ^
    create procedure sp_test_ssd sql security definer as
    begin
        execute procedure sp_test_ssi;
    end
    ^
    alter procedure sp_test_ssi as
    begin
        execute procedure sp_test_ssd;
    end
    ^
    create or alter package pg_test_ssd sql security definer as
    begin
       procedure pg_sp1(a_id int);
       function pg_fn1 returns int;
    end
    ^
    create package body pg_test_ssd as
    begin
       procedure pg_sp1(a_id int) as
       begin
       end

       function pg_fn1 returns int as
       begin
           return 1;
       end
    end
    ^
    create or alter package pg_test_ssi sql security invoker as
    begin
       procedure pg_sp1(a_id int);
       function pg_fn1 returns int;
    end
    ^
    create package body pg_test_ssi as
    begin
       procedure pg_sp1(a_id int) as
       begin
       end

       function pg_fn1 returns int as
       begin
           return 1;
       end
    end
    ^
    set term ;^
    commit;

    grant execute on function fn_bool_ssi to role tmp$gh_6866_boss;
    grant execute on function fn_bool_ssd to tmp$gh_6866_john;

    grant execute on procedure sp_test_ssi to role tmp$gh_6866_boss;
    grant execute on procedure sp_test_ssd to tmp$gh_6866_john;

    grant execute on package pg_test_ssd to role tmp$gh_6866_boss;
    grant execute on package pg_test_ssd to tmp$gh_6866_john;
    grant execute on package pg_test_ssi to package pg_test_ssd;
    grant execute on package pg_test_ssd to package pg_test_ssi;

    grant execute on procedure sp_test_ssi to function fn_bool_ssi;
    grant execute on procedure sp_test_ssd to function fn_bool_ssi;
    grant execute on function fn_bool_ssi to procedure sp_test_ssi;
    grant execute on function fn_bool_ssd to procedure sp_test_ssi;
    grant execute on procedure sp_test_ssi to function fn_bool_ssd;
    grant execute on procedure sp_test_ssd to function fn_bool_ssd;
    grant execute on function fn_bool_ssi to procedure sp_test_ssd;
    grant execute on function fn_bool_ssd to procedure sp_test_ssd;

    grant execute on function fn_bool_ssi to package pg_test_ssd;
    grant execute on function fn_bool_ssi to package pg_test_ssi;
    grant execute on procedure sp_test_ssi to package pg_test_ssd;
    grant execute on procedure sp_test_ssi to package pg_test_ssi;
    grant execute on function fn_bool_ssd to package pg_test_ssd;
    grant execute on function fn_bool_ssd to package pg_test_ssi;
    grant execute on procedure sp_test_ssd to package pg_test_ssd;
    grant execute on procedure sp_test_ssd to package pg_test_ssi;

    grant execute on package pg_test_ssd to function fn_bool_ssi;
    grant execute on package pg_test_ssd to function fn_bool_ssd;
    grant execute on package pg_test_ssd to procedure sp_test_ssi;
    grant execute on package pg_test_ssd to procedure sp_test_ssd;
    grant execute on package pg_test_ssi to function fn_bool_ssi;
    grant execute on package pg_test_ssi to function fn_bool_ssd;
    grant execute on package pg_test_ssi to procedure sp_test_ssi;
    grant execute on package pg_test_ssi to procedure sp_test_ssd;

    commit;

    set term ^;
    alter function fn_bool_ssi returns boolean as begin end
    ^
    alter function fn_bool_ssd returns boolean as begin end
    ^
    alter procedure sp_test_ssi as begin end
    ^
    alter procedure sp_test_ssd as begin end
    ^
    set term ;^
    commit;

    drop function fn_bool_ssi;
    drop procedure sp_test_ssi;
    drop function fn_bool_ssd;
    drop procedure sp_test_ssd;
    drop package pg_test_ssd;
    drop package pg_test_ssi;
    drop user tmp$gh_6866_john using plugin Srp;
    drop user tmp$gh_6866_mike using plugin Srp;
    drop role tmp$gh_6866_boss;
    commit;

    set term ^;
    execute block returns(msg varchar(255)) as
    begin
        rdb$set_context('USER_SESSION', 'LAST_SEC_CLS', (SELECT COUNT(*) AS INIT_SEC_CLS FROM RDB$SECURITY_CLASSES));
        rdb$set_context('USER_SESSION', 'LAST_USR_PRV', (SELECT COUNT(*) AS INIT_SEC_CLS FROM RDB$USER_PRIVILEGES));
        msg = '';
        if ( rdb$get_context('USER_SESSION', 'INIT_SEC_CLS') <> rdb$get_context('USER_SESSION', 'LAST_SEC_CLS') ) then
        begin
            msg = 'rdb$security_classes count mismatch: init=' || rdb$get_context('USER_SESSION', 'INIT_SEC_CLS') || ' vs last=' || rdb$get_context('USER_SESSION', 'LAST_SEC_CLS');
            suspend;
        end

        if ( rdb$get_context('USER_SESSION', 'INIT_USR_PRV') <> rdb$get_context('USER_SESSION', 'LAST_USR_PRV') ) then
        begin
            msg = 'rdb$user_privileges count mismatch: init=' || rdb$get_context('USER_SESSION', 'INIT_USR_PRV') || ' vs last=' || rdb$get_context('USER_SESSION', 'LAST_USR_PRV');
            suspend;
        end
        if (msg = '' ) then
        begin
            msg = 'Number of rows in rdb$security_classes and rdb$user_privileges was not changed.';
            suspend;
        end
    end
    ^
    set term ;^
    commit;
"""

act_2 = isql_act('db', test_script_2, substitutions=substitutions)

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stdout = expected_stdout
    act_2.execute(combine_output = True)
    assert act_2.clean_stdout == act_2.clean_expected_stdout
