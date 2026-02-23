#coding:utf-8

"""
ID:          issue-4310
ISSUE:       4310
TITLE:       DELETE FROM MON$STATEMENTS does not interrupt a longish fetch
DESCRIPTION:
JIRA:        CORE-3977
FBTEST:      bugs.core_3977
NOTES:
    [12.12.2023] pzotov
    1. Re-implemented using code that does not operate with execute statement on external data source:
       we use asynchronous launch of ISQL with redirecting data to OS null device and check only STDERR
       content after deletion record in MON$STATEMENTS (content of ISQL log which did 'longish fetch'
       *must* contain text about interrupting of query, its SQLSTATE = HY008).
       Removed substitutions.

    2. Removed 'naive method' for waiting until ISQL process started its work (used 'time.sleep(...)').
       Intead, we have to use loop which does query to mon$ statements and looks there for record which
       mon$sql_text contains some KNOWN phrase - see 'HEAVY_TAG'. See function check_mon_for_pid_appearance()

    Checked on 3.0.12.33725, 4.0.5.3040, 5.0.0.1294, 6.0.0.172 

    [18.01.2025] pzotov
    Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
    in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
    Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
    This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
    The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
"""

import os
import subprocess
from datetime import datetime as dt
import time
from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation, TraLockResolution, DatabaseError

MAX_WAIT_FOR_ISQL_PID_APPEARS_MS = 10000
HEAVY_TAG = '/* HEAVY_TAG */'

init_script = """
    create sequence g;
    commit;
"""

db = db_factory(init=init_script)
act = python_act('db')

heavy_sql = temp_file('work_script.sql')
heavy_log = temp_file('work_script.log')

#---------------------------------------------------------

def check_mon_for_pid_appearance(act: Action, p_async_launched: subprocess.CompletedProcess, HEAVY_TAG: str, MAX_WAIT_FOR_ISQL_PID_APPEARS_MS: int):

    chk_mon_sql = """
        select 1
        from mon$attachments a
        join mon$statements s
        using (mon$attachment_id)
        where
            a.mon$attachment_id <> current_connection
            and a.mon$remote_pid = ?
            and s.mon$sql_text containing ?
    """

    found_in_mon_tables = False
    with act.db.connect() as con_watcher:

        ps, rs = None, None
        try:
            custom_tpb = tpb(isolation = Isolation.SNAPSHOT, lock_timeout = -1)
            tx_watcher = con_watcher.transaction_manager(custom_tpb)
            cur_watcher = tx_watcher.cursor()

            ps = cur_watcher.prepare(chk_mon_sql)

            i = 0
            da = dt.now()
            while True:
                mon_result = -1

                # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
                # We have to store result of cur.execute(<psInstance>) in order to
                # close it explicitly.
                # Otherwise AV can occur during Python garbage collection and this
                # causes pytest to hang on its final point.
                # Explained by hvlad, email 26.10.24 17:42
                rs = cur_watcher.execute(ps, (p_async_launched.pid, HEAVY_TAG,) )
                for r in rs:
                    mon_result = r[0]

                tx_watcher.commit()
                db = dt.now()
                diff_ms = (db-da).seconds*1000 + (db-da).microseconds//1000
                if mon_result == 1:
                    found_in_mon_tables = True
                    break
                elif diff_ms > MAX_WAIT_FOR_ISQL_PID_APPEARS_MS:
                    break
                time.sleep(0.1)

        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if rs:
                rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
            if ps:
                ps.free()



    assert found_in_mon_tables, f'Could not find attachment in mon$ tables for {MAX_WAIT_FOR_ISQL_PID_APPEARS_MS} ms.'

#---------------------------------------------------------

