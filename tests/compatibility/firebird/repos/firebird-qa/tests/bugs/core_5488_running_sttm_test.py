#coding:utf-8

"""
ID:          issue-5758-A
ISSUE:       5758
TITLE:       Timeout for running SQL statement (SET STATEMENT TIMEOUT <N>)
DESCRIPTION:
  This is _initial_ test with trivial checks:
  1) it hould be possible to cancel 'heavy query' executed on <this_test_DB>;
  2) the same, but passed via ES/EDS, should also be cancelled;
  Elapsed time should be equal to statement timeout with precise -0.1% / +2%
  (minus 0.1% - because PC clock can be imprecise!)

  More complex cases will be implemented later.

  Explanations were given by hvlad, see letters from him of 05-mar-2017.
NOTES:
[01.04.2020]
  changed logic in execute block (see "--- 2 ===') after letter from Vlad, 31.03.2020 13:15.
  We must NOT suppose that engine could always in time do SUSPEND after ES/EDS will be interrupted.
  For this reason all results that we get inside EB must be stored as context variables, WITHOUT using
  output arguments which are returned by SUSPEND (see variables: 'returned_cnt', 'gds_after_eds').
  Engine restarts mechanism was changed 30-03-2020, see:
    https://github.com/FirebirdSQL/firebird/commit/059694b0b4f3a6f92f852b0a7be6358d708ee5e0
    et al.
JIRA:        CORE-5488
FBTEST:      bugs.core_5488
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    -- ####################################
    set statement timeout 1234 millisecond;
    -- ####################################


    ----------------------------------------  1 ----------------------------------------------

    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION','EB_FINAL_DTS', null);
        rdb$set_context('USER_SESSION','MIN_RATIO', 0.95); -- BEFORE 28.10.2019: 0.999
        rdb$set_context('USER_SESSION','MAX_RATIO', 1.05); -- BEFORE 28.10.2019: 1.02
        rdb$set_context('USER_SESSION','START_DTS', cast('now' as timestamp));
    end^
    set term ;^

    select count(*) from rdb$types,rdb$types,rdb$types;

    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION','EB_FINAL_DTS', cast('now' as timestamp));
    end^
    set term ;^


    select iif( 1.00 * elapsed_ms / sttm_timeout between low_bound and high_bound,
                'Acceptable',
                'WRONG: sttm_timeout = ' || sttm_timeout
                 || ', elapsed_ms = ' || elapsed_ms
                 || ', ratio elapsed_ms / sttm_timeout = '|| (1.00 * elapsed_ms / sttm_timeout)
                 ||' must belong to [' || low_bound || ' ... '|| high_bound || ']'
               ) as result_1
    from (
        select
            cast(rdb$get_context('SYSTEM','STATEMENT_TIMEOUT') as int) as sttm_timeout
            ,datediff(  millisecond
                       from cast( rdb$get_context('USER_SESSION','START_DTS') as timestamp)
                         to cast(rdb$get_context('USER_SESSION','EB_FINAL_DTS') as timestamp)
                    ) elapsed_ms
            ,cast( rdb$get_context('USER_SESSION','MIN_RATIO') as double precision) as low_bound   -- ### THRESHOLD, LOW  ###
            ,cast( rdb$get_context('USER_SESSION','MAX_RATIO') as double precision) as high_bound  -- ### THRESHOLD, HIGH ###
        from rdb$database
    );

    ----------------------------------------  2 ----------------------------------------------

    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION','EB_FINAL_DTS', null);
        rdb$set_context('USER_SESSION','START_DTS', cast('now' as timestamp));
    end
    ^

    execute block as
        declare p varchar(20) = 'masterkey';
        declare returned_cnt int = null;
        declare gds_after_eds int = null;
    begin

        begin
            execute statement 'select count(*) from rdb$types,rdb$types,rdb$types'
            on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
            as user current_user password :p role right( uuid_to_char(gen_uuid()), 12 )
            into returned_cnt;
        when any do
            begin
                -- Control MUST come here after statement will be interruptec (cancelled) in EDS.
                -- gdscode should be = 335544926.
                gds_after_eds = gdscode;
                rdb$set_context('USER_SESSION','GDS_IN_WHEN_ANY_BLOCK', gds_after_eds);
                rdb$set_context('USER_SESSION','TIMESTAMP_IN_WHEN_ANY_BLOCK', cast('now' as timestamp));
            end
        end
        if (gds_after_eds is null) then
        begin
            -- Normally we should NOT pass here, thus following context variables must remain NULL:
            rdb$set_context('USER_SESSION','EB_FINAL_DTS', timestamp '01.01.2000 00:00:01' );
            rdb$set_context('USER_SESSION','EB_EDS_CNT', -1);
        end

        -- control MUST come here, but: 'returned_cnt' will remain undefined, 'gds_after_eds'  will be 335544926.
        -- disabled 01.04.2020, see letter from hvlad 31.03.2020 -- suspend;

    end
    ^
    set term ;^

    -------------------------------------  verify results  ------------------------------------

    select iif( 1.00 * x.elapsed_ms / x.sttm_timeout between x.low_bound and x.high_bound,
                'Acceptable',
                'WRONG: sttm_timeout = ' || sttm_timeout
                 || ', elapsed_ms = ' || elapsed_ms
                 || ', ratio elapsed_ms / sttm_timeout = '|| (1.00 * elapsed_ms / sttm_timeout)
                 ||' must belong to [' || low_bound || ' ... '|| high_bound || ']'
               ) as result_2
           ,x.gds_in_when_any_block -- expected: 335544926, which means that ES/EDS was really interrupted
           ,iif( x.end_of_eb_cnt is null, 'EXPECTED, <null>', 'UNEXPECTED: ' || x.end_of_eb_cnt ) as end_of_eb_cnt -- expected: null which means that GDSCODE in when-any block had not-null value
    from (
        select
            cast(rdb$get_context('SYSTEM','STATEMENT_TIMEOUT') as int) as sttm_timeout
            ,datediff(  millisecond
                       from cast( rdb$get_context('USER_SESSION','START_DTS') as timestamp)
                         to cast(rdb$get_context('USER_SESSION','TIMESTAMP_IN_WHEN_ANY_BLOCK') as timestamp)
                    ) elapsed_ms
            ,rdb$get_context('USER_SESSION','GDS_IN_WHEN_ANY_BLOCK') as gds_in_when_any_block -- expected: 335544926
            ,rdb$get_context('USER_SESSION','EB_FINAL_DTS') end_of_eb_dts -- expected: NULL
            ,rdb$get_context('USER_SESSION','EB_EDS_CNT') as end_of_eb_cnt -- expected: NULL
            ,cast( rdb$get_context('USER_SESSION','MIN_RATIO') as double precision) as low_bound   -- ### THRESHOLD, LOW  ###
            ,cast( rdb$get_context('USER_SESSION','MAX_RATIO') as double precision) as high_bound  -- ### THRESHOLD, HIGH ###
        from rdb$database
    ) x;

    commit;
    --                                    ||||||||||||||||||||||||||||
    -- ###################################|||  FB 4.0+, SS and SC  |||##############################
    --                                    ||||||||||||||||||||||||||||
    -- If we check SS or SC and ExtConnPoolLifeTime > 0 (config parameter FB 4.0+) then current
    -- DB (bugs.core_NNNN.fdb) will be 'captured' by firebird.exe process and fbt_run utility
    -- will not able to drop this database at the final point of test.
    -- Moreover, DB file will be hold until all activity in firebird.exe completed and AFTER this
    -- we have to wait for <ExtConnPoolLifeTime> seconds after it (discussion and small test see
    -- in the letter to hvlad and dimitr 13.10.2019 11:10).
    -- This means that one need to kill all connections to prevent from exception on cleanup phase:
    -- SQLCODE: -901 / lock time-out on wait transaction / object <this_test_DB> is in use
    -- #############################################################################################
    delete from mon$attachments where mon$attachment_id != current_connection;
    commit;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    RESULT_1                        Acceptable

    RESULT_2                        Acceptable
    GDS_IN_WHEN_ANY_BLOCK           335544926
    END_OF_EB_CNT                   EXPECTED, <null>
"""

expected_stderr = """
    Statement failed, SQLSTATE = HY008
    operation was cancelled
    -Attachment level timeout expired.
"""

@pytest.mark.es_eds
@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

