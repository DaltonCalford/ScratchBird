#coding:utf-8

"""
ID:          session.alter-session-reset
TITLE:       Test results of ALTER SESSION RESET
DESCRIPTION:
NOTES:
[31.10.2019]
  Refactoring:
  * remove IDs of attachment/transaction from output.
  * replaced mon$isolation_mode with its describing text - take in account that in FB 4.0
    READ CONSISTENCY is default isolation mode for READ COMMITTED Tx.
FBTEST:      functional.session.alter_session_reset
"""

import pytest
from firebird.qa import *

substitutions = [  ('(-At)?\\s+line(:)?\\s+[\\d]+.*', '')
                  ,('(-)?Effective user is.*', 'Effective user')
                  ,('(-)?At procedure .*', 'At procedure')
                  ,('no permission for SELECT access to TABLE\\s+.*', 'no permission for SELECT access')
                ]

db = db_factory()

test_script = """
    --set bail on;
    -- set wng off;
    set autoddl off;
    set list on;
    -- connect '$(DSN)' user sysdba password 'masterkey';
    commit;

    create or alter user tmp$user4test password 'bar';
    commit;
    recreate global temporary table gtt_test(id int, x int) on commit preserve rows;
    commit;

    set term ^;
    execute block as
    begin
        begin
            execute statement 'drop role acnt';
        when any do begin end
        end
        begin
            execute statement 'drop role boss';
        when any do begin end
        end
    end
    ^
    create or alter procedure sp_decfloat_test returns( raised_gds int, raised_sqlst char(5) ) as
        declare a1 decfloat(34) = 9e6144;
    begin
        --rdb$set_context('USER_SESSION', 'RAISED_GDSCODE', null);
        --rdb$set_context('USER_SESSION', 'RAISED_SQLSTATE', null);
        begin
            ------------------------------------
            -- When decfloat traps set to default (Division_by_zero, Invalid_operation, Overflow):
            --     RAISED_GDS                      335545142
            --     RAISED_SQLST                    22003
            --     Message text: Decimal float overflow.  The exponent of a result is greater than the magnitude allowed.
            ------------------------------------
            -- When decfloat traps set to Inexact:
            --     RAISED_GDS                      335545140
            --     RAISED_SQLST                    22000
            --     Message text: Decimal float inexact result.  The result of an operation cannot be represented as a decimal fraction.
            a1 = a1 + a1;

            when any do
            begin
                --rdb$set_context('USER_SESSION', 'RAISED_GDSCODE', gdscode);
                --rdb$set_context('USER_SESSION', 'RAISED_SQLSTATE', sqlstate);
                raised_gds = gdscode;
                raised_sqlst = sqlstate;
                suspend;
                exception;
            end
        end
        suspend;
    end
    ^
    set term ;^
    commit;

    create role acnt;
    create role boss;
    commit;
    grant acnt to tmp$user4test;
    grant boss to tmp$user4test;
    grant execute on procedure sp_decfloat_test to tmp$user4test;
    commit;

    grant select, insert, update, delete on gtt_test to boss;
    commit;

    recreate view v_info as
    select
         current_user as my_name
        ,current_role as my_role
        ,t.mon$lock_timeout as tx_lock_timeout
        ,t.mon$read_only as tx_read_only
        ,t.mon$auto_undo as tx_auto_undo
        -- ,mon$isolation_mode as tx_isol_mode
        -- 15.01.2019: removed detailed info about read committed TIL because of read consistency TIL that 4.0 introduces.
        -- Any record with t.mon$isolation_mode = 4 now is considered just as read committed, w/o any detalization (this not much needed here).
        ,decode( t.mon$isolation_mode, 0,'CONSISTENCY', 1,'SNAPSHOT', 2, 'READ_COMMITTED', 3, 'READ_COMMITTED', 4, 'READ_COMMITTED', '??' ) as isol_descr
    from mon$transactions t
    where t.mon$attachment_id = current_connection;
    commit;

    recreate view v_ctx_vars as
    select m.mon$variable_name as context_var_name, m.mon$variable_value as context_var_value
    from rdb$database r left join mon$context_variables m on 1=1;

    grant select on v_info to public;
    grant select on v_ctx_vars to public;
    commit;

    connect '$(DSN)' user tmp$user4test password 'bar' role 'acnt';

    ------------------ 1: change Tx attributes ----------
    commit;
    set transaction read only read committed lock timeout 9 no auto undo;

    ------------------ 2: change current role -----------
    set role boss;

    ------------------ 3: write into session-level GTT
    -- should PASS because now we work as BOSS who has granted to write into this table:
    insert into gtt_test(id, x) values(1,100);

    ------------------ 4: define session-level context variables
    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION','WHATS_MY_NAME', current_user);
        rdb$set_context('USER_SESSION','WHATS_MY_ROLE', current_role);
    end^
    set term ;^

    ---------------------- 5: make num overflow with default decfloat trap settings
    -- Should raise:
    -- Statement failed, SQLSTATE = 22003
    -- Decimal float overflow.  The exponent of a result is greater than the magnitude allowed.

    select 'Point before call sp_decfloat_test with trap settings: {Division_by_zero, Invalid_operation, Overflow}' as msg from rdb$database;
    select * from sp_decfloat_test;

    -- this will CLEAR previous trap settings which were: {Division_by_zero, Invalid_operation, Overflow}
    set decfloat traps to Inexact;


    select 'Point before call sp_decfloat_test with trap settings: {Inexact}' as msg from rdb$database;
    -- Should raise:
    --  Statement failed, SQLSTATE = 22000
    --  Decimal float inexact result.  The result of an operation cannot be represented as a decimal fraction.
    select * from sp_decfloat_test;


    ------------------ check state  BEFORE session reset:
    select * from v_info;
    set count on;
    select * from gtt_test; -- should PASS because current role is BOSS and it was allowed to query GTT_TEST table to this role.
    set count off;
    select * from v_ctx_vars; -- should issue two records for context vars: WHATS_MY_NAME='TMP$USER4TEST' and WHATS_MY_ROLE='BOSS'


    -- ###################################
    -- ### R E S E T     S E S S I O N ###
    -- ###################################

    -- Should issue:
    -- Session was reset with warning(s)
    -- -Transaction is rolled back due to session reset, all changes are lost
    --##################
    alter session reset;
    --##################


    select 'Point AFTER reset session, before call sp_decfloat_test' as msg from rdb$database;
    -- Should raise:
    -- Statement failed, SQLSTATE = 22003
    -- Decimal float overflow.  The exponent of a result is greater than the magnitude allowed.
    -- (i.e. the same as it was when trap setting were: {Division_by_zero, Invalid_operation, Overflow} )
    select * from sp_decfloat_test;


    ------------------ check state AFTER session reset:

    select * from v_info; -- should contain "MY_ROLE       ACNT"
    set count on;
    select * from gtt_test; -- should raise Statement failed, SQLSTATE = 28000 / no permission for SELECT access to TABLE GTT_TEST / -Effective user is TMP$USER4TEST
    set count off;
    select * from v_ctx_vars; -- should issue one record with nulls: no context variables exist now because ALTER SESSION RESET was done.
    commit;

    -- cleanup:
    -- ########
    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$user4test;
    commit;

"""

