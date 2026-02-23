#coding:utf-8

"""
ID:          issue-7652
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7652
TITLE:       Make the profiler store aggregated requests by default, with option for detailed store
DESCRIPTION:
    Test executes two times script with loop which does call to trivial stored procedure.
    First time we launch profiler without any parameters except session name, then - with 'DETAILED_REQUESTS'.
    In both cases (after finish profiling) we run SQL against tables plg$prof_requests and plg$prof_psql_stats
    and obtain TWO aggregated values: count number of records with request_id = 0 and request_id <> 0.
    For case when we call profiler WITHOUT any parameters (except session name):
    * before this ticket was implemented (for builds with date <= 27-JUN-2023) data in both mentioned tables
      did not contain records with request_id = 0 (i.e. all rows had request_id > 0);
    * after implementation ( https://github.com/FirebirdSQL/firebird/commit/00bb8e4581b66b624de47bfcde6b248e163ec6c1 )
      all rows in tables plg$prof_requests and plg$prof_psql_stats have request_id > 0.
    For case when we call profiler WITH 'DETAILED_REQUESTS':
    * builds with date <= 27-JUN-2023 could not be used, exception raised:
        Statement failed, SQLSTATE = 42000
        validation error for variable ATTACHMENT_ID, value "*** null ***"
        -At function 'RDB$PROFILER.START_SESSION'
    * after implementation all rows in tables plg$prof_requests and plg$prof_psql_stats have request_id > 0.
NOTES:
    Compared 5.0.0.1087 (26-JUN-2023) vs 5.0.0.1088 (27-JUN-2023)
    Checked on 6.0.0.395

    [13.07.2025] pzotov
    Adjusted for FB 6.x: it is MANDATORY to specify schema `PLG$PROFILER.` when querying created profiler tables.
    See doc/sql.extensions/README.schemas.md, section title: '### gbak'; see 'SQL_SCHEMA_PREFIX' variable here.
    Checked on 6.0.0.970; 5.0.3.1683.
"""
import pytest
from firebird.qa import *
import locale
import re

db = db_factory()
act = python_act('db', substitutions=[('[ \t]+', ' ')])

def strip_white(value):
    value = re.sub('(?m)^\\s+', '', value)
    return re.sub('(?m)\\s+$', '', value)

@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'PLG$PROFILER.'
    actual_out = ''
    test_sql = f"""
        set bail on;
        set list on;
        set term ^;
        create or alter procedure sp_rand_pair(a_i smallint) returns(r double precision, s varchar(36)) as
        begin
            if ( mod(a_i, 2) = 0 ) then
                begin
                    r = -1;
                    s = '';
                end
            else
                begin
                    r = rand();
                    s = uuid_to_char(gen_uuid());
                end
            suspend;
        end
        ^
        set term ;^
        commit;

        --set echo on;
        %(debug_message_sttm)s;
        %(profiler_start_sttm)s;
        --set echo off;

        set term ^;
        execute block as
            declare i int = 3;
            declare r double precision;
            declare s varchar(36);
        begin
            while (i > 0) do
            begin
                select r,s from sp_rand_pair( :i ) into r,s;
                i = i - 1;
            end
        end
        ^
        set term ;^
        execute procedure rdb$profiler.finish_session(true);
        commit;

        set transaction read committed;

        select
             iif( sum( iif(request_id = 0, 1, 0) ) > 0, 'NON_ZERO', 'ZERO' ) as requests_cnt_zero_request_id
            ,iif( sum( iif(request_id = 0, 0, 1) ) > 0, 'NON_ZERO', 'ZERO' ) as requests_cnt_non_zero_req_id
        from {SQL_SCHEMA_PREFIX}plg$prof_requests;

        select
             iif( sum( iif(request_id = 0, 1, 0) ) > 0, 'NON_ZERO', 'ZERO' ) as psql_stats_cnt_zero_request_id
            ,iif( sum( iif(request_id = 0, 0, 1) ) > 0, 'NON_ZERO', 'ZERO' ) as psql_stats_cnt_non_zero_req_id
        from {SQL_SCHEMA_PREFIX}plg$prof_psql_stats;
    """

    debug_message_sttm = "select 'DETAILED_REQUESTS: OFF' as msg from rdb$database"
    profiler_start_sttm = "select sign(rdb$profiler.start_session('prof_ssn_no_details')) from rdb$database"
    act.isql(input = test_sql % locals(), combine_output = True)
    actual_out += act.clean_stdout + '\n'
    act.reset()

    #-------------------------------------------------------------------------------------------------------

    debug_message_sttm = "select 'DETAILED_REQUESTS: ON' as msg from rdb$database"
    profiler_start_sttm = "select sign(rdb$profiler.start_session('prof_ssn_with_details', null, null, null, 'DETAILED_REQUESTS')) from rdb$database"
    act.isql(input = test_sql % locals(), combine_output = True)
    actual_out += act.clean_stdout + '\n'
    act.reset()

    expected_out = f"""
        MSG DETAILED_REQUESTS: OFF
        SIGN 1
        REQUESTS_CNT_ZERO_REQUEST_ID NON_ZERO
        REQUESTS_CNT_NON_ZERO_REQ_ID ZERO
        PSQL_STATS_CNT_ZERO_REQUEST_ID NON_ZERO
        PSQL_STATS_CNT_NON_ZERO_REQ_ID ZERO

        MSG DETAILED_REQUESTS: ON
        SIGN 1
        REQUESTS_CNT_ZERO_REQUEST_ID NON_ZERO
        REQUESTS_CNT_NON_ZERO_REQ_ID NON_ZERO
        PSQL_STATS_CNT_ZERO_REQUEST_ID NON_ZERO
        PSQL_STATS_CNT_NON_ZERO_REQ_ID NON_ZERO
    """
    assert strip_white(actual_out) == strip_white(expected_out)
