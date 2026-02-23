#coding:utf-8

"""
ID:          replication.test_make_async_reinit_reliable
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8324
TITLE:       Make asynchronous replica re-initialization reliable #8324
DESCRIPTION:
    To reproduce problem from ticket one need to do following:
    * create two tables ('test' and 'test2'), one of them must have quite long text column;
    * add about one-two hundred rows in the 'test', see var. ADD_ROWS_INIT;
    * start Tx on master (it will be OAT, see var. 'tx_oat') and:
        ** add several thousands rows in the table 'test' (see var. ADD_ROWS_IN_OAT), but do NOT commit Tx;
        ** stay idle (wait) until message with phrase "Segment... preserving (OAT: ... in segment ...)"
           will appear in the replication log 
    * start one more Tx on master (see var 'tx_bef') and run some DML within this Tx against table 'TEST2',
      with obtaining value of rdb$get_context('SYSTEM', 'REPLICATION_SEQUENCE') and store it for further
      check (see var 'preserved_segment_no'). Commit this Tx.
      Note: one need to use DIFFERENT table here. Do not use 'TEST'.
    * Stay idle (wait) until replica becomes actual with master, i.e. table 'TEST2' will have same value
      that was just used on master (see 'select count(*) from test2 where id = {TEST2_VALUE}')
    * Change state of replica DB to full shutdown (in order to have ability to overwrite it further).
    * Run on master: 'nacbkup -b 0' (see var. 'db_nbk0'). NOTE: this action increases ID of segment that
      will be used for further writes. It is NOT so for command 'NBACKUP -L'.
    * Handle just created .nbk0: run 'nbackup -SEQ -F', then set is as REPLICA and put fo FULL SHUTDOWN.
    * Overwrite "old" replica with just created copy of master that has been prepared to serve as replica.
    * Bring replica online; replication log will have following messages at this point:
      BEFORE fix:
          VERBOSE: Segment 2 (15729380 bytes) is replicated in 1.296s, preserving (OAT: 13 in segment 2)
          VERBOSE: Segment 3 (646 bytes) is replicated in 0.013s, preserving (OAT: 13 in segment 2)
          VERBOSE: Added 2 segment(s) to the queue
          VERBOSE: Deleting segment 2 due to fast forward
          VERBOSE: Deleting segment 3 due to fast forward
      AFTER fix:
          VERBOSE: Segment 2 (15729380 bytes) is replicated in 0.944s, preserving (OAT: 14 in segment 2)
          VERBOSE: Added 1 segment(s) to the queue
          VERBOSE: Added 2 segment(s) to the queue
          VERBOSE: Segment 2 (15729380 bytes) is replayed in 1.438s, preserving (OAT: 14 in segment 2)
          VERBOSE: Segment 3 (646 bytes) is replicated in 0.009s, preserving (OAT: 14 in segment 2)
          VERBOSE: Added 2 segment(s) to the queue
          ERROR: database <DB_REPL> shutdown
          VERBOSE: Database sequence has been changed to 3, preparing for replication reset
    * Master DB: do COMMIT for tx_oat. After this replication log following messages:
      BEFORE fix:
          VERBOSE: Added 1 segment(s) to the queue
          VERBOSE: Resetting replication to continue from segment 4
          ERROR: Transaction 13 is not found
          At segment 4, offset 48
      AFTER fix:
          VERBOSE: Database sequence has been changed to 3, preparing for replication reset
          VERBOSE: Added 2 segment(s) to the queue
          VERBOSE: Database sequence has been changed to 3, preparing for replication reset
          VERBOSE: Added 3 segment(s) to the queue
          VERBOSE: Database sequence has been changed to 3, preparing for replication reset
          VERBOSE: Segment 2 (15729380 bytes) is scanned in 0.006s, preserving (OAT: 14 in segment 2)
          VERBOSE: Segment 3 (646 bytes) is scanned in 0.014s, preserving (OAT: 14 in segment 2)
          VERBOSE: Resetting replication to continue from segment 4 (new OAT: 14 in segment 2)
          VERBOSE: Segment 2 (15729380 bytes) is replayed in 1.343s, preserving (OAT: 14 in segment 2)
          VERBOSE: Segment 3 (646 bytes) is replayed in 0.014s, preserving (OAT: 14 in segment 2)
          VERBOSE: Segment 4 (321088 bytes) is replicated in 0.105s, deleting
          VERBOSE: Deleting segment 2 as no longer needed
          VERBOSE: Deleting segment 3 as no longer needed
    * Check replication log that there is NO error message with text 'Transaction <N> is not found'
    * Stay idle (wait) until replica becomes actual with master, i.e. query 'select max(id) from test'
      being issued on replica will return expected value = ADD_ROWS_INIT + ADD_ROWS_IN_OAT.

NOTES:
    [10.12.2024] pzotov
    Confirmed bug on 5.0.2.1567-9fbd574 (16.11.2024 20:15).
    Checked on Windows:
        5.0.2.1569-684bb87 (27.11.2024 20:40).
        4.0.6.3170-cc44002 (10.12.2024 07:02)
        6.0.0.548-a8c5b9f  (10.12.2024 10:13)
    Great thanks to dimitr for suggestions about test implementation.
"""
import os
import shutil
import re
import locale
from difflib import unified_diff
from pathlib import Path
import datetime
import time