act = isql_act('db', test_script, substitutions=substitutions)

@pytest.mark.version('>=4.0')
def test_1(act: Action):

    expected_stdout = """
        MSG                             Point before call sp_decfloat_test with trap settings: {Division_by_zero, Invalid_operation, Overflow}
        RAISED_GDS                      335545142
        RAISED_SQLST                    22003
        Statement failed, SQLSTATE = 22003
        Decimal float overflow.  The exponent of a result is greater than the magnitude allowed.
        At procedure
        At procedure
        MSG                             Point before call sp_decfloat_test with trap settings: {Inexact}
        RAISED_GDS                      335545140
        RAISED_SQLST                    22000
        Statement failed, SQLSTATE = 22000
        Decimal float inexact result.  The result of an operation cannot be represented as a decimal fraction.
        At procedure
        At procedure
        MY_NAME                         TMP$USER4TEST
        MY_ROLE                         BOSS
        TX_LOCK_TIMEOUT                 9
        TX_READ_ONLY                    1
        TX_AUTO_UNDO                    0
        ISOL_DESCR                      READ_COMMITTED
        ID                              1
        X                               100
        Records affected: 1
        CONTEXT_VAR_NAME                WHATS_MY_NAME
        CONTEXT_VAR_VALUE               TMP$USER4TEST
        CONTEXT_VAR_NAME                WHATS_MY_ROLE
        CONTEXT_VAR_VALUE               BOSS
        Session was reset with warning(s)
        -Transaction is rolled back due to session reset, all changes are lost
        MSG                             Point AFTER reset session, before call sp_decfloat_test
        RAISED_GDS                      335545142
        RAISED_SQLST                    22003
        Statement failed, SQLSTATE = 22003
        Decimal float overflow.  The exponent of a result is greater than the magnitude allowed.
        At procedure
        At procedure
        MY_NAME                         TMP$USER4TEST
        MY_ROLE                         ACNT
        TX_LOCK_TIMEOUT                 9
        TX_READ_ONLY                    1
        TX_AUTO_UNDO                    0
        ISOL_DESCR                      READ_COMMITTED
        Statement failed, SQLSTATE = 28000
        no permission for SELECT access
        Effective user
        CONTEXT_VAR_NAME                <null>
        CONTEXT_VAR_VALUE               <null>
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
