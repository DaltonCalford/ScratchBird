#coding:utf-8

"""
ID:          issue-7086
ISSUE:       7086
TITLE:       PSQL and SQL profiler
NOTES:
    [21.02.2023] pzotov
        Test verifies only example from doc/sql.extensions/README.profiler.md
        More complex checks will be implementer later.
        Checked on 5.0.0.958 SS/CS.
    [01.12.2023] pzotov
        New behaviour of ISQL was introduced after implementation of PR #7868: SET AUTOTERM.
        Since that was implemented, ISQL handles comments (single- and multi-lined) as PART of statement that follows these comments.
        In other words, ISQL in 6.x does not 'swallow' comments and sends them to engine together with statement that follows.
        This means that comment PLUS statement can be 'unexpectedly' seen in PROFILER tables (plg$prof_record_source_stats_view in this test).
        Currently this is not considered as a bug, see note by Adriano: https://groups.google.com/g/firebird-devel/c/AM8vlA3YJws
        Because of this, we have (in this test) to either not use comments at all or filter them out by applying substitution which
        will 'know' about some special text ('comment_tag') that must be suppressed.
        Checked on 6.0.0.163, 5.0.0.1284
    [13.07.2025] pzotov
        Adjusted output for FB 6.x: it is MANDATORY to specify schema `PLG$PROFILER.` when querying created profiler tables.
        See doc/sql.extensions/README.schemas.md, section title: '### gbak'; see 'SQL_SCHEMA_PREFIX' variable here.
        Added substitutions in order to ignore concrete values of line/col and key length when sorting occurs.
        Checked on 6.0.0.970; 5.0.3.1683.
"""
import locale
import os
import pytest
from firebird.qa import *

db = db_factory()


# Output contains lot of data with concrete values for attachment_id, timestamps etc.
# We have to check only presense of such lines and ignore these values:
ptn_list = [
     'ATTACHMENT_ID'
    ,'START_TIMESTAMP'
    ,'FINISH_TIMESTAMP'
    ,'SQL_TEXT'
    ,'LINE_NUM'
    ,'COLUMN_NUM'
    ,'MIN_ELAPSED_TIME'
    ,'MAX_ELAPSED_TIME'
    ,'TOTAL_ELAPSED_TIME'
    ,'AVG_ELAPSED_TIME'
    ,'STATEMENT_ID'
    ,'OPEN_MIN_ELAPSED_TIME'
    ,'OPEN_MAX_ELAPSED_TIME'
    ,'OPEN_TOTAL_ELAPSED_TIME'
    ,'OPEN_AVG_ELAPSED_TIME'
    ,'FETCH_COUNTER'
    ,'FETCH_MIN_ELAPSED_TIME'
    ,'FETCH_MAX_ELAPSED_TIME'
    ,'FETCH_TOTAL_ELAPSED_TIME'
    ,'FETCH_AVG_ELAPSED_TIME'
    ,'OPEN_FETCH_TOTAL_ELAPSED_TIME'
    ,'REQUEST_ID'
    ,'STATEMENT_ID'
    ,'CALLER_REQUEST_ID'
    ,'START_TIMESTAMP'
    ,'FINISH_TIMESTAMP'
    ,'TOTAL_ELAPSED_TIME'
]
sub_list = [ (x+' .*', x) for x in ptn_list ]

COMMENT_TAG = 'DONT_SHOW_IN_OUTPUT'

substitutions = [  ('[ \t]+', ' ')
                  ,(f'-- {COMMENT_TAG}.*', '')
                  ,('line(:)?\\s+\\d+, col(umn)?(:)?\\s+\\d+', 'line X col Y')
                  ,('rec(ord)?\\s+len(gth)?(:)?\\s+\\d+(,)?\\s+key\\s+len(gth)?(:)?\\s+\\d+', 'record length: R, key length: K')
                ] + sub_list
act = isql_act('db', substitutions = substitutions)