import pytest
from firebird.qa import *
from firebird.driver import * 

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
repl_settings = QA_GLOBALS['replication']

MAX_TIME_FOR_WAIT_SEGMENT_IN_LOG = int(repl_settings['max_time_for_wait_segment_in_log'])
MAX_TIME_FOR_WAIT_DATA_IN_REPLICA = int(repl_settings['max_time_for_wait_data_in_replica'])

MAIN_DB_ALIAS = repl_settings['main_db_alias']
REPL_DB_ALIAS = repl_settings['repl_db_alias']
RUN_SWEEP_AT_END = int(repl_settings['run_sweep_at_end'])

db_main = db_factory( filename = '#' + MAIN_DB_ALIAS, do_not_create = True, do_not_drop = True)
db_repl = db_factory( filename = '#' + REPL_DB_ALIAS, do_not_create = True, do_not_drop = True)

# Where we want to store result of 'nbackup -b 0 <db_master>':
db_nbk0 = temp_file('gh_8324_tmp.nbk0')

substitutions = [('Start removing objects in:.*', 'Start removing objects'),
                 ('Finish. Total objects removed:  [1-9]\\d*', 'Finish. Total objects removed'),
                 ('.* CREATE DATABASE .*', ''),
                 ('[\t ]+', ' '),
                 ('FOUND message about replicated segment N .*', 'FOUND message about replicated segment')]

act_db_main = python_act('db_main', substitutions=substitutions)
act_db_repl = python_act('db_repl', substitutions=substitutions)

#--------------------------------------------

def cleanup_folder(p):
    # Removed all files and subdirs in the folder <p>
    # Used for cleanup <repl_journal> and <repl_archive> when replication must be reset
    # in case when any error occurred during test execution.
    assert os.path.dirname(p) != p, f"@@@ ABEND @@@ CAN NOT operate in the file system root directory. Check your code!"

    for root, dirs, files in os.walk(p):
        for f in files:
            # ::: NB ::: 22.12.2023.
            # We have to expect that attempt to delete of GUID and (maybe) archived segments can FAIL with
            # PermissionError: [WinError 32] The process cannot ... used by another process: /path/to/{GUID}
            # Also, we have to skip exception if file (segment) was just deleted by engine
            try:
                Path(root +'/' + f).unlink(missing_ok = True)
            except PermissionError as x:
                pass

        for d in dirs:
            shutil.rmtree(os.path.join(root, d), ignore_errors = True)

    return os.listdir(p)

#--------------------------------------------

