#coding:utf-8

"""
ID:          issue-7553
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7553
TITLE:       Firebird 5 profiler error with subselects
DESCRIPTION:
    Test checks that:
        1) profiler snapshot tables *do* have expected records after code described in the ticket;
        2) firebird.log have *no* differences related to FK violation in PLG$PROF_RECORD_SOURCES
NOTES:
    Confirmed bug on 5.0.0.1030.
    Checked on 5.0.0.1033 SS/CS (intermediate build, timestamp: 26.04.2023 08:00) -- all fine.

    [13.07.2025] pzotov
    Adjusted for FB 6.x: it is MANDATORY to specify schema `PLG$PROFILER.` when querying created profiler tables.
    See doc/sql.extensions/README.schemas.md, section title: '### gbak'; see 'SQL_SCHEMA_PREFIX' variable here.
    Checked on 6.0.0.970; 5.0.3.1683.
"""
import locale
import re
from difflib import unified_diff
import time

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db', substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'PLG$PROFILER.'

    # Get Firebird log before test
    fb_log_init = act.get_firebird_log()

    test_sql = f"""
        set term ^;
        execute block as
            declare dummy varchar(200);
            declare num bigint;
        begin
            select rdb$profiler.start_session('Profile Session 1') from rdb$database into :num;
            num = 1;
            if (:num in (select rdb$linger from rdb$database)) then
                dummy = 'xxx';

            execute procedure rdb$profiler.finish_session(true);
        end^
        set term ;^
        commit;

        set list on;
        set count on;

        with 
        p_ssn as (
            select profile_id
            from {SQL_SCHEMA_PREFIX}plg$prof_sessions
            order by 1 desc rows 1
        )
        select
             s.profile_id as p_sessions_profile_id
            ,q.counter    as p_psql_counter_id
            ,r.cursor_id     as p_recsource_cursor_id
            ,r.open_counter  as p_recsource_open_counter
            ,r.fetch_counter as p_recsource_fetch_counter
        from p_ssn s
        join {SQL_SCHEMA_PREFIX}plg$prof_psql_stats_view as q on s.profile_id = q.profile_id
        join {SQL_SCHEMA_PREFIX}plg$prof_record_source_stats_view r on s.profile_id = r.profile_id
        order by 1,2,3
        ;
    """

    act.expected_stdout = f"""
        P_SESSIONS_PROFILE_ID 1
        P_PSQL_COUNTER_ID 1
        P_RECSOURCE_CURSOR_ID 1
        P_RECSOURCE_OPEN_COUNTER 0
        P_RECSOURCE_FETCH_COUNTER 1

        P_SESSIONS_PROFILE_ID 1
        P_PSQL_COUNTER_ID 1
        P_RECSOURCE_CURSOR_ID 1
        P_RECSOURCE_OPEN_COUNTER 0
        P_RECSOURCE_FETCH_COUNTER 1
        
        P_SESSIONS_PROFILE_ID 1
        P_PSQL_COUNTER_ID 1
        P_RECSOURCE_CURSOR_ID 1
        P_RECSOURCE_OPEN_COUNTER 0
        P_RECSOURCE_FETCH_COUNTER 1
        
        P_SESSIONS_PROFILE_ID 1
        P_PSQL_COUNTER_ID 1
        P_RECSOURCE_CURSOR_ID 1
        P_RECSOURCE_OPEN_COUNTER 0
        P_RECSOURCE_FETCH_COUNTER 1
        
        P_SESSIONS_PROFILE_ID 1
        P_PSQL_COUNTER_ID 1
        P_RECSOURCE_CURSOR_ID 2
        P_RECSOURCE_OPEN_COUNTER 1
        P_RECSOURCE_FETCH_COUNTER 1
        
        P_SESSIONS_PROFILE_ID 1
        P_PSQL_COUNTER_ID 1
        P_RECSOURCE_CURSOR_ID 2
        P_RECSOURCE_OPEN_COUNTER 1
        P_RECSOURCE_FETCH_COUNTER 2
        
        P_SESSIONS_PROFILE_ID 1
        P_PSQL_COUNTER_ID 1
        P_RECSOURCE_CURSOR_ID 2
        P_RECSOURCE_OPEN_COUNTER 1
        P_RECSOURCE_FETCH_COUNTER 1
        
        P_SESSIONS_PROFILE_ID 1
        P_PSQL_COUNTER_ID 1
        P_RECSOURCE_CURSOR_ID 2
        P_RECSOURCE_OPEN_COUNTER 1
        P_RECSOURCE_FETCH_COUNTER 2
        
        P_SESSIONS_PROFILE_ID 1
        P_PSQL_COUNTER_ID 1
        P_RECSOURCE_CURSOR_ID 2
        P_RECSOURCE_OPEN_COUNTER 1
        P_RECSOURCE_FETCH_COUNTER 1
        
        P_SESSIONS_PROFILE_ID 1
        P_PSQL_COUNTER_ID 1
        P_RECSOURCE_CURSOR_ID 2
        P_RECSOURCE_OPEN_COUNTER 1
        P_RECSOURCE_FETCH_COUNTER 1

        Records affected: 10
    """
    act.isql(input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # Get Firebird log after test
    fb_log_curr = act.get_firebird_log()

    # Profiler flush
    # violation of FOREIGN KEY constraint "PLG$PROF_RECORD_SOURCES_CURSOR_FK" on table "PLG$PROF_RECORD_SOURCES"
    # Foreign key reference target does not exist
    # Problematic key value is ("PROFILE_ID" = 1, "STATEMENT_ID" = 140, "CURSOR_ID" = 2)

    allowed_patterns = [  re.compile('profiler\\s+flush',re.IGNORECASE),
                          re.compile('FOREIGN KEY',re.IGNORECASE),
                          re.compile('Problematic\\s+key',re.IGNORECASE)
                       ]

    for line in unified_diff(fb_log_init, fb_log_curr):
        if line.startswith('+') and act.match_any(line, allowed_patterns):
            print(f'UNEXPECTED MESSAGE IN FIREBIRD.LOG: {line.rstrip()}')
    
    assert '' == capsys.readouterr().out