@pytest.mark.version('>=5.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'PLG$PROFILER.'
    test_script = f"""
        set list on;
        create table tab (
            id integer not null,
            val integer not null
        );

        set term ^;

        create or alter function mult(p1 integer, p2 integer) returns integer
        as
        begin
            return p1 * p2;
        end^

        create or alter procedure ins
        as
            declare n integer = 1;
        begin
            while (n <= 1000)
            do
            begin
                if (mod(n, 2) = 1) then
                    insert into tab values (:n, mult(:n, 2));
                n = n + 1;
            end
        end^
        set term ;^

        -- {COMMENT_TAG} ######################################
        -- {COMMENT_TAG} ###    Start profiling, session 1  ###
        -- {COMMENT_TAG} ######################################
        select rdb$profiler.start_session('profile session 1') from rdb$database;

        set term ^;

        execute block
        as
        begin
            execute procedure ins;
            delete from tab;
        end^
        set term ;^

        -- {COMMENT_TAG} ######################################
        -- {COMMENT_TAG} ###   Finish profiling session 1   ###
        -- {COMMENT_TAG} ######################################
        execute procedure rdb$profiler.finish_session(true);

        execute procedure ins;

        -- {COMMENT_TAG} ######################################
        -- {COMMENT_TAG} ###    Start profiling, session 2  ###
        -- {COMMENT_TAG} ######################################
        select rdb$profiler.start_session('profile session 2') from rdb$database;

        out {os.devnull};
        select mod(id, 5),
               sum(val)
          from tab
          where id <= 50
          group by mod(id, 5)
          order by sum(val);
        out;

        -- {COMMENT_TAG} ######################################
        -- {COMMENT_TAG} ###   Finish profiling session 2   ###
        -- {COMMENT_TAG} ######################################
        execute procedure rdb$profiler.finish_session(true);


        -- Data analysis

        commit;
        set transaction read committed;

        set count on;

        -- ##############################
        select
            '--- [ 1: plg$prof_sessions ] ---' as msg
            ,s.* 
        from {SQL_SCHEMA_PREFIX}plg$prof_sessions s
        order by profile_id
        ;

        -- ##############################
        select
            '--- [ 2: plg$prof_psql_stats_view ] ---' as msg
            ,p.profile_id
            ,p.statement_type
            ,p.sql_text
            ,p.line_num
            ,p.column_num
            ,p.counter
            ,p.min_elapsed_time
            ,p.max_elapsed_time
            ,p.total_elapsed_time
            ,p.avg_elapsed_time
        from {SQL_SCHEMA_PREFIX}plg$prof_psql_stats_view p
        order by p.profile_id,
                 p.statement_id,
                 p.line_num,
                 p.column_num
        ;

        -- ##############################
        select
           '--- [ 3: plg$prof_record_source_stats_view ] ---' as msg
           ,p.profile_id
           ,p.statement_id
           ,p.statement_type
           ,p.package_name
           ,p.routine_name
           ,p.parent_statement_id
           ,p.parent_statement_type
           ,p.parent_routine_name
           ,p.sql_text
           ,p.cursor_id
           ,p.cursor_name
           ,p.cursor_line_num
           ,p.cursor_column_num
           ,p.record_source_id
           ,p.parent_record_source_id
           ,p.access_path
           ,p.open_counter
           ,p.open_min_elapsed_time
           ,p.open_max_elapsed_time
           ,p.open_total_elapsed_time
           ,p.open_avg_elapsed_time
           ,p.fetch_counter
           ,p.fetch_min_elapsed_time
           ,p.fetch_max_elapsed_time
           ,p.fetch_total_elapsed_time
           ,p.fetch_avg_elapsed_time
           ,p.open_fetch_total_elapsed_time
        from {SQL_SCHEMA_PREFIX}plg$prof_record_source_stats_view p
        order by p.profile_id,
                 p.statement_id
        ;

        -- ##############################
        select
            '--- [ 4: plg$prof_requests ] ---' as msg
            ,q.profile_id
            ,q.request_id
            ,q.statement_id
            ,q.caller_request_id
            ,q.start_timestamp
            ,q.finish_timestamp
            ,q.total_elapsed_time
          from {SQL_SCHEMA_PREFIX}plg$prof_requests q
          join {SQL_SCHEMA_PREFIX}plg$prof_sessions s
            on s.profile_id = q.profile_id and
               s.description = 'profile session 1'
          order by q.profile_id,
                   q.statement_id,
                   q.request_id
        ;

        -- ##############################
        select -- pstat.*
            '--- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---' as msg
            ,t.profile_id
            ,t.request_id
            ,t.line_num
            ,t.column_num
            ,t.statement_id
            ,t.counter
            ,t.min_elapsed_time
            ,t.max_elapsed_time
            ,t.total_elapsed_time
          from {SQL_SCHEMA_PREFIX}plg$prof_psql_stats t
          join {SQL_SCHEMA_PREFIX}plg$prof_sessions s
            on s.profile_id = t.profile_id and
               s.description = 'profile session 1'
          order by t.profile_id,
                   t.statement_id,
                   t.request_id,
                   t.line_num,
                   t.column_num
        ;

        -- ##############################
        select
            '--- [ 6: plg$prof_record_source_stats ] ---' as msg
            ,t.profile_id
            ,t.request_id
            ,t.cursor_id
            ,t.record_source_id
            ,t.statement_id
            ,t.open_counter
            ,t.open_min_elapsed_time
            ,t.open_max_elapsed_time
            ,t.open_total_elapsed_time
            ,t.fetch_counter
            ,t.fetch_min_elapsed_time
            ,t.fetch_max_elapsed_time
            ,t.fetch_total_elapsed_time
          from {SQL_SCHEMA_PREFIX}plg$prof_record_source_stats t
          join {SQL_SCHEMA_PREFIX}plg$prof_sessions s
            on s.profile_id = t.profile_id and
               s.description = 'profile session 2'
          order by t.profile_id,
                   t.statement_id,
                   t.request_id,
                   t.cursor_id,
                   t.record_source_id
        ;
    """

    fb5x_expected_out = """
        START_SESSION 1
        START_SESSION 2
        MSG --- [ 1: plg$prof_sessions ] ---
        PROFILE_ID 1
        ATTACHMENT_ID
        USER_NAME SYSDBA
        DESCRIPTION profile session 1
        START_TIMESTAMP
        FINISH_TIMESTAMP
        MSG --- [ 1: plg$prof_sessions ] ---
        PROFILE_ID 2
        ATTACHMENT_ID
        USER_NAME SYSDBA
        DESCRIPTION profile session 2
        START_TIMESTAMP
        FINISH_TIMESTAMP
        Records affected: 2
        MSG --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID 1
        STATEMENT_TYPE BLOCK
        SQL_TEXT
        execute block
        as
        begin
        execute procedure ins;
        delete from tab;
        end
        LINE_NUM
        COLUMN_NUM
        COUNTER 1
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        MSG --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID 1
        STATEMENT_TYPE BLOCK
        SQL_TEXT
        execute block
        as
        begin
        execute procedure ins;
        delete from tab;
        end
        LINE_NUM
        COLUMN_NUM
        COUNTER 1
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        MSG --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID 1
        STATEMENT_TYPE PROCEDURE
        SQL_TEXT
        LINE_NUM
        COLUMN_NUM
        COUNTER 1
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        MSG --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID 1
        STATEMENT_TYPE PROCEDURE
        SQL_TEXT
        LINE_NUM
        COLUMN_NUM
        COUNTER 1001
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        MSG --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID 1
        STATEMENT_TYPE PROCEDURE
        SQL_TEXT
        LINE_NUM
        COLUMN_NUM
        COUNTER 1000
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        MSG --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID 1
        STATEMENT_TYPE PROCEDURE
        SQL_TEXT
        LINE_NUM
        COLUMN_NUM
        COUNTER 500
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        MSG --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID 1
        STATEMENT_TYPE PROCEDURE
        SQL_TEXT
        LINE_NUM
        COLUMN_NUM
        COUNTER 1000
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        MSG --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID 1
        STATEMENT_TYPE FUNCTION
        SQL_TEXT
        LINE_NUM
        COLUMN_NUM
        COUNTER 500
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        Records affected: 8
        MSG --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID 1
        STATEMENT_ID
        STATEMENT_TYPE BLOCK
        PACKAGE_NAME <null>
        ROUTINE_NAME <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE <null>
        PARENT_ROUTINE_NAME <null>
        SQL_TEXT
        select rdb$profiler.start_session('profile session 1') from rdb$database
        CURSOR_ID 1
        CURSOR_NAME <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID 2
        PARENT_RECORD_SOURCE_ID 1
        ACCESS_PATH 84:1
        -> Table "RDB$DATABASE" Full Scan
        OPEN_COUNTER 0
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID 1
        STATEMENT_ID
        STATEMENT_TYPE BLOCK
        PACKAGE_NAME <null>
        ROUTINE_NAME <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE <null>
        PARENT_ROUTINE_NAME <null>
        SQL_TEXT
        select rdb$profiler.start_session('profile session 1') from rdb$database
        CURSOR_ID 1
        CURSOR_NAME <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID 1
        PARENT_RECORD_SOURCE_ID <null>
        ACCESS_PATH 84:0
        Select Expression
        OPEN_COUNTER 0
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID 1
        STATEMENT_ID
        STATEMENT_TYPE BLOCK
        PACKAGE_NAME <null>
        ROUTINE_NAME <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE <null>
        PARENT_ROUTINE_NAME <null>
        SQL_TEXT
        execute block
        as
        begin
        execute procedure ins;
        delete from tab;
        end
        CURSOR_ID 1
        CURSOR_NAME <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID 2
        PARENT_RECORD_SOURCE_ID 1
        ACCESS_PATH 84:3
        -> Table "TAB" Full Scan
        OPEN_COUNTER 1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID 1
        STATEMENT_ID
        STATEMENT_TYPE BLOCK
        PACKAGE_NAME <null>
        ROUTINE_NAME <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE <null>
        PARENT_ROUTINE_NAME <null>
        SQL_TEXT
        execute block
        as
        begin
        execute procedure ins;
        delete from tab;
        end
        CURSOR_ID 1
        CURSOR_NAME <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID 1
        PARENT_RECORD_SOURCE_ID <null>
        ACCESS_PATH 84:2
        Select Expression (line 5, column 9)
        OPEN_COUNTER 1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID 2
        STATEMENT_ID
        STATEMENT_TYPE BLOCK
        PACKAGE_NAME <null>
        ROUTINE_NAME <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE <null>
        PARENT_ROUTINE_NAME <null>
        SQL_TEXT
        select rdb$profiler.start_session('profile session 2') from rdb$database
        CURSOR_ID 1
        CURSOR_NAME <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID 2
        PARENT_RECORD_SOURCE_ID 1
        ACCESS_PATH 84:5
        -> Table "RDB$DATABASE" Full Scan
        OPEN_COUNTER 0
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID 2
        STATEMENT_ID
        STATEMENT_TYPE BLOCK
        PACKAGE_NAME <null>
        ROUTINE_NAME <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE <null>
        PARENT_ROUTINE_NAME <null>
        SQL_TEXT
        select rdb$profiler.start_session('profile session 2') from rdb$database
        CURSOR_ID 1
        CURSOR_NAME <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID 1
        PARENT_RECORD_SOURCE_ID <null>
        ACCESS_PATH 84:4
        Select Expression
        OPEN_COUNTER 0
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID 2
        STATEMENT_ID
        STATEMENT_TYPE BLOCK
        PACKAGE_NAME <null>
        ROUTINE_NAME <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE <null>
        PARENT_ROUTINE_NAME <null>
        SQL_TEXT
        select mod(id, 5),
        sum(val)
        from tab
        where id <= 50
        group by mod(id, 5)
        order by sum(val)
        CURSOR_ID 1
        CURSOR_NAME <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID 2
        PARENT_RECORD_SOURCE_ID 1
        ACCESS_PATH 84:7
        -> Sort (record length: 44, key length: 12)
        OPEN_COUNTER 1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID 2
        STATEMENT_ID
        STATEMENT_TYPE BLOCK
        PACKAGE_NAME <null>
        ROUTINE_NAME <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE <null>
        PARENT_ROUTINE_NAME <null>
        SQL_TEXT
        select mod(id, 5),
        sum(val)
        from tab
        where id <= 50
        group by mod(id, 5)
        order by sum(val)
        CURSOR_ID 1
        CURSOR_NAME <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID 3
        PARENT_RECORD_SOURCE_ID 2
        ACCESS_PATH 84:8
        -> Aggregate
        OPEN_COUNTER 1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID 2
        STATEMENT_ID
        STATEMENT_TYPE BLOCK
        PACKAGE_NAME <null>
        ROUTINE_NAME <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE <null>
        PARENT_ROUTINE_NAME <null>
        SQL_TEXT
        select mod(id, 5),
        sum(val)
        from tab
        where id <= 50
        group by mod(id, 5)
        order by sum(val)
        CURSOR_ID 1
        CURSOR_NAME <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID 4
        PARENT_RECORD_SOURCE_ID 3
        ACCESS_PATH 84:9
        -> Sort (record length: 44, key length: 8)
        OPEN_COUNTER 1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID 2
        STATEMENT_ID
        STATEMENT_TYPE BLOCK
        PACKAGE_NAME <null>
        ROUTINE_NAME <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE <null>
        PARENT_ROUTINE_NAME <null>
        SQL_TEXT
        select mod(id, 5),
        sum(val)
        from tab
        where id <= 50
        group by mod(id, 5)
        order by sum(val)
        CURSOR_ID 1
        CURSOR_NAME <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID 5
        PARENT_RECORD_SOURCE_ID 4
        ACCESS_PATH 84:a
        -> Filter
        OPEN_COUNTER 1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID 2
        STATEMENT_ID
        STATEMENT_TYPE BLOCK
        PACKAGE_NAME <null>
        ROUTINE_NAME <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE <null>
        PARENT_ROUTINE_NAME <null>
        SQL_TEXT
        select mod(id, 5),
        sum(val)
        from tab
        where id <= 50
        group by mod(id, 5)
        order by sum(val)
        CURSOR_ID 1
        CURSOR_NAME <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID 6
        PARENT_RECORD_SOURCE_ID 5
        ACCESS_PATH 84:b
        -> Table "TAB" Full Scan
        OPEN_COUNTER 1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID 2
        STATEMENT_ID
        STATEMENT_TYPE BLOCK
        PACKAGE_NAME <null>
        ROUTINE_NAME <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE <null>
        PARENT_ROUTINE_NAME <null>
        SQL_TEXT
        select mod(id, 5),
        sum(val)
        from tab
        where id <= 50
        group by mod(id, 5)
        order by sum(val)
        CURSOR_ID 1
        CURSOR_NAME <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID 1
        PARENT_RECORD_SOURCE_ID <null>
        ACCESS_PATH 84:6
        Select Expression
        OPEN_COUNTER 1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        Records affected: 12
        MSG --- [ 4: plg$prof_requests ] ---
        PROFILE_ID 1
        REQUEST_ID
        STATEMENT_ID
        CALLER_REQUEST_ID
        START_TIMESTAMP
        FINISH_TIMESTAMP
        TOTAL_ELAPSED_TIME
        MSG --- [ 4: plg$prof_requests ] ---
        PROFILE_ID 1
        REQUEST_ID
        STATEMENT_ID
        CALLER_REQUEST_ID
        START_TIMESTAMP
        FINISH_TIMESTAMP
        TOTAL_ELAPSED_TIME
        MSG --- [ 4: plg$prof_requests ] ---
        PROFILE_ID 1
        REQUEST_ID
        STATEMENT_ID
        CALLER_REQUEST_ID
        START_TIMESTAMP
        FINISH_TIMESTAMP
        TOTAL_ELAPSED_TIME
        MSG --- [ 4: plg$prof_requests ] ---
        PROFILE_ID 1
        REQUEST_ID
        STATEMENT_ID
        CALLER_REQUEST_ID
        START_TIMESTAMP
        FINISH_TIMESTAMP
        TOTAL_ELAPSED_TIME
        MSG --- [ 4: plg$prof_requests ] ---
        PROFILE_ID 1
        REQUEST_ID
        STATEMENT_ID
        CALLER_REQUEST_ID
        START_TIMESTAMP
        FINISH_TIMESTAMP
        TOTAL_ELAPSED_TIME
        Records affected: 5
        MSG --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID 1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER 1
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        MSG --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID 1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER 1
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        MSG --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID 1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER 1
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        MSG --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID 1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER 1001
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        MSG --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID 1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER 1000
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        MSG --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID 1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER 500
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        MSG --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID 1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER 1000
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        MSG --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID 1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER 500
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        Records affected: 8
        MSG --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID 2
        REQUEST_ID
        CURSOR_ID 1
        RECORD_SOURCE_ID 1
        STATEMENT_ID
        OPEN_COUNTER 0
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID 2
        REQUEST_ID
        CURSOR_ID 1
        RECORD_SOURCE_ID 2
        STATEMENT_ID
        OPEN_COUNTER 0
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID 2
        REQUEST_ID
        CURSOR_ID 1
        RECORD_SOURCE_ID 1
        STATEMENT_ID
        OPEN_COUNTER 1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID 2
        REQUEST_ID
        CURSOR_ID 1
        RECORD_SOURCE_ID 2
        STATEMENT_ID
        OPEN_COUNTER 1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID 2
        REQUEST_ID
        CURSOR_ID 1
        RECORD_SOURCE_ID 3
        STATEMENT_ID
        OPEN_COUNTER 1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID 2
        REQUEST_ID
        CURSOR_ID 1
        RECORD_SOURCE_ID 4
        STATEMENT_ID
        OPEN_COUNTER 1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID 2
        REQUEST_ID
        CURSOR_ID 1
        RECORD_SOURCE_ID 5
        STATEMENT_ID
        OPEN_COUNTER 1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        MSG --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID 2
        REQUEST_ID
        CURSOR_ID 1
        RECORD_SOURCE_ID 6
        STATEMENT_ID
        OPEN_COUNTER 1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        Records affected: 8
    """

    fb6x_expected_out = """
        START_SESSION                   1
        START_SESSION                   2
        MSG                             --- [ 1: plg$prof_sessions ] ---
        PROFILE_ID                      1
        ATTACHMENT_ID
        USER_NAME                       SYSDBA
        DESCRIPTION                     profile session 1
        START_TIMESTAMP
        FINISH_TIMESTAMP
        MSG                             --- [ 1: plg$prof_sessions ] ---
        PROFILE_ID                      2
        ATTACHMENT_ID
        USER_NAME                       SYSDBA
        DESCRIPTION                     profile session 2
        START_TIMESTAMP
        FINISH_TIMESTAMP
        Records affected: 2
        MSG                             --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID                      1
        STATEMENT_TYPE                  BLOCK
        SQL_TEXT
        execute block
        as
        begin
        execute procedure ins;
        delete from tab;
        end
        LINE_NUM
        COLUMN_NUM
        COUNTER                         1
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        MSG                             --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID                      1
        STATEMENT_TYPE                  BLOCK
        SQL_TEXT
        execute block
        as
        begin
        execute procedure ins;
        delete from tab;
        end
        LINE_NUM
        COLUMN_NUM
        COUNTER                         1
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        MSG                             --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID                      1
        STATEMENT_TYPE                  PROCEDURE
        SQL_TEXT
        LINE_NUM
        COLUMN_NUM
        COUNTER                         1
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        MSG                             --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID                      1
        STATEMENT_TYPE                  PROCEDURE
        SQL_TEXT
        LINE_NUM
        COLUMN_NUM
        COUNTER                         1001
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        MSG                             --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID                      1
        STATEMENT_TYPE                  PROCEDURE
        SQL_TEXT
        LINE_NUM
        COLUMN_NUM
        COUNTER                         1000
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        MSG                             --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID                      1
        STATEMENT_TYPE                  PROCEDURE
        SQL_TEXT
        LINE_NUM
        COLUMN_NUM
        COUNTER                         500
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        MSG                             --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID                      1
        STATEMENT_TYPE                  PROCEDURE
        SQL_TEXT
        LINE_NUM
        COLUMN_NUM
        COUNTER                         1000
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        MSG                             --- [ 2: plg$prof_psql_stats_view ] ---
        PROFILE_ID                      1
        STATEMENT_TYPE                  FUNCTION
        SQL_TEXT
        LINE_NUM
        COLUMN_NUM
        COUNTER                         500
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        AVG_ELAPSED_TIME
        Records affected: 8
        MSG                             --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID                      1
        STATEMENT_ID
        STATEMENT_TYPE                  BLOCK
        PACKAGE_NAME                    <null>
        ROUTINE_NAME                    <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE           <null>
        PARENT_ROUTINE_NAME             <null>
        SQL_TEXT
        select rdb$profiler.start_session('profile session 1') from rdb$database
        CURSOR_ID                       1
        CURSOR_NAME                     <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID                2
        PARENT_RECORD_SOURCE_ID         1
        ACCESS_PATH                     84:1
        -> Table "SYSTEM"."RDB$DATABASE" Full Scan
        OPEN_COUNTER                    0
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID                      1
        STATEMENT_ID
        STATEMENT_TYPE                  BLOCK
        PACKAGE_NAME                    <null>
        ROUTINE_NAME                    <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE           <null>
        PARENT_ROUTINE_NAME             <null>
        SQL_TEXT
        select rdb$profiler.start_session('profile session 1') from rdb$database
        CURSOR_ID                       1
        CURSOR_NAME                     <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID                1
        PARENT_RECORD_SOURCE_ID         <null>
        ACCESS_PATH                     84:0
        Select Expression
        OPEN_COUNTER                    0
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID                      1
        STATEMENT_ID
        STATEMENT_TYPE                  BLOCK
        PACKAGE_NAME                    <null>
        ROUTINE_NAME                    <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE           <null>
        PARENT_ROUTINE_NAME             <null>
        SQL_TEXT
        execute block
        as
        begin
        execute procedure ins;
        delete from tab;
        end
        CURSOR_ID                       1
        CURSOR_NAME                     <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID                2
        PARENT_RECORD_SOURCE_ID         1
        ACCESS_PATH                     84:3
        -> Table "PUBLIC"."TAB" Full Scan
        OPEN_COUNTER                    1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID                      1
        STATEMENT_ID
        STATEMENT_TYPE                  BLOCK
        PACKAGE_NAME                    <null>
        ROUTINE_NAME                    <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE           <null>
        PARENT_ROUTINE_NAME             <null>
        SQL_TEXT
        execute block
        as
        begin
        execute procedure ins;
        delete from tab;
        end
        CURSOR_ID                       1
        CURSOR_NAME                     <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID                1
        PARENT_RECORD_SOURCE_ID         <null>
        ACCESS_PATH                     84:2
        Select Expression (line 5, column 13)
        OPEN_COUNTER                    1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID                      2
        STATEMENT_ID
        STATEMENT_TYPE                  BLOCK
        PACKAGE_NAME                    <null>
        ROUTINE_NAME                    <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE           <null>
        PARENT_ROUTINE_NAME             <null>
        SQL_TEXT
        select rdb$profiler.start_session('profile session 2') from rdb$database
        CURSOR_ID                       1
        CURSOR_NAME                     <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID                2
        PARENT_RECORD_SOURCE_ID         1
        ACCESS_PATH                     84:5
        -> Table "SYSTEM"."RDB$DATABASE" Full Scan
        OPEN_COUNTER                    0
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID                      2
        STATEMENT_ID
        STATEMENT_TYPE                  BLOCK
        PACKAGE_NAME                    <null>
        ROUTINE_NAME                    <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE           <null>
        PARENT_ROUTINE_NAME             <null>
        SQL_TEXT
        select rdb$profiler.start_session('profile session 2') from rdb$database
        CURSOR_ID                       1
        CURSOR_NAME                     <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID                1
        PARENT_RECORD_SOURCE_ID         <null>
        ACCESS_PATH                     84:4
        Select Expression
        OPEN_COUNTER                    0
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID                      2
        STATEMENT_ID
        STATEMENT_TYPE                  BLOCK
        PACKAGE_NAME                    <null>
        ROUTINE_NAME                    <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE           <null>
        PARENT_ROUTINE_NAME             <null>
        SQL_TEXT
        select mod(id, 5),
        sum(val)
        from tab
        where id <= 50
        group by mod(id, 5)
        order by sum(val)
        CURSOR_ID                       1
        CURSOR_NAME                     <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID                2
        PARENT_RECORD_SOURCE_ID         1
        ACCESS_PATH                     84:7
        -> Sort (record length: 44, key length: 12)
        OPEN_COUNTER                    1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID                      2
        STATEMENT_ID
        STATEMENT_TYPE                  BLOCK
        PACKAGE_NAME                    <null>
        ROUTINE_NAME                    <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE           <null>
        PARENT_ROUTINE_NAME             <null>
        SQL_TEXT
        select mod(id, 5),
        sum(val)
        from tab
        where id <= 50
        group by mod(id, 5)
        order by sum(val)
        CURSOR_ID                       1
        CURSOR_NAME                     <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID                3
        PARENT_RECORD_SOURCE_ID         2
        ACCESS_PATH                     84:8
        -> Aggregate
        OPEN_COUNTER                    1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID                      2
        STATEMENT_ID
        STATEMENT_TYPE                  BLOCK
        PACKAGE_NAME                    <null>
        ROUTINE_NAME                    <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE           <null>
        PARENT_ROUTINE_NAME             <null>
        SQL_TEXT
        select mod(id, 5),
        sum(val)
        from tab
        where id <= 50
        group by mod(id, 5)
        order by sum(val)
        CURSOR_ID                       1
        CURSOR_NAME                     <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID                4
        PARENT_RECORD_SOURCE_ID         3
        ACCESS_PATH                     84:9
        -> Sort (record length: 44, key length: 8)
        OPEN_COUNTER                    1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID                      2
        STATEMENT_ID
        STATEMENT_TYPE                  BLOCK
        PACKAGE_NAME                    <null>
        ROUTINE_NAME                    <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE           <null>
        PARENT_ROUTINE_NAME             <null>
        SQL_TEXT
        select mod(id, 5),
        sum(val)
        from tab
        where id <= 50
        group by mod(id, 5)
        order by sum(val)
        CURSOR_ID                       1
        CURSOR_NAME                     <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID                5
        PARENT_RECORD_SOURCE_ID         4
        ACCESS_PATH                     84:a
        -> Filter
        OPEN_COUNTER                    1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID                      2
        STATEMENT_ID
        STATEMENT_TYPE                  BLOCK
        PACKAGE_NAME                    <null>
        ROUTINE_NAME                    <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE           <null>
        PARENT_ROUTINE_NAME             <null>
        SQL_TEXT
        select mod(id, 5),
        sum(val)
        from tab
        where id <= 50
        group by mod(id, 5)
        order by sum(val)
        CURSOR_ID                       1
        CURSOR_NAME                     <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID                6
        PARENT_RECORD_SOURCE_ID         5
        ACCESS_PATH                     84:b
        -> Table "PUBLIC"."TAB" Full Scan
        OPEN_COUNTER                    1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 3: plg$prof_record_source_stats_view ] ---
        PROFILE_ID                      2
        STATEMENT_ID
        STATEMENT_TYPE                  BLOCK
        PACKAGE_NAME                    <null>
        ROUTINE_NAME                    <null>
        PARENT_STATEMENT_ID
        PARENT_STATEMENT_TYPE           <null>
        PARENT_ROUTINE_NAME             <null>
        SQL_TEXT
        select mod(id, 5),
        sum(val)
        from tab
        where id <= 50
        group by mod(id, 5)
        order by sum(val)
        CURSOR_ID                       1
        CURSOR_NAME                     <null>
        CURSOR_LINE_NUM
        CURSOR_COLUMN_NUM
        RECORD_SOURCE_ID                1
        PARENT_RECORD_SOURCE_ID         <null>
        ACCESS_PATH                     84:6
        Select Expression
        OPEN_COUNTER                    1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        OPEN_AVG_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        FETCH_AVG_ELAPSED_TIME
        OPEN_FETCH_TOTAL_ELAPSED_TIME
        Records affected: 12
        MSG                             --- [ 4: plg$prof_requests ] ---
        PROFILE_ID                      1
        REQUEST_ID
        STATEMENT_ID
        CALLER_REQUEST_ID
        START_TIMESTAMP
        FINISH_TIMESTAMP
        TOTAL_ELAPSED_TIME
        MSG                             --- [ 4: plg$prof_requests ] ---
        PROFILE_ID                      1
        REQUEST_ID
        STATEMENT_ID
        CALLER_REQUEST_ID
        START_TIMESTAMP
        FINISH_TIMESTAMP
        TOTAL_ELAPSED_TIME
        MSG                             --- [ 4: plg$prof_requests ] ---
        PROFILE_ID                      1
        REQUEST_ID
        STATEMENT_ID
        CALLER_REQUEST_ID
        START_TIMESTAMP
        FINISH_TIMESTAMP
        TOTAL_ELAPSED_TIME
        MSG                             --- [ 4: plg$prof_requests ] ---
        PROFILE_ID                      1
        REQUEST_ID
        STATEMENT_ID
        CALLER_REQUEST_ID
        START_TIMESTAMP
        FINISH_TIMESTAMP
        TOTAL_ELAPSED_TIME
        Records affected: 4
        MSG                             --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID                      1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER                         1
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        MSG                             --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID                      1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER                         1
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        MSG                             --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID                      1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER                         1
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        MSG                             --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID                      1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER                         1001
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        MSG                             --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID                      1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER                         1000
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        MSG                             --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID                      1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER                         500
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        MSG                             --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID                      1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER                         1000
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        MSG                             --- [ 5: plg$prof_psql_stats join plg$prof_sessions ] ---
        PROFILE_ID                      1
        REQUEST_ID
        LINE_NUM
        COLUMN_NUM
        STATEMENT_ID
        COUNTER                         500
        MIN_ELAPSED_TIME
        MAX_ELAPSED_TIME
        TOTAL_ELAPSED_TIME
        Records affected: 8
        MSG                             --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID                      2
        REQUEST_ID
        CURSOR_ID                       1
        RECORD_SOURCE_ID                1
        STATEMENT_ID
        OPEN_COUNTER                    0
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID                      2
        REQUEST_ID
        CURSOR_ID                       1
        RECORD_SOURCE_ID                2
        STATEMENT_ID
        OPEN_COUNTER                    0
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID                      2
        REQUEST_ID
        CURSOR_ID                       1
        RECORD_SOURCE_ID                1
        STATEMENT_ID
        OPEN_COUNTER                    1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID                      2
        REQUEST_ID
        CURSOR_ID                       1
        RECORD_SOURCE_ID                2
        STATEMENT_ID
        OPEN_COUNTER                    1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID                      2
        REQUEST_ID
        CURSOR_ID                       1
        RECORD_SOURCE_ID                3
        STATEMENT_ID
        OPEN_COUNTER                    1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID                      2
        REQUEST_ID
        CURSOR_ID                       1
        RECORD_SOURCE_ID                4
        STATEMENT_ID
        OPEN_COUNTER                    1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID                      2
        REQUEST_ID
        CURSOR_ID                       1
        RECORD_SOURCE_ID                5
        STATEMENT_ID
        OPEN_COUNTER                    1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        MSG                             --- [ 6: plg$prof_record_source_stats ] ---
        PROFILE_ID                      2
        REQUEST_ID
        CURSOR_ID                       1
        RECORD_SOURCE_ID                6
        STATEMENT_ID
        OPEN_COUNTER                    1
        OPEN_MIN_ELAPSED_TIME
        OPEN_MAX_ELAPSED_TIME
        OPEN_TOTAL_ELAPSED_TIME
        FETCH_COUNTER
        FETCH_MIN_ELAPSED_TIME
        FETCH_MAX_ELAPSED_TIME
        FETCH_TOTAL_ELAPSED_TIME
        Records affected: 8
    """

    act.expected_stdout = fb5x_expected_out if act.is_version('<6') else fb6x_expected_out
    act.isql(switches = ['-q'], input = test_script, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
