#coding:utf-8

"""
ID:          issue-6802
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6802
TITLE:       When the statement timeout is set, it causes the lock manager to delay reporting deadlocks until timeout is expired
DESCRIPTION:
FBTEST:      bugs.gh_6802
NOTES:
    [20.05.2021] pcizar
    adjusted expected_stderr for case-2: non-suppressed exception raises instead of issuing gdscode.

    [15.03.2024] pzotov
    Added combine_output in order to see concrete case where test fails.
    Checked on Windows, ServerMode = CS/SS: 4.0.5.3077, 5.0.1.1360, 6.0.0.288
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate global temporary table gt(f01 int) on commit preserve rows;
    commit;
    insert into gt values(1);
    commit;
    set term ^;
    create or alter procedure sp_test_waiting_time( a_expected_wait int ) returns(was_waiting_acceptable varchar(100)) as
    begin
        for
            select iif( waited_for_sec between expected_wait and expected_wait + 1
                        ,'Acceptable.'
                        ,'NO, UNEXPECTED: ' || waited_for_sec || ' - out of scope: ' || expected_wait || '...' || (expected_wait+1)
                      )
            from (
                select
                    datediff(second from cast( rdb$get_context('USER_SESSION', 'DTS_BEG') as timestamp) to cast('now' as timestamp)) as waited_for_sec
                   ,:a_expected_wait as expected_wait
                --  ^^
                -- #################################################
                -- ### EXPECTED TIME TO WAIT FOR LOCK RESOLUTION ###
                -- #################################################
                from rdb$database
            )
        into was_waiting_acceptable
        do
            suspend;
    end
    ^
    set term ;^
    commit;

    -- #######################
    -- ###  c a s e   N 1  ###
    -- #######################
    -- Initial state:
    --    * statement_timeout > deadlocktimeout (which is supposed to be default, i.e. 10 seconds)
    --    * Tx lock resolution = WAIT
    -- Expected result:
    --    * gdscode = 335544336 must be issued on console.
    --    * waiting for 10 sec (= actual value of 'DeadLockTimeout' parameter);

    SET STATEMENT TIMEOUT 13 second;
    commit;

    set transaction read write read committed record_version;

    delete from gt;
    set term ^;
    execute block returns(raised_gds_01 int) as
    begin
        rdb$set_context('USER_SESSION','DTS_BEG', cast('now' as timestamp));
        begin
            in autonomous transaction
            do update gt set f01=-1;
        when any do
            begin
                raised_gds_01 = gdscode;
            end
        end
        suspend;
    end
    ^
    set term ;^

    select was_waiting_acceptable as waiting_time_01 from sp_test_waiting_time( (select rdb$config_value from rdb$config where upper(rdb$config_name) = upper('DeadlockTimeout')) );
    rollback;


    -- #######################
    -- ###  c a s e   N 2  ###
    -- #######################
    -- Initial state:
    --    * statement_timeout < deadlocktimeout
    --    * Tx lock resolution = WAIT
    -- Expected result:
    --    * non-supressed exception "SQLSTATE = HY008 / operation was cancelled / -Attachment level timeout expired."
    --      ::: NOTE ::: EXCEPTION 'operation was cancelled' (isc_cancelled, gds = 335544794) can not be supressed in when-block!
    --      See letter from Vlad, 15.05.2021 19:42.
    --      NB:  gdscode = 335544336 issued on console on 5.0.0.29, but on 5.0.0.43 it is not so.
    --    * waiting for 4..5 sec (i.e. <StatementTimeout> seconds rather than DeadLockTimeout);

    SET STATEMENT TIMEOUT 4 second;
    commit;

    set transaction read write read committed record_version;

    delete from gt;
    set term ^;
    execute block returns(raised_gds_02 int) as
    begin
        rdb$set_context('USER_SESSION','DTS_BEG', cast('now' as timestamp));
        begin
            in autonomous transaction
            do update gt set f01=-1;

        when any do
            begin
                raised_gds_02 = gdscode;
            end
        end

        suspend;
    end
    ^
    set term ;^
    select was_waiting_acceptable as waiting_time_02 from sp_test_waiting_time( cast(rdb$get_context('SYSTEM', 'STATEMENT_TIMEOUT') as int) / 1000 );
    rollback;


    -- #######################
    -- ###  c a s e   N 3  ###
    -- #######################
    -- Initial state:
    --    * statement_timeout > 0 (no matter greater or less than deadlocktimeout);
    --    * Tx lock resolution = LOCK_TIMEOUT <N> and this value GREATER than statement_timeout
    -- Expected result:
    --    * non-supressed exception "SQLSTATE = HY008 / operation was cancelled / -Attachment level timeout expired."
    --      ::: NOTE ::: EXCEPTION 'operation was cancelled' (isc_cancelled, gds = 335544794) can not be supressed in when-block!
    --      See letter from Vlad, 15.05.2021 19:42.
    --    * waiting for value that is specified in STATEMENT TIMEOUT (with probable addition 1 second)

    SET STATEMENT TIMEOUT 4 second;
    commit;

    set transaction read write read committed record_version lock timeout 9;

    delete from gt;
    set term ^;
    execute block returns(raised_gds_03 int) as
    begin
        rdb$set_context('USER_SESSION','DTS_BEG', cast('now' as timestamp));
        begin
            in autonomous transaction
            do update gt set f01=-1;

        when any do
            begin
                raised_gds_03 = gdscode;
            end
        end

        suspend;
    end
    ^
    set term ;^
    select was_waiting_acceptable as waiting_time_03 from sp_test_waiting_time( cast(rdb$get_context('SYSTEM', 'STATEMENT_TIMEOUT') as int) / 1000 );
    rollback;


    -- #######################
    -- ###  c a s e   N 4  ###
    -- #######################
    -- Initial state:
    --    * statement_timeout  > 0 (no matter greater or less than deadlocktimeout);
    --    * Tx lock resolution = LOCK_TIMEOUT <N> and this value LESS than statement_timeout
    -- Expected result:
    --    * gdscode = 335544336 must be issued on console.
    --    * waiting for value that is specified in LOCK TIMEOUT clause (with probable addition 1 second)

    SET STATEMENT TIMEOUT 9 second;
    commit;

    set transaction read write read committed record_version lock timeout 3;

    delete from gt;
    set term ^;
    execute block returns(raised_gds_04 int) as
    begin
        rdb$set_context('USER_SESSION','DTS_BEG', cast('now' as timestamp));
        begin
            in autonomous transaction
            do update gt set f01=-1;

        when any do
            begin
                raised_gds_04 = gdscode;
            end
        end

        suspend;
    end
    ^
    set term ;^
    select was_waiting_acceptable as waiting_time_04 from sp_test_waiting_time( rdb$get_context('SYSTEM', 'LOCK_TIMEOUT') );
    rollback;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    RAISED_GDS_01 335544336
    WAITING_TIME_01 Acceptable.


    Statement failed, SQLSTATE = HY008
    operation was cancelled
    -Attachment level timeout expired.
    WAITING_TIME_02 Acceptable.
    

    Statement failed, SQLSTATE = HY008
    operation was cancelled
    -Attachment level timeout expired.
    WAITING_TIME_03 Acceptable.


    RAISED_GDS_04 335544336
    WAITING_TIME_04 Acceptable.
"""

@pytest.mark.version('>=4.0.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
