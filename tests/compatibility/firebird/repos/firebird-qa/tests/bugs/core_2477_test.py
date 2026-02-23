#coding:utf-8

"""
ID:          issue-2890
ISSUE:       2890
TITLE:       mon$memory_usage: Sorting memory should be reported as owned by the statement
DESCRIPTION:
    We open cursor that executes statement that requires to sort 10'000 rows of 32K width.
    Cursor fetches only half of this rows number and breaks from loop (but we do not close this cursor).
    This causes huge memory consumation and we can compare value in mon$memory_used that was before
    this query start.
    In order to check ticket issue we filer mon$ data by criteria 'where m.mon$stat_group = 3' (i.e. STATEMENT level)
    and also we have to check memory usage only for statement that we are running. Because of this, we add special
    tag in the SQL query '/* HEAVY_SORT_TAG */' - and search for that.

    Runs this test on firebird.conf with default TempCacheLimit show following values of differences:
      * for SuperServer: ~68.1 Mb; 
      * for Classic: ~9.4 Mb
    Because of this, difference of mon$memory_used is compared with DIFFERENT threshold depending on
    FB ServerMode (see act.vars['server-arch']).
JIRA:        CORE-2477
FBTEST:      bugs.core_2477
NOTES:
    [16.11.2021] pcisar
    This test is too complicated and fragile, and it's IMHO not worth to be implemented

    [24.07.2022] pzotov
    Test was totally re-implemented. No async call of ISQL, waiting/killing etc.

    Checked on Windows: 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.591
    Checked on Linux: 4.0.1.2692 (SS/CS) - needed to increase number of rows to be sorted.

    [18.01.2025] pzotov
    Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
    in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
    Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
    This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
    The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
"""

import subprocess
from pathlib import Path
from firebird.driver import DatabaseError
import time

import pytest
from firebird.qa import *

init_script = """
    create or alter view v_mon as
    select *
    from (
        select
            m.mon$stat_group as stat_gr
           ,rpad( decode(m.mon$stat_group, 0,'0:database', 1,'1:attachment', 2,'2:transaction', 3,'3:statement', 4,'4:call'), 15,' ') as stat_type
           ,m.mon$memory_used          as memo_used
           ,m.mon$memory_allocated     as memo_allo
           ,m.mon$max_memory_used      as max_memo_used
           ,m.mon$max_memory_allocated as max_memo_allo
           ,m.mon$stat_id              as stat_id
           ,coalesce( s.mon$attachment_id, t.mon$attachment_id, a.mon$attachment_id, -999 ) as att_id
           ,coalesce( s.mon$transaction_id, t.mon$transaction_id, -999 ) as trn_id
           ,coalesce( s.mon$statement_id, -999) as sttm_id
           ,coalesce( decode( s.mon$state, 0,'idle', 1,'running', 2,'stalled' ), 'n/a') as stm_state
           ,lower(right( coalesce(trim(coalesce(a.mon$remote_process, a.mon$user)), ''), 20 )) as att_process -- isql.exe or Garbage Collector or Cache Writer
           ,lower(left( coalesce(cast(s.mon$sql_text as varchar(8100)),''), 128 )) as sql_text
        from mon$memory_usage m
        left join mon$statements s on m.mon$stat_group = 3 and m.mon$stat_id = s.mon$stat_id
        left join mon$transactions t on
            m.mon$stat_group = 2 and m.mon$stat_id = t.mon$stat_id
            or m.mon$stat_group = 3 and m.mon$stat_id = s.mon$stat_id and t.mon$transaction_id = s.mon$transaction_id
        left join mon$attachments a on
            m.mon$stat_group = 1 and m.mon$stat_id = a.mon$stat_id
            or m.mon$stat_group=2 and m.mon$stat_id = t.mon$stat_id and a.mon$attachment_id = t.mon$attachment_id
            or m.mon$stat_group=3 and m.mon$stat_id = s.mon$stat_id and a.mon$attachment_id = s.mon$attachment_id

    ) t
    where t.stat_gr = 3 and sql_text containing 'HEAVY_SORT_TAG'
    order by stat_type, stat_id
    ;

    create or alter view v_gather_mon as
     select
         v.att_process
        ,replace(replace(replace(replace(v.sql_text, ascii_char(10),' '), ascii_char(13),' '),'   ',' '),'  ',' ') as sql_text
        ,v.stat_type
        ,v.stm_state
        ,v.att_id
        ,v.trn_id
        ,v.sttm_id
        ,v.memo_used
        ,v.memo_allo
     from v_mon v
     where v.att_id is distinct from current_connection
    ;
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

def result_msg(a_diff_value, a_min_threshold):
    return ( ('OK, expected: increased significantly.') if a_diff_value > a_min_threshold else ('BAD! Did not increased as expected. Difference: ' + "{:d}".format(a_diff_value)+'.') )

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    ##################
    ROWS_TO_BE_SORTED = 10000
    MIN_DIFF_THRESHOLD = 10000000 if 'classic' in act.vars['server-arch'].lower() else 60000000
    ##################
    
    heavy_sql_sttm = "with a as (select 1 i from rdb$types rows 200) select /* HEAVY_SORT_TAG */ distinct lpad('', 32500, uuid_to_char(gen_uuid())) s from a x, a y"
    #heavy_sql_sttm = "select /* HEAVY_SORT_TAG */ guid_str from sp_produce_guid(5000) order by 1"
    
    map_result = {}
    with act.db.connect() as con_worker:

        cur_worker = con_worker.cursor()
        cur_wrk_ps, cur_wrk_rs = None, None

        try:
            cur_wrk_ps = cur_worker.prepare(heavy_sql_sttm)

            for m in ('beg','end'):
                with act.db.connect() as con_monitor:
                    cur_monitor=con_monitor.cursor()
                    cur_monitor.execute('select * from v_gather_mon')
                    
                    for r in cur_monitor:
                        map_result[m] = (r[3], r[7]) # ('idle' | 'stalled', memo_used)

                    assert map_result.get(m), f"No rows returned from v_gather_mon for m='{m}'"
           
                if m == 'beg':

                    # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
                    # We have to store result of cur.execute(<psInstance>) in order to
                    # close it explicitly.
                    # Otherwise AV can occur during Python garbage collection and this
                    # causes pytest to hang on its final point.
                    # Explained by hvlad, email 26.10.24 17:42
                    
                    cur_wrk_rs = cur_worker.execute(cur_wrk_ps)
                    for i in range(0, ROWS_TO_BE_SORTED):
                        r = cur_worker.fetchone()

                    # After this loop statement with huge sort will remain in stalled state
                    # (its mon$statements.mon$state must be 2).
                    # We can now gather mon$ info second time (in a NEW connection)
                    # and then evaluate DIFFERENCE.

        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if cur_wrk_rs:
                cur_wrk_rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
            if cur_wrk_ps:
                cur_wrk_ps.free()

    #------------------------------------------------------------------------------------------

    assert map_result['beg'][0].strip() == 'idle' and map_result['end'][0].strip() == 'stalled'

    expected_msg = 'DELTA of mon$memory_used increased significantly.'
    diff = map_result['end'][1] - map_result['beg'][1]

    if diff > MIN_DIFF_THRESHOLD:
        print(expected_msg)
    else:
        print('FAIL on %s, MON$MEMORY_USED increased for less than MIN_DIFF_THRESHOLD = %d' % (act.vars['server-arch'], MIN_DIFF_THRESHOLD) )
        for k,v in sorted(map_result.items()):
           print(k,':',v)
        print('diff = %d' % diff)


    act.expected_stdout = expected_msg
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