def reset_replication(act_db_main, act_db_repl, db_main_file, db_repl_file):
    out_reset = ''
    failed_shutdown_db_map = {} # K = 'db_main', 'db_repl'; V = error that occurred when we attempted to change DB state to full shutdown (if it occurred)

    with act_db_main.connect_server() as srv:

        # !! IT IS ASSUMED THAT REPLICATION FOLDERS ARE IN THE SAME DIR AS <DB_MAIN> !!
        # DO NOT use 'a.db.db_path' for ALIASED database!
        # It will return '.' rather than full path+filename.

        repl_root_path = Path(db_main_file).parent
        repl_jrn_sub_dir = Path(db_main_file).with_suffix('.journal')
        repl_arc_sub_dir = Path(db_main_file).with_suffix('.archive')

        for f in (db_main_file, db_repl_file):
            # Method db.drop() changes LINGER to 0, issues 'delete from mon$att' with suppressing exceptions
            # and calls 'db.drop_database()' (also with suppressing exceptions).
            # We change DB state to FULL SHUTDOWN instead of call action.db.drop() because
            # this is more reliable (it kills all attachments in all known cases and does not use mon$ table)
            #
            try:
                srv.database.shutdown(database = f, mode = ShutdownMode.FULL, method = ShutdownMethod.FORCED, timeout = 0)

                # REMOVE db file from disk: we can safely assume that this can be done because DB in full shutdown state.
                ###########################
                os.unlink(f)
            except DatabaseError as e:
                failed_shutdown_db_map[ f ] = e.__str__()


        # Clean folders repl_journal and repl_archive: remove all files from there.
        # NOTE: test must NOT raise unrecoverable error if some of files in these folders can not be deleted.
        # Rather, this must be displayed as diff and test must be considered as just failed.
        for p in (repl_jrn_sub_dir,repl_arc_sub_dir):
            remained_files = cleanup_folder(p)
            if remained_files:
                out_reset += '\n'.join( (f"Directory '{str(p)}' remains non-empty. Could not delete file(s):", '\n'.join(remained_files)) )

    # xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    # xxx  r e c r e a t e     d b _ m a i n     a n d     d b _ r e p l  xxx
    # xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    for a in (act_db_main,act_db_repl):
        d = a.db.db_path
        failed_shutdown_msg = failed_shutdown_db_map.get( str(d), '' )
        if failed_shutdown_msg:
            # we could NOT change state of this database to full shutdown --> we must NOT recreate it.
            # Accumulate error messages in OUT arg (for displaying as diff):
            #
            out_reset += '\n'.join( failed_shutdown_msg )
        else:
            try:
                dbx = create_database(str(d), user = a.db.user)
                dbx.close()
                with a.connect_server() as srv:
                    srv.database.set_write_mode(database = d, mode = DbWriteMode.ASYNC)
                    srv.database.set_sweep_interval(database = d, interval = 0)
                    if a == act_db_repl:
                        srv.database.set_replica_mode(database = d, mode = ReplicaMode.READ_ONLY)
                    else:
                        with a.db.connect() as con:
                            con.execute_immediate('alter database enable publication')
                            con.execute_immediate('alter database include all to publication')
                            con.commit()
            except DatabaseError as e:
                out_reset += e.__str__()
        
    # Must remain EMPTY:
    ####################
    return out_reset

#--------------------------------------------