@pytest.mark.version('>=3')
def test_1(act: Action, heavy_sql: Path, heavy_log: Path, capsys):
    longish_fetch = f"""
        out {os.devnull};
        set heading off;
        select {HEAVY_TAG} 'SEQ_VALUE_' || gen_id(g,1) from rdb$types,rdb$types,rdb$types,rdb$types,rdb$types;
        out;
    """
    heavy_sql.write_text(longish_fetch)

    with open(heavy_log, 'w') as f:
        # Starting ISQL in separate process with doing 'heavy query'
        p_work_sql = subprocess.Popen( [ act.vars['isql'], '-i', str(heavy_sql),
                                         '-user', act.db.user,
                                         '-password', act.db.password,
                                         act.db.dsn
                                       ],
                                       stderr = f
                                     )

    # Wait for ISQL appear in MON$ tables:
    ######################################
    check_mon_for_pid_appearance(act, p_work_sql, HEAVY_TAG, MAX_WAIT_FOR_ISQL_PID_APPEARS_MS)

    # Run 2nd isql and issue there DELETE FROM MON$ATATSMENTS command. First ISQL process should be terminated for short time.
    drop_sql = f"""
        set bail on;
        set list on;
        set count on;
        commit;
        set term ^;
        execute block returns(kill_sttm_outcome varchar(255)) as
            declare v_heavy_conn type of column mon$statements.mon$attachment_id;
            declare v_heavy_pid type of column mon$attachments.mon$remote_pid;
        begin
            delete from mon$statements
            where
                mon$attachment_id != current_connection
                and mon$sql_text containing '{HEAVY_TAG}'
            returning mon$attachment_id into v_heavy_conn
            ;

            select mon$remote_pid
            from mon$attachments a
            where mon$attachment_id = :v_heavy_conn
            into v_heavy_pid;
            if (v_heavy_pid = {p_work_sql.pid}) then
                kill_sttm_outcome = 'OK';
            else
                kill_sttm_outcome = 'UNEXPECTED: v_heavy_conn=' || coalesce(v_heavy_conn, '[null]') || ', v_heavy_pid=' || coalesce(v_heavy_pid, '[null]') || ', p_work_sql.pid=' || {p_work_sql.pid}
                ;
            suspend;
        end
        ^
        set term ;^
        exit;
    """

    try:
        act.isql(switches=[], input=drop_sql, combine_output = True)
        delete_from_mon_sttm_log = act.clean_string(act.stdout)
        ##################################
        # Result: <heavy_log> must contain:
        # Statement failed, SQLSTATE = HY008
        # operation was cancelled
    finally:
        p_work_sql.terminate()

    # Run checking query: what is resuling value of sequence 'g' ?
    # (it must be > 0 and < total number of records to be handled).
    check_sql = """
        --set echo on;
        set list on;
        set count on;
        select iif( current_gen > 0 and current_gen < total_rows,
                    'OK: query was interrupted in the middle point.',
                    'WRONG! Query to be interrupted '
                    || iif(current_gen <= 0, 'did not start.', 'already gone, current_gen = '||current_gen )
                  ) as result_msg
        from (
            select gen_id(g,0) as current_gen, c.n * c.n * c.n * c.n * c.n as total_rows
            from (select (select count(*) from rdb$types) as n from rdb$database) c
        );
    """
    act.isql(switches=[], input=check_sql, combine_output = True)

    with open(heavy_log, 'r') as f:
        for line in f:
            if not 'line' in line:
                print('LONGISH FETCH LOG:',' '.join(line.upper().split()))

    for line in delete_from_mon_sttm_log.splitlines():
        print('DEL FROM MON$STTM: ', ' '.join(line.upper().split()))

    for line in act.clean_string(act.stdout).splitlines():
        print('CHECK RESULTS LOG: ', ' '.join(line.upper().split()))


    expected_stdout = """
        LONGISH FETCH LOG: STATEMENT FAILED, SQLSTATE = HY008
        LONGISH FETCH LOG: OPERATION WAS CANCELLED

        DEL FROM MON$STTM:  KILL_STTM_OUTCOME OK
        DEL FROM MON$STTM:  RECORDS AFFECTED: 1

        CHECK RESULTS LOG:  RESULT_MSG OK: QUERY WAS INTERRUPTED IN THE MIDDLE POINT.
        CHECK RESULTS LOG:  RECORDS AFFECTED: 1
    """
    
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