def watch_repl_log_pattern( act_db_main: Action, pattern_to_check, replold_lines, max_allowed_time_for_wait, consider_found_as_unexpected = False):

    replication_log = act_db_main.home_dir / 'replication.log'

    result = ''
    found_required_message = False
    found_required_line = ''
    t0 = time.time()
    for i in range(0,max_allowed_time_for_wait):

        time.sleep(1)

        with open(replication_log, 'r') as f:
            diff_data = unified_diff(
                replold_lines,
                f.readlines()
              )

        for k,d in enumerate(diff_data):
            if pattern_to_check.search(d):
                found_required_message = True
                break

        if found_required_message:
            break
    t1 = time.time()
    if not consider_found_as_unexpected and not found_required_message or consider_found_as_unexpected and found_required_message:
        # ACHTUNG! This looks weird but we have to either re-read replication log now or wait at least <JOURNAL_ARCHIVE_TIMEOUT> seconds
        # if we want to see FULL (actual) content of this log! Otherwise last part of log will be missed. I have no explanations for that :(
        repllog_diff = ''
        with open(replication_log, 'r') as f:
            diff_data = unified_diff(
                replold_lines,
                f.readlines()
              )
            repllog_diff = '\n'.join( ( ('%4d ' %i) + r.rstrip() for i,r in enumerate(diff_data) ) )

        if consider_found_as_unexpected:
            unexp_msg = f"UNEXPECTED outcome: pattern '{pattern_to_check.pattern}' must not occur in log but was ENCOUNTERED there for {int(t1-t0)} seconds."
        else:
            unexp_msg = f"MISSED outcome: pattern '{pattern_to_check.pattern}' was NOT FOUND for {int(t1-t0)} seconds."

        result = '\n'.join( (unexp_msg, 'replication log diff:', repllog_diff) )

    return result

#--------------------------------------------

def watch_replica( a: Action, max_allowed_time_for_wait, ddl_ready_query = '', isql_check_script = '', replica_expected_out = ''):

    retcode = 1;
    ready_to_check = False
    if ddl_ready_query:
        with a.db.connect(no_db_triggers = True) as con:
            with con.cursor() as cur:
                for i in range(0,max_allowed_time_for_wait):
                    cur.execute(ddl_ready_query)
                    count_actual = cur.fetchone()
                    if count_actual:
                        ready_to_check = True
                        break
                    else:
                        con.rollback()
                        time.sleep(1)
    else:
        ready_to_check = True

    if not ready_to_check:
        print( f'UNEXPECTED. Query to verify DDL completion did not return any rows for {max_allowed_time_for_wait} seconds.' )
        print('Query:')
        print(ddl_ready_query)

        return
    
    final_check_pass = False
    if isql_check_script:
        retcode = 0
        for i in range(max_allowed_time_for_wait):
            a.reset()
            a.expected_stdout = replica_expected_out
            a.isql(switches=['-q', '-nod'], input = isql_check_script, combine_output = True)

            if a.return_code:
                # "Token unknown", "Name longer than database column size" etc: we have to
                # immediately break from this loop because isql_check_script is incorrect!
                break
            
            if a.clean_stdout == a.clean_expected_stdout:
                final_check_pass = True
                break
            if i < max_allowed_time_for_wait-1:
                time.sleep(1)

        if not final_check_pass:
            print(f'UNEXPECTED. Final check query did not return expected dataset for {max_allowed_time_for_wait} seconds.')
            print('Final check query:')
            print(isql_check_script)
            print('Expected output:')
            print(a.clean_expected_stdout)
            print('Actual output:')
            print(a.clean_stdout)
            print(f'ISQL return_code={a.return_code}')
            print(f'Waited for {i} seconds')

        a.reset()

    else:
        final_check_pass = True

    return

#--------------------------------------------

def drop_db_objects(act_db_main: Action,  act_db_repl: Action, capsys):

    # return initial state of master DB:
    # remove all DB objects (tables, views, ...):
    #
    db_main_meta, db_repl_meta = '', ''
    for a in (act_db_main,act_db_repl):
        if a == act_db_main:
            sql_clean = (a.files_dir / 'drop-all-db-objects.sql').read_text()
            a.expected_stdout = """
                Start removing objects
                Finish. Total objects removed
            """
            a.isql(switches=['-q', '-nod'], input = sql_clean, combine_output = True)

            if a.clean_stdout == a.clean_expected_stdout:
                a.reset()
            else:
                print(a.clean_expected_stdout)
                a.reset()
                break

            # NB: one need to remember that rdb$system_flag can be NOT ONLY 1 for system used objects!
            # For example, it has value =3 for triggers that are created to provide CHECK-constraints,
            # Custom DB objects always have rdb$system_flag = 0 (or null for some very old databases).
            # We can be sure that there are no custom DB objects if following query result is NON empty:
            #
            ddl_ready_query = """
                select 1
                from rdb$database
                where NOT exists (
                    select custom_db_object_flag
                    from (
                        select rt.rdb$system_flag as custom_db_object_flag from rdb$triggers rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$relations rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$functions rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$procedures rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$exceptions rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$fields rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$collations rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$generators rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$roles rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$auth_mapping rt
                        UNION ALL
                        select 1 from sec$users s
                        where upper(s.sec$user_name) <> 'SYSDBA'
                    ) t
                    where coalesce(t.custom_db_object_flag,0) = 0
                )
            """


            ##############################################################################
            ###  W A I T   U N T I L    R E P L I C A    B E C O M E S   A C T U A L   ###
            ##############################################################################
            watch_replica( act_db_repl, MAX_TIME_FOR_WAIT_DATA_IN_REPLICA, ddl_ready_query)

            # Must be EMPTY:
            print(capsys.readouterr().out)

            db_main_meta = a.extract_meta(charset = 'utf8', io_enc = 'utf8')
        else:
            db_repl_meta = a.extract_meta(charset = 'utf8', io_enc = 'utf8')

        if RUN_SWEEP_AT_END:
            # Following sweep was mandatory during 2021...2022. Problem was fixed:
            # * for FB 4.x: 26-jan-2023, commit 2ed48a62c60c029cd8cb2b0c914f23e1cb56580a
            # * for FB 5.x: 20-apr-2023, commit 5af209a952bd2ec3723d2c788f2defa6b740ff69
            # (log message: 'Avoid random generation of field IDs, respect the user-specified order instead').
            # Until this problem was solved, subsequent runs of this test caused to fail with:
            # 'ERROR: Record format with length NN is not found for table TEST'
            #
            a.gfix(switches=['-sweep', a.db.dsn])

    # Final point: metadata must become equal:
    #
    diff_meta = ''.join(unified_diff( \
                         [x for x in db_main_meta.splitlines() if 'CREATE DATABASE' not in x],
                         [x for x in db_repl_meta.splitlines() if 'CREATE DATABASE' not in x])
                       )
    # Must be EMPTY:
    print(diff_meta)

#--------------------------------------------

def get_repl_log(act_db_main: Action):
    replication_log = act_db_main.home_dir / 'replication.log'
    rlog_lines = []
    with open(replication_log, 'r') as f:
        rlog_lines = f.readlines()

    return rlog_lines

#--------------------------------------------

@pytest.mark.replication
@pytest.mark.version('>=4.0.6')
def test_1(act_db_main: Action,  act_db_repl: Action, db_nbk0: Path, capsys):

    FLD_WIDTH = 500
    ADD_ROWS_INIT = 100
    ADD_ROWS_IN_OAT = 30000
    TEST2_VALUE = -2147483648

    # Map for storing mnemonas and details for every FAILED step:
    run_errors_map = {}

    # Obtain full path + filename for DB_MAIN and DB_REPL aliases.
    # NOTE: we must NOT use 'a.db.db_path' for ALIASED databases!
    # It will return '.' rather than full path+filename.
    # Use only con.info.name for that!
    #
    db_info = {}
    for a in (act_db_main, act_db_repl):
        with a.db.connect() as con:
            db_info[a,  'db_full_path'] = con.info.name

    sql_init = f"""
        set bail on;
        set wng off;
        recreate table test (
            id int generated by default as identity constraint test_pk primary key
            ,dts timestamp default 'now'
            ,trn bigint default current_transaction
            ,s varchar({FLD_WIDTH}) unique using index test_f01_unq
        );

        recreate table test2 (
            id int generated by default as identity constraint test2_pk primary key
            ,dts timestamp default 'now'
            ,trn bigint default current_transaction
            ,s varchar({FLD_WIDTH}) unique using index test2_f01_unq
        );
        commit;

        insert into test(s) select lpad( '', {FLD_WIDTH}, uuid_to_char(gen_uuid()) ) from rdb$types rows {ADD_ROWS_INIT};
        commit;

        recreate table t_completed(id int primary key);
        commit;
    """
    act_db_main.isql(switches=['-q'], input = sql_init, combine_output = True)
    run_errors_map['out_prep_ddl'] = act_db_main.clean_stdout
    act_db_main.reset()

    if max(v.strip() for v in run_errors_map.values()):
        # Some problem raised during init_sql execution
        pass
    else:
        # Query to be used for check that all DB objects present in replica (after last DML statement completed on master DB):
        ddl_ready_query = "select 1 from rdb$relations where rdb$relation_name = upper('t_completed')"
        #####################################################################
        ###  WAIT UNTIL REPLICA GET SEGMENT(S) WITH INITIAL DDL AND DATA  ###
        #####################################################################
        watch_replica( act_db_repl, MAX_TIME_FOR_WAIT_DATA_IN_REPLICA, ddl_ready_query)

        # Must be EMPTY:
        run_errors_map['out_repl_ddl'] = capsys.readouterr().out

    if max(v.strip() for v in run_errors_map.values()):
        # Some problem raised with delivering DDL changes to replica
        pass
    else:

        with act_db_main.db.connect() as con:
            preserved_segment_no = -1

            tp_oat = tpb(isolation = Isolation.SNAPSHOT, lock_timeout = 0)
            tx_oat = con.transaction_manager(tp_oat)
            cur_oat = tx_oat.cursor()

            tx_oat.begin()
            replold_lines = get_repl_log(act_db_main)

            try:
                cur_oat.execute(f"insert /* trace_tag OAT */ into test(s) select lpad('', {FLD_WIDTH}, uuid_to_char(gen_uuid())) from rdb$types,rdb$types rows {ADD_ROWS_IN_OAT}")
                cur_oat.execute('select max(id) from test')
                cur_oat.fetchone()
            except DatabaseError as e:
                run_errors_map['main_oat_init_err'] = e.__str__()
        
            if max(v.strip() for v in run_errors_map.values()):
                # Some problem was in just executed OAT-statement
                pass
            else:
                ##############################################################
                ###  CHECK REPL.LOG: WAITING FOR "PRESERVING OAT" MESSAGE  ###
                ##############################################################
                # VERBOSE: Segment 2 (11534562 bytes) is replicated in 0.934s, preserving (OAT: 10 in segment 2)
                pattern_to_check = re.compile( r'preserving \(oat:\s+\d+\s+in\s+segment\s+\d+', re.IGNORECASE )
                run_errors_map['repl_preserve_oat_err'] = watch_repl_log_pattern( act_db_main, pattern_to_check, replold_lines, MAX_TIME_FOR_WAIT_SEGMENT_IN_LOG)

            if max(v.strip() for v in run_errors_map.values()):
                # Timeout expired: message "Segment ... preserving (OAT: ... in segment ...)" did not appeared
                # in replication log for <MAX_TIME_FOR_WAIT_SEGMENT_IN_LOG> seconds.
                pass
            else:

                ###############################################
                ###  MASTER: RUN DML BEFORE 'NBACKUP -B 0'  ###
                ###############################################
                # tp_wrk = tpb(isolation = Isolation.READ_COMMITTED_READ_CONSISTENCY, lock_timeout = 3) # ?!?! check trace  !!!
                tp_bef = tpb(isolation = Isolation.READ_COMMITTED, lock_timeout = 3)
                tx_bef = con.transaction_manager(tp_bef)
                cur_bef = tx_bef.cursor()
                try:
                    tx_bef.begin()
                    cur_bef.execute(f"insert /* trace_tag before creating nbk-0 */ into test2(id) values(?) returning rdb$get_context('SYSTEM', 'REPLICATION_SEQUENCE')", (TEST2_VALUE,))

                    # If returned value of rdb$get_context('SYSTEM', 'REPLICATION_SEQUENCE') is <N> then
                    # message in replication log contains this number PLUS 1, e.g. (for REPLICATION_SEQUENCE = 1):
                    # "Segment 2 (11534562 bytes) is replicated in 0.934s, preserving (OAT: 10 in segment 2)"
                    # We store this result in order to check further content of replication log that this
                    # segment was eventuially deleted (this will mean SUCCESSFUL finish of test):
                    #
                    preserved_segment_no = int(cur_bef.fetchone()[0]) + 1
                    tx_bef.commit()
                except DatabaseError as e:
                    run_errors_map['main_dml_afte_nbk0_err'] = e.__str__()
            
            if max(v.strip() for v in run_errors_map.values()):
                # Some problem was in just executed statement
                pass
            else:
                ##############################################
                ###  WAIT UNTIL REPLICA DB BECOMES ACTUAL  ###
                ##############################################
                # ( a: Action, max_allowed_time_for_wait, ddl_ready_query = '', isql_check_script = '', replica_expected_out = ''):
                chk_bef_sql = f'set heading off;select count(*) from test2 where id = {TEST2_VALUE};'
                watch_replica( act_db_repl, MAX_TIME_FOR_WAIT_DATA_IN_REPLICA, ddl_ready_query = '', isql_check_script = chk_bef_sql, replica_expected_out = '1' )

                # Must be EMPTY:
                run_errors_map['repl_chk_addi_data_err'] = capsys.readouterr().out

            if max(v.strip() for v in run_errors_map.values()):
                # Timeout expired: expected data did not appear in replica DB for <MAX_TIME_FOR_WAIT_DATA_IN_REPLICA> seconds.
                pass
            else:
                ################################################
                ###  REPLICA: CHANGE STATE TO FULL SHUTDOWN  ###
                ################################################
                act_db_repl.gfix(switches=['-shut', 'full', '-force', '0', act_db_repl.db.dsn], io_enc = locale.getpreferredencoding(), combine_output = True )
                run_errors_map['repl_full_shutdown_err'] = act_db_repl.stdout
                act_db_repl.reset()
            
            if max(v.strip() for v in run_errors_map.values()):
                pass
            else:
                ##########################################
                ###  M A S T E R:    M A K E   N B K 0  ##
                ##########################################
                # DO NOT use 'combine_output = True' here: nbackup produces non-empty output when successfully completes.
                act_db_main.nbackup( switches = ['-b', '0', act_db_main.db.dsn, db_nbk0], io_enc = locale.getpreferredencoding(), combine_output = False )
                run_errors_map['main_create_nbk0_err'] = act_db_main.stderr
                act_db_main.reset()

            if max(v.strip() for v in run_errors_map.values()):
                # Some errors occurres during 'nbackup -b 0 db_main_alias ...'
                pass
            else:
                ##############################################################################################
                ###   NBK0: FIXUP, SET AS REPLICA, CHANGE STATE TO FULL SHUTDOWN,  MOVE TO OLD  'DB_REPL'  ###
                ##############################################################################################
                try:
                    act_db_repl.nbackup(switches = ['-SEQ', '-F', str(db_nbk0)], combine_output = True, io_enc = locale.getpreferredencoding())
                    # act_db_main.svcmgr(switches=['action_nfix', 'dbname', str(db_nbk0)], io_enc = locale.getpreferredencoding())
                    with act_db_main.connect_server() as srv:
                        #srv.database.nfix_database(database = db_nbk0) --> "Internal error when using clumplet API: attempt to store data in dataless clumplet" // see also: core_5085_test.py
                        srv.database.set_replica_mode(database = db_nbk0, mode = ReplicaMode.READ_ONLY)
                        srv.database.shutdown(database = db_nbk0, mode = ShutdownMode.FULL, method = ShutdownMethod.FORCED, timeout = 0)
                except DatabaseError as e:
                    run_errors_map['nbk0_make_new_replica_err'] = e.__str__()

            if max(v.strip() for v in run_errors_map.values()):
                pass
            else:

                ##################################
                ###   OVERWRITE  OLD  REPLICA  ###
                ##################################
                shutil.move(db_nbk0, db_info[act_db_repl,'db_full_path'])
     
                ##################################
                ###   REPLICA:  BRING  ONLINE  ###
                ##################################
                act_db_repl.gfix(switches=['-online', act_db_repl.db.dsn], io_enc = locale.getpreferredencoding(), combine_output = True )
                run_errors_map['repl_bring_online_err']  = act_db_repl.stdout
                act_db_repl.reset()

            replold_lines = get_repl_log(act_db_main)
            if max(v.strip() for v in run_errors_map.values()):
                pass
            else:
                ##############################################
                ###  MASTER: FINAL ACTION WITHIN TX = OAT  ###
                ##############################################
                try:
                    tx_oat.commit()
                except DatabaseError as e:
                    run_errors_map['main_oat_fini_err'] = e.__str__()

            if max(v.strip() for v in run_errors_map.values()):
                pass
            else:

                ############################################################################
                ###  CHECK REPL.LOG: WAITING FOR "Transaction <N> is not found" MESSAGE  ###
                ############################################################################
                # Before fix: "ERROR: Transaction 13 is not found" appeared in the repl log at thios point.
                pattern_to_check = re.compile( r'Error:\s+Transaction\s+\d+\s+(is\s+)?not\s+found', re.IGNORECASE )
                run_errors_map['repl_tx_not_found_err'] = watch_repl_log_pattern( act_db_main, pattern_to_check, replold_lines, MAX_TIME_FOR_WAIT_SEGMENT_IN_LOG, consider_found_as_unexpected = True)

            if max(v.strip() for v in run_errors_map.values()):
                pass
            else:
                ##############################################
                ###  WAIT UNTIL REPLICA DB BECOMES ACTUAL  ###
                ##############################################
                # ( a: Action, max_allowed_time_for_wait, ddl_ready_query = '', isql_check_script = '', replica_expected_out = ''):
                chk_oat_sql = 'set heading off;select max(id) from test;'
                watch_replica( act_db_repl, MAX_TIME_FOR_WAIT_DATA_IN_REPLICA, ddl_ready_query = '', isql_check_script = chk_oat_sql, replica_expected_out = str(ADD_ROWS_INIT+ADD_ROWS_IN_OAT) )
                # Must be EMPTY:
                run_errors_map['repl_chk_fini_err'] = capsys.readouterr().out

            if max(v.strip() for v in run_errors_map.values()):
                pass
            else:
                #######################################################################################
                ###  CHECK REPL.LOG: WAITING FOR "DELETING SEGMENT {} AS NO LONGER NEEDED" MESSAGE  ###
                #######################################################################################
                # VERBOSE: Deleting segment 2 as no longer needed
                # NB: before fix message was: "VERBOSE: Deleting segment 2 due to fast forward"
                pattern_to_check = re.compile( r'VERBOSE:\s+Deleting\s+segment\s+%d\s+as\s+no\s+longer\s+needed' % preserved_segment_no, re.IGNORECASE )
                run_errors_map['repl_deleted_oat_segm'] = watch_repl_log_pattern( act_db_main, pattern_to_check, replold_lines, MAX_TIME_FOR_WAIT_SEGMENT_IN_LOG)

    if max(v.strip() for v in run_errors_map.values()):
        # We had a problem in some of previous steps.
        # First, we have to RECREATE both master and slave databases
        # (otherwise further execution of this test or other replication-related tests most likely will fail):
        run_errors_map['out_reset'] = reset_replication(act_db_main, act_db_repl, db_info[act_db_main,'db_full_path'], db_info[act_db_repl,'db_full_path'])
    else:
        drop_db_objects(act_db_main, act_db_repl, capsys)
        # Must be EMPTY:
        run_errors_map['out_drop'] = capsys.readouterr().out

    if max(v.strip() for v in run_errors_map.values()):
        print(f'Problem(s) detected, check run_errors_map:')
        for k,v in run_errors_map.items():
            if v.strip():
                print(k,':')
                print(v.strip())
                print('-' * 40)
    
    assert '' == capsys.readouterr().out
